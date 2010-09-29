#!/usr/bin/perl -W
# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2010, Pittsburgh Supercomputing Center (PSC).
#
# Permission to use, copy, and modify this software and its documentation
# without fee for personal use or non-commercial use within your organization
# is hereby granted, provided that the above copyright notice is preserved in
# all copies and that the copyright and this permission notice appear in
# supporting documentation.  Permission to redistribute this software to other
# organizations or individuals is not permitted without the written permission
# of the Pittsburgh Supercomputing Center.  PSC makes no representations about
# the suitability of this software for any purpose.  It is provided "as is"
# without express or implied warranty.
# -----------------------------------------------------------------------------
# %PSC_END_COPYRIGHT%

# Better algorithm:
#	use file mtime as long as mtime != ctime
#	and svn diff -I\$Id yields isn't empty
# XXX if file was copied from a revision, switch to that file to continue

use strict;
use warnings;
use Getopt::Std;

sub usage {
	warn "usage: $0 file ...\n";
	exit 1;
}

my %opts;
getopts("", \%opts) or usage;

my @m = qw(0 January February March April May June July
    August September October November December);

my %m;
@m{@m} = (1 .. @m);

sub get_unless_last_rev {
	my ($fn, $d, $m, $y) = @_;

	my @out = `svn log '$fn'`;

	foreach my $ln (@out) {
		my ($t_rev, $t_y, $t_m, $t_d) =
		    ($ln =~ /^r(\d+) \s+ \| \s+ (?:\w+) \s+ \| \s+
		    (\d+)-(\d+)-0*(\d+) \s+ (\d+):(\d+):(\d+)/x) or next;

		# if this revision solely was comprised of comments or date bump, ignore
		my $prev_rev = $t_rev - 1;
		my $t_out = `svn diff -r $prev_rev:$t_rev --diff-cmd=diff -x '-I^\\.Dd -I^\\.\\\\"' '$fn'`;
		my $cnt = ($t_out =~ tr/\n//);

		next unless $cnt > 2;

		return if $t_y == $y && $t_m == $m && $t_d == $d;
		return ($t_d, $t_m, $t_y);
	}
}

sub slurp {
	my ($fn) = @_;
	local $/;

	open F, "<", $fn or die "$fn: $!\n";
	my $data = <F>;
	close F;

	return $data;
}

my ($d, $m, $y, $sm);

foreach my $fn (@ARGV) {
	my $out = `svn diff --diff-cmd=diff -x '-I\$Id -I^\\.\\\\"' '$fn'`;
	my $cnt = ($out =~ tr/\n//);
	my $data;

	if ($cnt > 2) {
		# has local changes, bump date
		$data = slurp $fn;
		my ($mt) = (stat $fn)[9];
		($d, $m, $y) = (localtime $mt)[3 .. 5];
		$y += 1900;
		$m++;
		goto bump;
	}

	$data = slurp $fn;
	($sm, $d, $y) = ($data =~ /^\.Dd (\w+) (\d+), (\d+)$/m) or next;
	$m = $m{$sm};

	($d, $m, $y) = get_unless_last_rev($fn, $d, $m, $y) or next;

 bump:
	# update the Dd date
	$data =~ s/^\.Dd .*/.Dd $m[$m] $d, $y/m;

	open F, ">", $fn or die "$fn: $!\n";
	print F $data;
	close F;
} continue {
	system("groff -z $fn");
}
