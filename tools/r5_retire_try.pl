#!/usr/bin/env perl
#----------------------------------------------------------------------
# r5_retire_try.pl - retire the rethrowing __BEGIN_TRY / __END_CATCH pairs
# (src/Core/Exception.h) from the files it is given, and prove a diff did
# nothing else.
#
# __END_CATCH only pushes the enclosing function onto the Throwable's
# stack trace and rethrows, so a pair can go without changing what the
# code does, as long as the try block's scope survives:
#
#   * a wrapper site, where __BEGIN_TRY is the first token after a block's
#     `{` and its __END_CATCH the last before that block's `}`, loses both
#     macros: the try block was the whole block, so nothing changes scope
#     but the names it declares, which now share the block with a
#     function's parameters or an if/for/catch's declarations. A clash is a
#     compile error, and --brace keeps such a site as braces instead;
#   * every other site becomes a plain block: __BEGIN_TRY turns into `{` and
#     __END_CATCH into `}`, so every lifetime in it, locks included, ends
#     where it ended before.
#
# Pairs are matched by brace structure, never by line: `{`, `}` and every
# macro that opens or closes a brace (the Exception.h macros, the profile
# macros, and any macro whose definition anywhere under src/ has unbalanced
# braces or names one of those) are pushed and popped on one stack, and a
# file whose stack does not match up is refused whole. So is a file where
# an #if group opens or closes a brace its other branches might not. A
# pair that closes with __END_CATCH_NO_RETHROW swallows exceptions, which
# is control flow: it is left alone, as are __BEGIN_DEBUG / __END_DEBUG
# and their _EX forms. A site written inside a #define is reported and
# left alone. Comments, string and character literals and preprocessor
# lines are tokens of their own, so a macro name inside one is never a
# site.
#
# Usage (from the repository root, whose src/ holds the macro table):
#   perl tools/r5_retire_try.pl [--brace FILE:LINE]... FILE...
#       rewrite the files in place; --brace keeps the wrapper site whose
#       __BEGIN_TRY is on that line of FILE (spelled as on the command
#       line) as braces, and one that names no pair writes nothing.
#       Prints one line per changed file and a SKIP or REFUSE line for
#       each site or file it leaves alone.
#   perl tools/r5_retire_try.pl --check REV | REV..REV | REV...REV
#       prove that every C++ file the range modifies differs from its old
#       version only by rethrowing pairs that were removed (wrapper sites
#       only) or turned into braces; REV alone compares with the working
#       tree, REV...REV starts from the merge base. Other files are
#       reported and skipped; a git error, an added or deleted C++ file, a
#       failing file or a range that modifies no C++ file exits non-zero.
#   perl tools/r5_retire_try.pl --self-test
#
# Perl, like tests/tools/.
#----------------------------------------------------------------------
use strict;
use warnings;
use Cwd qw(getcwd);
use File::Find;
use File::Temp qw(tempdir);

my %OPEN = (
    '__BEGIN_TRY' => 'TRY',
    '__BEGIN_DEBUG' => 'DEBUG',
    '__BEGIN_DEBUG_EX' => 'DEBUG_EX',
    '__ENTER_CRITICAL_SECTION' => 'CS',
    'beginProfileEx' => 'PROFILE',
);
my %CLOSE = (
    '__END_CATCH' => 'TRY',
    '__END_CATCH_NO_RETHROW' => 'TRY',
    '__END_DEBUG' => 'DEBUG',
    '__END_DEBUG_EX' => 'DEBUG_EX',
    '__LEAVE_CRITICAL_SECTION' => 'CS',
    'endProfileEx' => 'PROFILE',
);

#----------------------------------------------------------------------
# tokenizer: [type, text, start, end, line]; type is c (comment), p
# (preprocessor line, continuations included), s (string or character
# literal), w (word or number), o (punctuation). start and end are byte
# offsets into the text, line the 1-based line the token starts on.
#----------------------------------------------------------------------
my $PUNCT = qr/->\*|->|\+\+|--|<=>|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||::|\.\.\.|\.\*|[-+*\/%&|^!=]=|##|[^\s\w]/;

# $inDirective: the text is a directive's own body, where a `#` is an
# operator, not the start of another directive.
sub tokenize {
    my ($text, $inDirective) = @_;
    my @tokens;
    my $bol = 1;    # only whitespace since the last newline
    my $line = 1;
    pos($text) = 0;
    while (pos($text) < length $text) {
        my $start = pos($text);
        if ($text =~ /\G[ \t\r\f\x0B]+/gc) { next; }
        if ($text =~ /\G\n/gc) { $bol = 1; $line++; next; }
        my $type;
        if ($bol && !$inDirective && $text =~ /\G#/gc) {
            # A directive runs to the first newline outside a comment or a
            # literal that no backslash continues.
            while (1) {
                if ($text =~ /\G\/\*.*?\*\//gcs) { next; }
                if ($text =~ /\G\/\/[^\n]*/gc) { next; }
                if ($text =~ /\G"(?:\\.|[^"\\\n])*"/gc) { next; }
                if ($text =~ /\G'(?:\\.|[^'\\\n])*'/gc) { next; }
                if ($text =~ /\G\\\r?\n/gc) { next; }
                if ($text =~ /\G[^\n\/"'\\]+/gc) { next; }
                if ($text =~ /\G[\/"'\\]/gc) { next; }
                last;
            }
            $type = 'p';
        } elsif ($text =~ /\G\/\/(?:[^\n\\]|\\.)*/gcs) {
            $type = 'c';
        } elsif ($text =~ /\G\/\*.*?\*\//gcs) {
            $type = 'c';
        } elsif ($text =~ /\G(?:u8|[uUL])?R"([^()\\\s]{0,16})\(/gc) {
            my $close = ")$1\"";
            my $end = index($text, $close, pos($text));
            die "unterminated raw string at offset $start\n" if $end < 0;
            pos($text) = $end + length $close;
            $type = 's';
        } elsif ($text =~ /\G(?:u8|[uUL])?"(?:\\.|[^"\\\n])*"/gcs) {
            $type = 's';
        } elsif ($text =~ /\G(?:u8|[uUL])?'(?:\\.|[^'\\\n])*'/gcs) {
            $type = 's';
        } elsif ($text =~ /\G\.?\d(?:[eEpP][-+]|[\w.'])*/gc) {
            $type = 'w';
        } elsif ($text =~ /\G[A-Za-z_]\w*/gc) {
            $type = 'w';
        } elsif ($text =~ /\G$PUNCT/gc) {
            $type = 'o';
        } else {
            die "cannot tokenize at offset $start\n";
        }
        $bol = 0;
        my $end = pos($text);
        my $t = substr($text, $start, $end - $start);
        push @tokens, [$type, $t, $start, $end, $line];
        $line += ($t =~ tr/\n//);
    }
    return \@tokens;
}

# The text a token is compared by. A comment's whitespace runs collapse to
# one space, so re-indenting it is not a change; a directive is compared as
# its own tokens, so its literals stay verbatim and a line splice (`\`)
# stays a token; everything else is verbatim.
sub norm {
    my ($t) = @_;
    if ($t->[0] eq 'p') {
        # The one blank that matters in a directive: `#define M(a)` is
        # function-like, `#define M (a)` is not.
        my $function = $t->[1] =~ /^#\s*define\s+\w+\(/ ? "\0(function-like)" : '';
        return join("\0", '#', map { norm($_) } @{ tokenize(substr($t->[1], 1), 1) }) . $function;
    }
    return $t->[1] unless $t->[0] eq 'c';
    (my $s = $t->[1]) =~ s/\s+/ /g;
    $s =~ s/^\s+|\s+$//g;
    return $s;
}

#----------------------------------------------------------------------
# Macros that open or close a brace: the fixed table above, plus every
# #define under src/ whose body has unbalanced braces (an unknown kind) or
# names a macro of a known kind.
#----------------------------------------------------------------------
my %braceMacro;    # name => kind for openers, '/'.kind for closers, '?' unknown

sub classifyDefine {
    my ($directive, $into) = @_;
    my $body = $directive;
    $body =~ s/\\\r?\n/ /g;
    return unless $body =~ /^#\s*define\s+(\w+)(\([^)]*\))?(.*)$/s;
    my ($name, $rest) = ($1, $3);
    return if $OPEN{$name} || $CLOSE{$name};
    # What the body leaves open or closes once its own pairs are matched.
    my (@open, @closed);
    for my $t (@{ tokenize($rest, 1) }) {
        next unless $t->[0] eq 'w' || $t->[0] eq 'o';
        my $k = $t->[1] eq '{' ? '{' : $t->[1] eq '}' ? '/{' : $OPEN{$t->[1]} || ($CLOSE{$t->[1]} ? '/' . $CLOSE{$t->[1]} : undef);
        next unless defined $k;
        if ($k !~ m{^/}) { push @open, $k; }
        elsif (@open && '/' . $open[-1] eq $k) { pop @open; }
        else { push @closed, $k; }
    }
    return unless @open || @closed;
    my $kind = @open + @closed > 1 ? '?' : @open ? $open[0] : $closed[0];
    $kind = '?' if $kind eq '{' || $kind eq '/{';
    # One name defined two ways (a profile switch and its no-op) keeps the
    # brace-bearing meaning; two different ones are unknown.
    my $old = $into->{$name};
    $into->{$name} = (!defined $old || $old eq $kind) ? $kind : '?';
}

sub loadBraceMacros {
    die "run from the repository root: src/Core/Exception.h is not here\n" unless -f 'src/Core/Exception.h';
    find({ no_chdir => 1, wanted => sub {
        return unless /\.(?:h|hpp|cpp)$/ && -f $_;
        my $text = slurp($_);
        while ($text =~ /^[ \t]*#[ \t]*define\b(?:[^\n]*\\\r?\n)*[^\n]*/mg) {
            classifyDefine($&, \%braceMacro);
        }
    } }, 'src');
    return \%braceMacro;
}

sub slurp {
    my ($path) = @_;
    open(my $fh, '<:raw', $path) or die "$path: $!\n";
    local $/;
    my $text = <$fh>;
    close $fh;
    return $text;
}

#----------------------------------------------------------------------
# analyse(text) -> { tokens, pairs => [...], refused => reason, skipped => [...] }
# Each pair: { begin, end, closer, wrapper } with token indices.
#----------------------------------------------------------------------
sub analyse {
    my ($text, $macros) = @_;
    my $toks = tokenize($text);
    my %local = %$macros;
    for my $t (@$toks) { classifyDefine($t->[1], \%local) if $t->[0] eq 'p'; }

    my (@stack, @pairs, @skipped, @groups);
    my $swallow = 0;
    my @code;    # indices of code tokens (w, o, s)
    my $prevCode = -1;
    for my $i (0 .. $#$toks) {
        my ($type, $text_) = @{ $toks->[$i] };
        if ($type eq 'p') {
            if ($text_ =~ /^#\s*if/) {
                push @groups, [scalar @stack, $i];
            } elsif ($text_ =~ /^#\s*(?:else|elif)/ || $text_ =~ /^#\s*endif/) {
                return { tokens => $toks, refused => 'unmatched ' . (split /\s/, $text_)[0] } unless @groups;
                return { tokens => $toks, refused => "a brace crosses the #if group at line $toks->[ $groups[-1][1] ][4]" }
                    if @stack != $groups[-1][0];
                pop @groups if $text_ =~ /^#\s*endif/;
            }
            for (1 .. scalar(() = $text_ =~ /\b__BEGIN_TRY\b/g)) {
                push @skipped, [$toks->[$i][4], 'inside a preprocessor directive'];
            }
            next;
        }
        next if $type eq 'c';
        push @code, $i;
        my $kind = $type eq 'o' && $text_ eq '{' ? '{'
                 : $type eq 'o' && $text_ eq '}' ? '/{'
                 : $type eq 'w' && $OPEN{$text_} ? $OPEN{$text_}
                 : $type eq 'w' && $CLOSE{$text_} ? '/' . $CLOSE{$text_}
                 : $type eq 'w' && $local{$text_} ? $local{$text_}
                 : undef;
        next unless defined $kind;
        my $line = $toks->[$i][4];
        return { tokens => $toks, refused => "macro $text_ at line $line opens or closes braces in a way this tool does not know" }
            if $kind eq '?';
        if ($kind !~ m{^/}) {
            push @stack, [$kind, $i, scalar @code - 1];
            next;
        }
        return { tokens => $toks, refused => "$text_ at line $line closes nothing" } unless @stack;
        return { tokens => $toks, refused => "the #if group at line $toks->[ $groups[-1][1] ][4] closes a brace it did not open" }
            if @groups && @stack == $groups[-1][0];
        my $open = pop @stack;
        return { tokens => $toks, refused => "$text_ at line $line closes the $toks->[ $open->[1] ][1] at line $toks->[ $open->[1] ][4]" }
            if '/' . $open->[0] ne $kind;
        next unless $open->[0] eq 'TRY' && $toks->[ $open->[1] ][1] eq '__BEGIN_TRY';
        $swallow++ if $text_ eq '__END_CATCH_NO_RETHROW';
        next unless $text_ eq '__END_CATCH';
        # wrapper: the code token before __BEGIN_TRY is a `{` and the one
        # after __END_CATCH is a `}` (which then closes that same `{`).
        my $b = $open->[2];
        my $wrapper = $b > 0 && $toks->[ $code[ $b - 1 ] ][1] eq '{' && $toks->[ $code[ $b - 1 ] ][0] eq 'o';
        push @pairs, { begin => $open->[1], end => $i, wrapper => $wrapper, endCode => scalar @code - 1 };
    }
    return { tokens => $toks, refused => "unclosed $toks->[ $stack[-1][1] ][1] at line $toks->[ $stack[-1][1] ][4]" } if @stack;
    return { tokens => $toks, refused => 'unclosed #if' } if @groups;
    # The token after __END_CATCH is known only once the walk is past it.
    for my $p (@pairs) {
        my $next = $code[ $p->{endCode} + 1 ];
        $p->{wrapper} &&= defined $next && $toks->[$next][1] eq '}' && $toks->[$next][0] eq 'o';
    }
    return { tokens => $toks, pairs => \@pairs, skipped => \@skipped, swallow => $swallow };
}

#----------------------------------------------------------------------
# rewrite(text, macros, braceLines)
#     -> (newText, removed, braced, skipped, refused, swallowing, pairLines)
#----------------------------------------------------------------------
sub rewrite {
    my ($text, $macros, $braceLines) = @_;
    my $an = analyse($text, $macros);
    return ($text, 0, 0, [], $an->{refused}) if $an->{refused};
    my $toks = $an->{tokens};
    my @edits;    # [start, end, replacement]
    my ($removed, $braced) = (0, 0);
    for my $p (@{ $an->{pairs} }) {
        my $line = $toks->[ $p->{begin} ][4];
        if ($p->{wrapper} && !$braceLines->{$line}) {
            push @edits, [ @{ $toks->[ $p->{begin} ] }[2, 3], '' ], [ @{ $toks->[ $p->{end} ] }[2, 3], '' ];
            $removed++;
        } else {
            push @edits, [ @{ $toks->[ $p->{begin} ] }[2, 3], '{' ], [ @{ $toks->[ $p->{end} ] }[2, 3], '}' ];
            $braced++;
        }
    }
    my ($out, $cursor) = ('', 0);
    my %touched;    # output offset of each edit => whether it removed a macro
    for my $e (sort { $a->[0] <=> $b->[0] } @edits) {
        my ($s, $t, $r) = @$e;
        if ($r eq '') {
            # Only the blanks beside the macro go, never text inside a
            # literal or comment: at the end of a line both sides' blanks
            # ("x __END_CATCH" -> "x"), elsewhere the right side's when the
            # left side has some ("x __BEGIN_TRY y" -> "x y").
            my ($before, $after) = ($s, $t);
            $before-- while $before > $cursor && substr($text, $before - 1, 1) =~ /[ \t]/;
            $after++ while $after < length $text && substr($text, $after, 1) =~ /[ \t]/;
            if ($after == length $text || substr($text, $after, 1) =~ /[\r\n]/) { ($s, $t) = ($before, $after); }
            elsif ($before < $s) { $t = $after; }
        }
        $out .= substr($text, $cursor, $s - $cursor);
        $touched{ length $out } = $r eq '';
        $out .= $r;
        $cursor = $t;
    }
    $out .= substr($text, $cursor);
    # A line a removal left blank goes, with the blank lines on the inside of
    # the block it opened or closed.
    my @lines = split /(?<=\n)/, $out;
    my $offset = 0;
    my @starts = sort { $a <=> $b } grep { $touched{$_} } keys %touched;
    my @kill;
    for my $i (0 .. $#lines) {
        my $len = length $lines[$i];
        while (@starts && $starts[0] < $offset + $len) {
            shift @starts;
            $kill[$i] = 1 if $lines[$i] =~ /^[ \t]*\r?\n?$/;
        }
        $offset += $len;
    }
    for my $i (0 .. $#lines) {
        next unless $kill[$i];
        my $j = $i + 1;    # blank lines below a removed opener
        while ($j <= $#lines && $lines[$j] =~ /^[ \t]*\r?\n$/ && !$kill[$j]) {
            last unless $i > 0 && $lines[ $i - 1 ] =~ /\{[ \t]*\r?\n$/;
            $kill[$j++] = 1;
        }
        $j = $i - 1;       # blank lines above a removed closer
        while ($j >= 0 && $lines[$j] =~ /^[ \t]*\r?\n$/ && !$kill[$j]) {
            last unless $i < $#lines && $lines[ $i + 1 ] =~ /^[ \t]*\}/;
            $kill[$j--] = 1;
        }
    }
    $out = join '', map { $kill[$_] ? () : $lines[$_] } 0 .. $#lines;
    my @pairLines = map { $toks->[ $_->{begin} ][4] } @{ $an->{pairs} };
    return ($out, $removed, $braced, $an->{skipped}, undef, $an->{swallow}, \@pairLines);
}

#----------------------------------------------------------------------
# check(oldText, newText, macros) -> undef on success, else the reason.
# Every token must match, except that each rethrowing pair of the old text
# may be kept, turned into `{` `}`, or (a wrapper site only) removed.
#----------------------------------------------------------------------
sub check {
    my ($oldText, $newText, $macros) = @_;
    my $an = analyse($oldText, $macros);
    my @old = map { norm($_) } @{ $an->{tokens} };
    my @new = map { norm($_) } @{ tokenize($newText) };
    if ($an->{refused}) {
        return "the old file is refused ($an->{refused}) yet changed" if "@old" ne "@new";
        return undef;
    }
    my (%beginOf, %endOf);
    for my $k (0 .. $#{ $an->{pairs} }) {
        $beginOf{ $an->{pairs}[$k]{begin} } = $k;
        $endOf{ $an->{pairs}[$k]{end} } = $k;
    }
    my @wrapper = map { $_->{wrapper} } @{ $an->{pairs} };
    my $far = 0;    # the furthest old token any attempt matched, for the report
    my $align;
    $align = sub {
        my ($i, $j, $fate) = @_;
        while ($i < @old) {
            $far = $i if $i > $far;
            if (defined(my $k = $beginOf{$i})) {
                my @options;
                push @options, ['keep', 1] if $j < @new && $new[$j] eq '__BEGIN_TRY';
                push @options, ['brace', 1] if $j < @new && $new[$j] eq '{';
                push @options, ['remove', 0] if $wrapper[$k];
                return 0 unless @options;
                if (@options == 1) {
                    $fate->{$k} = $options[0][0];
                    $i++;
                    $j += $options[0][1];
                    next;
                }
                for my $o (@options) {
                    return 1 if $align->($i + 1, $j + $o->[1], { %$fate, $k => $o->[0] });
                }
                return 0;
            }
            if (defined(my $k = $endOf{$i})) {
                my $f = $fate->{$k};
                if ($f eq 'remove') { $i++; next; }
                my $want = $f eq 'keep' ? '__END_CATCH' : '}';
                return 0 unless $j < @new && $new[$j] eq $want;
                $i++;
                $j++;
                next;
            }
            return 0 unless $j < @new && $new[$j] eq $old[$i];
            $i++;
            $j++;
        }
        return $j == @new;
    };
    return undef if $align->(0, 0, {});
    my $at = $an->{tokens}[$far] ? $an->{tokens}[$far][4] : 'end';
    return "differs by more than rethrowing pairs (old line $at)";
}

#----------------------------------------------------------------------
sub git {
    my @args = @_;
    open(my $fh, '-|', 'git', @args) or return ('', -1);
    local $/;
    my $out = <$fh>;
    close $fh;
    return ($out // '', $?);
}

# REV, REV..REV or REV...REV (from the two revisions' merge base) ->
# (from, to or undef for the working tree, symmetric); () when malformed.
sub parseRange {
    my ($range) = @_;
    if ($range =~ /^(.+?)(\.\.\.?)(.+)$/) {
        my ($from, $dots, $to) = ($1, $2, $3);
        return if $from =~ /\.$/ || $to =~ /^\./;
        return ($from, $to, $dots eq '...');
    }
    return if $range eq '' || $range =~ /\.\./;
    return ($range, undef, 0);
}

# runCheck(range, macros, git, readWorktree, say) -> exit status. Every
# modified C++ file must pass check(); other files are reported and
# skipped; a git error, an added or deleted C++ file, or a range that
# changes no C++ file at all fails.
sub runCheck {
    my ($range, $macros, $git, $read, $say) = @_;
    my ($from, $to, $symmetric) = parseRange($range);
    unless (defined $from) {
        $say->("check: '$range' is not REV, REV..REV or REV...REV\n");
        return 1;
    }
    if ($symmetric) {
        my ($base, $status) = $git->('merge-base', $from, $to);
        if ($status) { $say->("check: git merge-base $from $to failed\n"); return 1; }
        chomp($from = $base);
    }
    # -z: git quotes a path with a non-ASCII byte, a quote or a tab otherwise,
    # and the quoted name would fail the extension test below.
    my ($list, $status) = $git->('diff', '-z', '--name-status', '--no-renames', $from, defined $to ? $to : ());
    if ($status) { $say->("check: git diff $range failed\n"); return 1; }
    my ($files, $bad, $skipped) = (0, 0, 0);
    my @rows = split /\0/, $list;
    while (my ($change, $path) = splice @rows, 0, 2) {
        if ($path !~ /\.(?:cpp|h|hpp)$/) {
            $say->("SKIP $path: not a C++ source\n");
            $skipped++;
            next;
        }
        $files++;
        if ($change ne 'M') {
            $say->("FAIL $path: $change, not a modification\n");
            $bad++;
            next;
        }
        my ($old, $s1) = $git->('show', "$from:$path");
        my ($new, $s2) = defined $to ? $git->('show', "$to:$path") : ($read->($path), 0);
        my $why = $s1 || $s2 || !defined $new ? 'cannot read both versions' : check($old, $new, $macros);
        if ($why) { $say->("FAIL $path: $why\n"); $bad++; }
    }
    if (!$files) {
        $say->("check: $range changes no C++ source, so nothing was checked\n");
        return 1;
    }
    $say->("check: $files C++ file(s), $skipped other(s) skipped, "
        . ($bad ? "$bad FAILED" : 'all only retire rethrowing pairs') . "\n");
    return $bad ? 1 : 0;
}

# Every --brace must name a rethrowing pair's __BEGIN_TRY line in one of the
# files, or nothing is written.
sub runRewrite {
    my ($macros, $braces, @files) = @_;
    my %brace;    # path => { line => 1 }
    for my $b (@$braces) {
        die "--brace wants FILE:LINE, not '$b'\n" unless $b =~ /^(.+):(\d+)$/;
        $brace{$1}{$2} = 1;
    }
    my ($files, $removed, $braced, $skipped, $refused, $swallow) = (0, 0, 0, 0, 0, 0);
    my @writes;
    for my $path (@files) {
        my $text = slurp($path);
        my %lines = %{ $brace{$path} || {} };
        my ($out, $r, $b, $skips, $why, $sw, $pairLines) = rewrite($text, $macros, \%lines);
        delete $brace{$path}{$_} for @{ $pairLines || [] };
        $files++;
        if ($why) {
            my $n = () = $text =~ /\b__BEGIN_TRY\b/g;
            print "REFUSE $path: $why ($n __BEGIN_TRY left alone)\n";
            $refused++;
            next;
        }
        print "SKIP $path:$_->[0]: $_->[1]\n" for @$skips;
        $skipped += @$skips;
        $removed += $r;
        $braced += $b;
        $swallow += $sw;
        push @writes, [$path, $out, "$path: $r removed, $b braced\n"] if $out ne $text;
    }
    my @unmatched = map { my $p = $_; map { "$p:$_" } sort { $a <=> $b } keys %{ $brace{$p} } } sort keys %brace;
    if (@unmatched) {
        print "ERROR --brace $_ names no rethrowing pair's __BEGIN_TRY line; nothing written\n" for @unmatched;
        return 1;
    }
    for my $w (@writes) {
        open(my $fh, '>:raw', $w->[0]) or die "$w->[0]: $!\n";
        print $fh $w->[1];
        close $fh;
        print $w->[2];
    }
    print "total: $files file(s), $removed wrapper site(s) removed, $braced inner site(s) braced, "
        . "$swallow swallowing pair(s) left, $skipped site(s) skipped, $refused file(s) refused\n";
    return $refused || $skipped ? 1 : 0;
}

#----------------------------------------------------------------------
sub selfTest {
    my %none;
    my @cases = (
        # [input, expected output]
        ["void f(int a) {\n    __BEGIN_TRY\n\n    g(a);\n\n    __END_CATCH\n}\n",
         "void f(int a) {\n    g(a);\n}\n"],
        ["X::X() : m(1){__BEGIN_TRY __END_CATCH}\n", "X::X() : m(1){ }\n"],
        ["void f() {\n    a();\n    __BEGIN_TRY\n    b();\n    __END_CATCH\n}\n",
         "void f() {\n    a();\n    {\n    b();\n    }\n}\n"],
        ["void f() {\n    __BEGIN_TRY __BEGIN_DEBUG\n    b();\n    __END_DEBUG __END_CATCH\n}\n",
         "void f() {\n    __BEGIN_DEBUG\n    b();\n    __END_DEBUG\n}\n"],
        ["void f() {\n    __BEGIN_TRY\n    b();\n    __END_CATCH_NO_RETHROW\n}\n",
         "void f() {\n    __BEGIN_TRY\n    b();\n    __END_CATCH_NO_RETHROW\n}\n"],
        ["void f() {\n    __BEGIN_TRY\n    __BEGIN_TRY\n    b(\"}\"); // }\n    __END_CATCH\n    __END_CATCH\n}\n",
         "void f() {\n    {\n    b(\"}\"); // }\n    }\n}\n"],
        ["void f() {\n    __BEGIN_TRY\n    if (x) {\n        __BEGIN_TRY\n        y();\n        __END_CATCH\n    }\n    __END_CATCH_NO_RETHROW\n}\n",
         "void f() {\n    __BEGIN_TRY\n    if (x) {\n        y();\n    }\n    __END_CATCH_NO_RETHROW\n}\n"],
        ["void f() {\n    __BEGIN_TRY\n    __ENTER_CRITICAL_SECTION(m)\n    g();\n    __LEAVE_CRITICAL_SECTION(m)\n    __END_CATCH\n}\n",
         "void f() {\n    __ENTER_CRITICAL_SECTION(m)\n    g();\n    __LEAVE_CRITICAL_SECTION(m)\n}\n"],
        ["void f() {\n    __ENTER_CRITICAL_SECTION(m)\n    __BEGIN_TRY\n    g();\n    __END_CATCH\n    __LEAVE_CRITICAL_SECTION(m)\n}\n",
         "void f() {\n    __ENTER_CRITICAL_SECTION(m)\n    {\n    g();\n    }\n    __LEAVE_CRITICAL_SECTION(m)\n}\n"],
        ["// m\n  #define M \\\n    __BEGIN_TRY x(); __END_CATCH\nvoid f() { __BEGIN_TRY M __END_CATCH }\n",
         "// m\n  #define M \\\n    __BEGIN_TRY x(); __END_CATCH\nvoid f() { M }\n"],
        ["void f() {\n#if A\n    __BEGIN_TRY g(); __END_CATCH\n#endif\n}\n", "void f() {\n#if A\n    g();\n#endif\n}\n"],
        ["void f() {\r\n    __BEGIN_TRY\r\n\r\n    g();\r\n\r\n    __END_CATCH\r\n}\r\n", "void f() {\r\n    g();\r\n}\r\n"],
        ["// __BEGIN_TRY\nconst char* s = \"__END_CATCH\";\n", "// __BEGIN_TRY\nconst char* s = \"__END_CATCH\";\n"],
        ["void f() {\n#ifdef A\n    if (a) {\n#else\n    if (b) {\n#endif\n        __BEGIN_TRY g(); __END_CATCH\n    }\n}\n",
         undef],
        ["void f() {\n    __BEGIN_TRY\n    g();\n}\n", undef],
        ["#define OPEN(x) beginProfileEx(x);\nvoid f() {\n    OPEN(1)\n    __BEGIN_TRY\n    g();\n    __END_CATCH\n    endProfileEx(1);\n}\n",
         "#define OPEN(x) beginProfileEx(x);\nvoid f() {\n    OPEN(1)\n    {\n    g();\n    }\n    endProfileEx(1);\n}\n"],
        ["auto s = R\"x(__BEGIN_TRY })x\"; void f() { __BEGIN_TRY g(); __END_CATCH }\n",
         "auto s = R\"x(__BEGIN_TRY })x\"; void f() { g(); }\n"],
        # The blanks of a raw string that runs past the edited line stay.
        ["void f() {\n    __BEGIN_TRY auto s = R\"(abc   \ndef)\"; g(s);\n    __END_CATCH\n}\n",
         "void f() {\n    auto s = R\"(abc   \ndef)\"; g(s);\n}\n"],
    );
    my $failed = 0;
    for my $n (0 .. $#cases) {
        my ($in, $want) = @{ $cases[$n] };
        my ($out, undef, undef, undef, $why) = rewrite($in, \%none, {});
        if (!defined $want) {
            unless ($why) { print "case $n: expected a refusal, got:\n$out"; $failed++; }
            next;
        }
        if ($why || $out ne $want) {
            print "case $n: ", ($why ? "refused: $why\n" : "got:\n$out---- want:\n$want");
            $failed++;
            next;
        }
        my $c = check($in, $out, \%none);
        if ($c) { print "case $n: check rejected the rewrite: $c\n"; $failed++; }
    }
    # --brace keeps a wrapper as braces, and the checker accepts both forms.
    my $in = "void f(int a) {\n    __BEGIN_TRY\n    int a = 1;\n    __END_CATCH\n}\n";
    my ($out) = rewrite($in, \%none, { 2 => 1 });
    if ($out ne "void f(int a) {\n    {\n    int a = 1;\n    }\n}\n" || check($in, $out, \%none)) {
        print "brace override: got:\n$out";
        $failed++;
    }
    # The checker refuses anything else: a removed inner site, a removed
    # swallowing pair, a moved statement, a changed comment.
    my @rejects = (
        ["void f() {\n    a();\n    __BEGIN_TRY\n    b();\n    __END_CATCH\n}\n", "void f() {\n    a();\n    b();\n}\n"],
        ["void f() {\n    __BEGIN_TRY\n    b();\n    __END_CATCH_NO_RETHROW\n}\n", "void f() {\n    b();\n}\n"],
        ["void f() {\n    __BEGIN_TRY\n    b();\n    c();\n    __END_CATCH\n}\n", "void f() {\n    c();\n    b();\n}\n"],
        ["void f() {\n    // one\n    __BEGIN_TRY b(); __END_CATCH\n}\n", "void f() {\n    // two\n    b();\n}\n"],
        ["void f() {\n    __BEGIN_TRY\n    b();\n    __END_CATCH\n}\n", "void f() {\n    {\n    b();\n    __END_CATCH\n}\n"],
        ["#define S \"a  b\"\nint x;\n", "#define S \"a b\"\nint x;\n"],
        ["#define X a \\\n    b\nint x;\n", "#define X a b\nint x;\n"],
        ["int x = a >= b;\n", "int x = a > = b;\n"],
        ["int x = a >> b;\n", "int x = a > > b;\n"],
        ["void f() { x >>= 1; }\n", "void f() { x >> = 1; }\n"],
        ["char c = ' ';\n", "char c = '  ';\n"],
    );
    for my $n (0 .. $#rejects) {
        unless (check($rejects[$n][0], $rejects[$n][1], \%none)) {
            print "reject $n: the checker accepted it\n";
            $failed++;
        }
    }
    # The checker accepts a directive and a comment re-indented by a formatter.
    if (my $c = check("#define X a \\\n  b // c\n/* d\n   e */\n", "#define X a \\\n    b   // c\n/* d\n e */\n", \%none)) {
        print "reindent: the checker rejected it: $c\n";
        $failed++;
    }

    # A function-like macro made object-like differs only by a blank.
    unless (check("#define M(a) a\n", "#define M (a) a\n", \%none)) {
        print "define: the checker accepted a function-like macro made object-like\n";
        $failed++;
    }

    # --check over a fake git: revisions A, B (A with a retired pair and a
    # changed document), C (a changed statement), D (a document only), E
    # (an added source), and the working tree.
    my $before = "void f() {\n    __BEGIN_TRY\n    g();\n    __END_CATCH\n}\n";
    # F: x.cpp retired and a changed statement in a source with a non-ASCII
    # name, which git would quote without -z.
    my $accented = "\xc3\xa9.cpp";
    my %blob = ('A:x.cpp' => $before, 'B:x.cpp' => "void f() {\n    g();\n}\n", 'C:x.cpp' => "void f() {\n    h();\n}\n",
                'F:x.cpp' => "void f() {\n    g();\n}\n", "A:$accented" => "void k() {}\n", "F:$accented" => "void k() { h(); }\n");
    my %diff = ('A B' => "M\0x.cpp\0M\0docs/plan.md\0", 'A C' => "M\0x.cpp\0", 'A A' => '',
                'A D' => "M\0docs/plan.md\0", 'A E' => "A\0y.cpp\0", 'A' => "M\0x.cpp\0",
                'A F' => "M\0x.cpp\0M\0$accented\0");
    my $fakeGit = sub {
        my ($cmd, @args) = @_;
        return ("A\n", 0) if $cmd eq 'merge-base' && $args[0] eq 'Z';
        return ($blob{ $args[0] }, 0) if $cmd eq 'show' && exists $blob{ $args[0] };
        return ('', 128 << 8) if $cmd eq 'diff' && $args[0] ne '-z';
        my $key = join ' ', @args[ 3 .. $#args ];
        return ($diff{$key}, 0) if $cmd eq 'diff' && exists $diff{$key};
        return ('', 128 << 8);    # fatal: unknown revision
    };
    my $worktree = sub { "void f() {\n    {\n    g();\n    }\n}\n" };
    my %ranges = ('A..B' => 0, 'Z...B' => 0, 'A' => 0, 'A..C' => 1, 'A..A' => 1, 'A..D' => 1, 'A..E' => 1, 'A..F' => 1,
                  'A..Q' => 1, 'Q..B' => 1, 'Q...B' => 1, 'A....B' => 1, 'A..' => 1, '..B' => 1, '' => 1);
    for my $range (sort keys %ranges) {
        my $said = '';
        my $got = runCheck($range, \%none, $fakeGit, $worktree, sub { $said .= join '', @_ });
        if ($got != $ranges{$range}) {
            print "--check '$range': exit $got, want $ranges{$range}:\n$said";
            $failed++;
        }
    }

    # The non-ASCII path is read and checked, not skipped or misread: its own
    # content is what fails it.
    my $said = '';
    runCheck('A..F', \%none, $fakeGit, $worktree, sub { $said .= join '', @_ });
    unless ($said =~ /^FAIL \Q$accented\E: differs/m) {
        print "--check 'A..F' did not fail $accented on its content:\n$said";
        $failed++;
    }

    # A --brace that names no pair writes nothing, and the brace-macro table
    # is not read outside the repository root.
    my $dir = tempdir(CLEANUP => 1);
    my $file = "$dir/x.cpp";
    open(my $fh, '>', $file) or die "$file: $!\n";
    print $fh $before;
    close $fh;
    my $sink = '';
    open(my $quiet, '>', \$sink) or die;
    my $previous = select $quiet;
    my $got = runRewrite(\%none, ["$file:3"], $file);
    select $previous;
    if ($got != 1 || slurp($file) ne $before) {
        print "--brace on a line with no pair: exit $got, file " . (slurp($file) eq $before ? 'unchanged' : 'rewritten') . "\n";
        $failed++;
    }
    my $home = getcwd();
    chdir $dir or die "$dir: $!\n";
    my $loaded = eval { loadBraceMacros(); 1 };
    chdir $home or die "$home: $!\n";
    if ($loaded) {
        print "loadBraceMacros ran outside the repository root\n";
        $failed++;
    }

    print $failed ? "self-test: $failed FAILED\n" : "self-test: OK\n";
    return $failed ? 1 : 0;
}

#----------------------------------------------------------------------
my (@braces, @files, $check, $selfTest);
while (@ARGV) {
    my $arg = shift @ARGV;
    if ($arg eq '--brace') { push @braces, shift(@ARGV) // die "--brace wants FILE:LINE\n"; }
    elsif ($arg eq '--check') { $check = shift(@ARGV) // die "--check wants a revision range\n"; }
    elsif ($arg eq '--self-test') { $selfTest = 1; }
    elsif ($arg =~ /^-/) { die "unknown option $arg\n"; }
    else { push @files, $arg; }
}
exit selfTest() if $selfTest;
exit runCheck($check, loadBraceMacros(), \&git, sub { -f $_[0] ? slurp($_[0]) : undef }, sub { print @_ }) if defined $check;
die "usage: perl tools/r5_retire_try.pl [--brace FILE:LINE]... FILE... | --check REV[..REV] | --self-test\n"
    unless @files;
exit runRewrite(loadBraceMacros(), \@braces, @files);
