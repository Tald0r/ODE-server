#!/usr/bin/env perl
#----------------------------------------------------------------------
# check_includes.pl — include-graph architecture test
# (docs/RESTRUCTURING.md task 2.2; sidecar's ArchitectureRules analog).
#
# Perl, not Python: the dev container ships perl (the repo's generators
# already use it) and has no python3.
#
# Layers (task 2.1):
#   kernel  — the files listed in tests/arch/kernel_files.txt. These are
#             the de-kernel candidates: wire primitives with no game,
#             DB, script or server-type dependencies.
#   core    — game-domain code: the gameserver domain module dirs.
#             Membership is aspirational for existing files (the split
#             has not happened yet); the rules below still bind them.
#   app     — everything else under src/. No rules yet.
#
# Rules:
#   K1  a kernel file may quote-include only kernel files. (System
#       <...> includes are always allowed.) This is transitive by
#       construction: every included file must itself be in the list,
#       so the whole closure stays kernel.
#   K2  a kernel file may not mention a server-type macro
#       (__GAME_SERVER__ / __LOGIN_SERVER__ / __SHARED_SERVER__ /
#       __GAME_CLIENT__) nor __COMBAT__: kernel code has one meaning,
#       not four — and since the 2.4 flip every app links the ONE
#       macro-free de-kernel compile, so a macro-conditional in a
#       kernel file would silently compile as "off" everywhere.
#   K3  the packet libraries in src/Core/CMakeLists.txt may not define
#       any macro outside the K2 banned set: the kernel objects they
#       share from de-kernel are guaranteed macro-independent only for
#       macros K2 scans for, so "one meaning" must be enforced at the
#       definition site too, not hold by coincidence of today's -D set.
#   C1  a core file may not include MySQL, Lua, or socket-transport
#       headers. Persistence belongs behind repository interfaces
#       (task 3.2), transport belongs to the apps.
#   D1  a de-core file (src/domain/) may quote-include only an existing
#       domain header, spelled "domain/X.h", and angle-include only
#       <algorithm> and <cmath>: de-core
#       is freestanding by definition (task 3.3), and the client builds a
#       byte-identical copy of part of it with every toolchain it has, so
#       a standard header beyond those two is a new portability question
#       for all of them. Until this rule existed the quote half was
#       enforced only indirectly, by formula_tests linking nothing else.
#
# Existing violations of C1 live in tests/arch/baseline.txt and may
# only shrink (the ratchet pattern): a NEW violation fails the build,
# and fixing one fails too until the baseline line is deleted in the
# same commit. K1/K2 have no baseline — the kernel list is defined as
# exactly what passes, so extend the list only with files that comply.
#
# Never weaken a rule to fix a failure: a violation means the file is
# in the wrong layer, or the include is. Run from the repository root.
#----------------------------------------------------------------------
use strict;
use warnings;

my $kernel_list = 'tests/arch/kernel_files.txt';
my $baseline    = 'tests/arch/baseline.txt';

#----------------------------------------------------------------------
# Layer membership
#----------------------------------------------------------------------
my (%kernel, %kernel_base);
open(my $kf, '<', $kernel_list) or die "$kernel_list: $!";
while (<$kf>) {
    s/\r?\n$//;
    next if /^\s*(#|$)/;
    -f $_ or die "$kernel_list names a missing file: $_\n";
    $kernel{$_} = 1;
    my ($base) = m{([^/]+)$};
    die "$kernel_list: duplicate basename $base\n" if $kernel_base{$base};
    $kernel_base{$base} = $_;
}
close $kf;

my @core_dirs = map { "src/server/gameserver/$_" }
    qw(skill item quest war mission couple ctf mofus exchange);

# C1: forbidden include basenames for core files.
my %core_forbidden = map { $_ => 1 } qw(
    mysql.h
    lua.h lauxlib.h lualib.h lua.hpp
    Socket.h ServerSocket.h SocketImpl.h SocketAPI.h DatagramSocket.h
);

#----------------------------------------------------------------------
# Scan
#----------------------------------------------------------------------
sub quoted_includes {
    my ($path) = @_;
    my @inc;
    open(my $fh, '<', $path) or die "$path: $!";
    while (<$fh>) {
        # Strip line comments so a commented-out include is not an edge.
        s{//.*$}{};
        push @inc, $1 if /^\s*#\s*include\s*"([^"]+)"/;
    }
    close $fh;
    return @inc;
}

sub angle_includes {
    my ($path) = @_;
    my @inc;
    open(my $fh, '<', $path) or die "$path: $!";
    while (<$fh>) {
        s{//.*$}{};
        push @inc, $1 if /^\s*#\s*include\s*<([^>]+)>/;
    }
    close $fh;
    return @inc;
}

my @violations;

# K1 + K2 over the kernel list.
for my $file (sort keys %kernel) {
    for my $inc (quoted_includes($file)) {
        # A parent-relative quoted include resolves from the including
        # file's own directory BEFORE any -I, so it can escape src/Core
        # while the basename check below still passes (e.g.
        # "../server/gameserver/Player.h" — basename Player.h IS a
        # kernel file). Ban the form outright.
        if ($inc =~ m{\.\.}) {
            push @violations, "K1 $file uses parent-relative include \"$inc\"";
            next;
        }
        my ($base) = $inc =~ m{([^/]+)$};
        push @violations, "K1 $file includes non-kernel \"$inc\""
            unless $kernel_base{$base};
    }
    open(my $fh, '<', $file) or die "$file: $!";
    while (<$fh>) {
        if (/__(GAME_SERVER|LOGIN_SERVER|SHARED_SERVER|GAME_CLIENT|COMBAT)__/) {
            push @violations, "K2 $file mentions a server-type macro";
            last;
        }
    }
    close $fh;
}

# K3: the packet libraries may only define K2-banned macros. The whole
# link-flip safety argument (task 2.4) is that the shared de-kernel
# objects could never have depended on the per-server defines because K2
# scans kernel files for exactly those macros; a NEW define on a packet
# library would sit outside that guarantee.
{
    my %k2_banned = map { $_ => 1 }
        qw(__GAME_SERVER__ __LOGIN_SERVER__ __SHARED_SERVER__
           __GAME_CLIENT__ __COMBAT__);
    my $core_cmake = 'src/Core/CMakeLists.txt';
    open(my $cm, '<', $core_cmake) or die "$core_cmake: $!";
    while (<$cm>) {
        next unless /target_compile_definitions\s*\(\s*(\S+)\s+\S+\s+(.+?)\)/;
        my ($target, $defs) = ($1, $2);
        for my $d (split ' ', $defs) {
            push @violations,
                "K3 $core_cmake: $target defines $d, outside the K2 set"
                unless $k2_banned{$d};
        }
    }
    close $cm;
}

# D1 over src/domain: quote-includes are "domain/X.h" only, and the
# standard library is <algorithm> and <cmath> only.
{
    my %d1_std = map { $_ => 1 } qw(algorithm cmath);
    if (opendir(my $dd, 'src/domain')) {
        for my $entry (sort readdir($dd)) {
            next unless $entry =~ /\.(h|cpp)$/;
            my $path = "src/domain/$entry";
            for my $inc (quoted_includes($path)) {
                # The one allowed form is "domain/X.h" for a header that
                # exists: the spelling every consumer resolves the same
                # way, the client's copy included.
                next if $inc =~ m{^domain/[^/]+$} && -f "src/$inc";
                push @violations, "D1 $path includes \"$inc\", not an existing \"domain/X.h\"";
            }
            for my $inc (angle_includes($path)) {
                next if $d1_std{$inc};
                push @violations, "D1 $path includes <$inc>, outside <algorithm> and <cmath>";
            }
        }
        closedir $dd;
    }
}

# C1 over the core dirs.
for my $dir (@core_dirs) {
    next unless -d $dir;
    my @files;
    my @stack = ($dir);
    while (my $d = pop @stack) {
        opendir(my $dh, $d) or die "$d: $!";
        for my $e (sort readdir $dh) {
            next if $e =~ /^\./;
            my $p = "$d/$e";
            if    (-d $p)              { push @stack, $p; }
            elsif ($e =~ /\.(cpp|h)$/) { push @files, $p; }
        }
        closedir $dh;
    }
    for my $file (@files) {
        for my $inc (quoted_includes($file)) {
            my ($base) = $inc =~ m{([^/]+)$};
            push @violations, "C1 $file includes \"$inc\""
                if $core_forbidden{$base};
        }
    }
}

#----------------------------------------------------------------------
# Compare against the baseline (C1 only; K rules must be clean).
#----------------------------------------------------------------------
my %base;
if (open(my $bf, '<', $baseline)) {
    while (<$bf>) {
        s/\r?\n$//;
        next if /^\s*(#|$)/;
        $base{$_} = 1;
    }
    close $bf;
}

my $fail = 0;
my %seen;
for my $v (sort @violations) {
    $seen{$v} = 1;
    next if $base{$v};
    print "[FAIL] new architecture violation: $v\n";
    $fail = 1;
}
for my $b (sort keys %base) {
    next if $seen{$b};
    print "[FAIL] fixed but still baselined (delete the line from "
        . "$baseline in this commit): $b\n";
    $fail = 1;
}

my $held = grep { $base{$_} } keys %seen;
printf "[OK]   arch: %d violation(s), all baselined (shrink-only)\n", $held
    unless $fail;
printf "       kernel files: %d\n", scalar keys %kernel;
exit $fail;
