#!/usr/bin/env perl
# check_knowledge_bundle.pl - the structural owner of docs/, the Open
# Knowledge Format (OKF v0.2) bundle: a rule lives in one concept file,
# every folder's index.md lists what it holds, and CLAUDE.md, README.md
# and AGENTS.md link into it. That only works while the links hold and
# every concept can be found from its folder's index, so this check fails
# the suite on:
#
#   1. a concept (any .md under docs/ but index.md and log.md) without a
#      YAML frontmatter block carrying a non-empty `type`;
#   2. an index.md with frontmatter - the bundle root's carries only
#      `okf_version`, every other index none at all;
#   3. a folder holding markdown without an index.md, or an index.md that
#      does not link each concept beside it and each subfolder (as
#      `name/` or `name/index.md`) that holds markdown;
#   4. a `## ` heading in a log.md that is not an ISO day (`## YYYY-MM-DD`);
#   5. a local link, in the bundle or in CLAUDE.md, README.md and
#      AGENTS.md, that points at nothing (fenced code is skipped, as is a
#      bare `#fragment`; a fragment on a real file is not checked).
#
# Usage: check_knowledge_bundle.pl [<repo root>]   (default: the cwd)
#
# Frontmatter is read with a line scanner, not a YAML parser: the keys the
# bundle uses (docs/knowledge-bundle.md) are all top-level `key: value`
# lines, and a block that opens and never closes counts as missing.

use strict;
use warnings;
use File::Find;
use File::Basename qw(dirname basename);
use File::Spec;
use Cwd qw(abs_path);

my $root = abs_path(shift @ARGV // '.');
my $bundle = "$root/docs";
die "no bundle at $bundle\n" unless -d $bundle;

my @markdown;
find({ wanted => sub { push @markdown, $File::Find::name if /\.md$/ && -f }, no_chdir => 1 }, $bundle);
@markdown = sort @markdown;

my @problems;

sub slurp {
	my ($file) = @_;
	open my $fh, '<', $file or die "cannot read $file: $!\n";
	local $/;
	my $text = <$fh>;
	close $fh;
	$text =~ s/\r\n/\n/g;
	return $text;
}

sub relative {
	my ($file) = @_;
	return File::Spec->abs2rel($file, $root);
}

# Returns undef without a frontmatter block, otherwise a hash of its
# top-level keys to their raw values.
sub frontmatter {
	my ($text) = @_;
	my @lines = split /\n/, $text, -1;
	return undef unless @lines && $lines[0] eq '---';
	my %keys;
	for my $i (1 .. $#lines) {
		return \%keys if $lines[$i] eq '---';
		if ($lines[$i] =~ /^([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$/) {
			$keys{$1} = $2;
		}
	}
	return undef;
}

sub links {
	my ($text) = @_;
	$text =~ s/^\s*```.*?^\s*```//msg;
	my @targets;
	while ($text =~ /\]\(([^)\s]+)\)/g) {
		push @targets, $1;
	}
	return @targets;
}

# 1. every concept declares a type
for my $file (@markdown) {
	my $name = basename($file);
	next if $name eq 'index.md' || $name eq 'log.md';
	my $front = frontmatter(slurp($file));
	unless ($front && defined $front->{type} && $front->{type} =~ /\S/) {
		push @problems, relative($file) . ": needs a YAML frontmatter block with a non-empty `type` (see docs/knowledge-bundle.md)";
	}
}

# 2. indexes carry no frontmatter except the root's okf_version
for my $file (@markdown) {
	next unless basename($file) eq 'index.md';
	my $front = frontmatter(slurp($file));
	my $is_root = dirname($file) eq $bundle;
	if ($is_root) {
		my @keys = $front ? sort keys %$front : ();
		push @problems, relative($file) . ": the bundle root's index carries only `okf_version` in its frontmatter"
			unless @keys == 1 && $keys[0] eq 'okf_version';
	} elsif ($front) {
		push @problems, relative($file) . ": an index.md below the root has no frontmatter";
	}
}

# 3. every folder with markdown has an index listing its concepts and subfolders
my %folders;
for my $file (@markdown) {
	my $folder = dirname($file);
	while (length($folder) >= length($bundle)) {
		$folders{$folder} = 1;
		last if $folder eq $bundle;
		$folder = dirname($folder);
	}
}
for my $folder (sort keys %folders) {
	my $index = "$folder/index.md";
	unless (-f $index) {
		push @problems, relative($index) . " is missing: every folder holding knowledge has an index.md";
		next;
	}
	my %listed = map { $_ => 1 } links(slurp($index));
	my @entries;
	for my $file (@markdown) {
		next unless dirname($file) eq $folder;
		my $name = basename($file);
		next if $name eq 'index.md' || $name eq 'log.md';
		push @entries, $name;
	}
	for my $sub (sort keys %folders) {
		push @entries, basename($sub) . '/' if dirname($sub) eq $folder && $sub ne $folder;
	}
	for my $entry (@entries) {
		next if $listed{$entry} || $listed{"${entry}index.md"} || $listed{"./$entry"};
		push @problems, relative($index) . " does not list $entry (add a `* [Title]($entry) - description` line)";
	}
}

# 4. log headings are ISO days
for my $file (@markdown) {
	next unless basename($file) eq 'log.md';
	for my $line (split /\n/, slurp($file)) {
		next unless $line =~ /^## /;
		(my $heading = $line) =~ s/\s+$//;
		push @problems, relative($file) . ": `$heading` - log.md groups entries under `## YYYY-MM-DD` headings"
			unless $heading =~ /^## \d{4}-\d{2}-\d{2}$/;
	}
}

# 5. local links resolve
my @documents = @markdown;
for my $top (qw(CLAUDE.md README.md AGENTS.md)) {
	push @documents, "$root/$top" if -f "$root/$top";
}
for my $file (@documents) {
	for my $target (links(slurp($file))) {
		next if $target =~ /^(https?:|mailto:|#)/;
		(my $path = $target) =~ s/#.*$//;
		$path =~ s/%20/ /g;
		next if $path eq '';
		my $resolved = File::Spec->rel2abs($path, dirname($file));
		push @problems, relative($file) . " -> $target points at nothing" unless -e $resolved;
	}
}

if (@problems) {
	print "knowledge_bundle: FAIL\n";
	print "  $_\n" for @problems;
	exit 1;
}
printf "knowledge_bundle: OK (%d markdown files under docs/)\n", scalar @markdown;
exit 0;
