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

sub getoptv {
	my ($optstr, $ropts) = @_;
	my @av = @ARGV;

	getopts($optstr, $ropts) or return 0;

	for (my $j = 0; $j < length($optstr); $j++) {
		delete $ropts->{$1}, $j++ if substr($optstr, $j, 2) =~ /^([a-zA-Z0-9]):/;
	}

	ARG: for (my $i = 0; $i < @av; $i++) {
		my $s = $av[$i];
		if ($s =~ /^-/) {
			for (my $k = 1; $k < length($s); $k++) {
				for (my $j = 0; $j < length($optstr); $j++) {
					my $ch = substr($optstr, $j, 1);
					my $wantarg = $j + 1 < length($optstr) &&
					    substr($optstr, $j + 1, 1) eq ":";
					next unless $wantarg;
					$j++, next unless substr($s, $k, 1) eq $ch;

					my $arg;
					if ($k + 1 == length($s)) {
						$arg = $av[$i + 1];
						$s++;
					} else {
						$arg = substr($s, $k + 1);
					}
					$ropts->{$ch} = [] unless exists $ropts->{$ch};
					push @{ $ropts->{$ch} }, $arg;
					next ARG;
				}
			}
		}
	}
	return 1;
}

my %opts;
getoptv("g:h:x:", \%opts) or usage();
usage() unless @ARGV == 1;

my $outfn = $ARGV[0];

my @vals;
my @types;
my @hdrs;
@hdrs = uniq sort map { glob } @{ $opts{h} } if $opts{h};
if ($opts{x}) {
	@hdrs = filter [ map { glob } @{ $opts{x} } ], @hdrs;
}
my $lvl = 0;
foreach my $hdr (@hdrs) {
	open HDR, "<", $hdr or die "$hdr: $!\n";
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
