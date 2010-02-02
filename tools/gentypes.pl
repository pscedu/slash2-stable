#!/usr/bin/perl -W
# $Id$

use Getopt::Std;
use strict;
use warnings;

sub usage {
	die "usage: $0 [-g pat] [-h headers] [-x exclude] file\n";
}

sub in_array {
	my ($ar, $elem) = @_;
	local $_;

	foreach (@$ar) {
		return 1 if $_ eq $elem;
	}
	return (0);
}

sub uniq {
	my @a;
	local $_;

	foreach (@_) {
		push @a, $_ unless @a && $_ eq $a[$#a];
	}
	return @a;
}

sub filter {
	my $xr = shift;
	my @a;
	local $_;

	foreach (@_) {
		push @a, $_ unless in_array $xr, $_;
	}
	return @a;
}

my %opts;
getopts("g:h:x:", \%opts) or usage();
usage() unless @ARGV == 1;

my $outfn = $ARGV[0];

my @vals;
my @types;
my @hdrs = uniq sort <$opts{h}>;
if ($opts{x}) {
	@hdrs = filter [<$opts{x}>], @hdrs;
}
my $lvl = 0;
foreach my $hdr (@hdrs) {
	open HDR, "<", $hdr;
	while (<HDR>) {
		if ($lvl) {
			$lvl++ if /^\s*#if/;
			$lvl-- if /^\s*#endif/;
		} else {
			$lvl = 1 if /^\s*#if\s+0\s*$/;
			push @types, "struct $1" if /^(?:typedef\s+)?struct\s+(\w+)\s*{/;
			push @types, $1 if /^typedef\s+(?:struct\s+)?(?:\w+)\s+(\w+)\s*;/;

			push @vals, /($opts{g})/ if $opts{g};
		}
	}
	close HDR;
}

open OUT, "<", $outfn;
my $lines = eval {
	local $/;
	return <OUT>;
};
close OUT;

my $includes = join '', map { s!(?:include|\.\.)/!!g; qq{#include "$_"\n} } @hdrs;
$lines =~ s!(?<=/\* start includes \*/\n).*?(?=/\* end includes \*/)!$includes!s;

my $types = join '', map { "\tPRTYPE($_);\n" } uniq sort @types;
$lines =~ s!(?<=/\* start structs \*/\n).*?(?=/\* end structs \*/)!$types\t!s;

my $vals = join '', map { "\tPRVAL($_);\n" } uniq sort @vals;
$lines =~ s!(?<=/\* start constants \*/\n).*?(?=/\* end constants \*/)!$vals\t!s;

open OUT, ">", $outfn;
print OUT $lines;
close OUT;

exit 0;
