#!/usr/bin/perl
#
# golden2corpus.pl - writes the seed corpus of one packet-read fuzz target
# (tests/fuzz/) from the golden body fixtures in tests/golden/.
#
#   perl tools/fuzz/golden2corpus.pl <golden-dir> <wire-layout.txt> \
#        <factory_registrations.txt> <server> <status>[,<status>...] <out-dir>
#
# <server> is a first column of factory_registrations.txt (gameserver,
# loginserver): only the packets that server registers are seeded, since
# no other id gets past its factory table. Every seed is one input in the
# format tests/fuzz/StreamFuzz.h describes,
#
#   [code byte][status byte][id u16][size u32][sequence u8][body]
#
# all little-endian, with sequence 0, which is what a session's first
# packet carries. Seeds written:
#
#   golden-<file>-s<status>   each <Name>[.variant].code<N>.hex golden
#                             whose Name the server registers, with code N,
#                             once per status given;
#   id-<Name>-<len>-s<status> per registered packet, an empty body and a
#                             zero-filled body of min(max size, 64) bytes,
#                             under code 0, once per status given.
#
# The out-dir is emptied of earlier seeds first, so a golden that was
# removed does not linger. Prints the number of seeds written.

use strict;
use warnings;
use File::Path qw(make_path);

die "usage: $0 <golden-dir> <wire-layout.txt> <factory_registrations.txt> <server> <status,...> <out-dir>\n"
    unless @ARGV == 6;
my ($golden_dir, $layout, $registrations, $server, $status_list, $out) = @ARGV;

my @statuses = split /,/, $status_list;
die "bad status list '$status_list'\n" if !@statuses || grep { !/^\d+$/ || $_ > 255 } @statuses;

my (%id, %max);
open my $L, '<', $layout or die "$layout: $!\n";
while (<$L>) {
    next if /^#/;
    chomp;
    my ($i, $name, $m) = split /\t/;
    next unless defined $m;
    $id{$name} = $i;
    $max{$name} = $m;
}
close $L;

my %registered;
open my $R, '<', $registrations or die "$registrations: $!\n";
while (<$R>) {
    chomp;
    my ($srv, $factory) = split /\t/;
    next unless defined $factory && $srv eq $server;
    (my $name = $factory) =~ s/Factory$//;
    die "$registrations: $name is not in $layout\n" unless exists $id{$name};
    $registered{$name} = 1;
}
close $R;
die "$registrations: no packets registered for '$server'\n" unless %registered;

make_path($out);
-d $out or die "$out: cannot create\n";
opendir my $D, $out or die "$out: $!\n";
for my $f (readdir $D) {
    next unless $f =~ /^(golden|id)-/;
    unlink "$out/$f" or die "$out/$f: $!\n";
}
closedir $D;

my $n = 0;
sub seed {
    my ($file, $code, $frame) = @_;
    for my $status (@statuses) {
        open my $O, '>:raw', "$out/$file-s$status" or die "$out/$file-s$status: $!\n";
        print $O chr($code) . chr($status) . $frame;
        close $O;
        $n++;
    }
}

opendir my $G, $golden_dir or die "$golden_dir: $!\n";
for my $f (sort grep { /\.hex$/ } readdir $G) {
    my ($base) = $f =~ /^(.+)\.hex$/;
    my ($name) = split /\./, $base;
    next unless $registered{$name};
    my ($code) = $base =~ /\.code(\d+)$/;
    die "$f: no .code<N> suffix\n" unless defined $code;
    open my $H, '<', "$golden_dir/$f" or die "$golden_dir/$f: $!\n";
    local $/;
    (my $hex = <$H>) =~ s/\s+//g;
    close $H;
    my $body = pack 'H*', $hex;
    seed("golden-$base", $code, pack('vVC', $id{$name}, length $body, 0) . $body);
}
closedir $G;

for my $name (sort keys %registered) {
    my $filled = $max{$name} < 64 ? $max{$name} : 64;
    for my $len ($filled ? (0, $filled) : (0)) {
        seed("id-$name-$len", 0, pack('vVC', $id{$name}, $len, 0) . ("\0" x $len));
    }
}

print "$n seeds\n";
