#!/usr/bin/perl -W
# $Id$

# Better algorithm:
#	use file mtime as long as mtime != ctime
#	and svn diff -I\$Id yields isn't empty

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

	my $out = `svn log '$fn'`;

	my ($t_rev, $t_y, $t_m, $t_d) =
	    $out =~ /^r(\d+) \s+ \| \s+ (?:\w+) \s+ \| \s+
	    (\d+)-(\d+)-0*(\d+) \s+ (\d+):(\d+):(\d+)/mx or return;

	return if $t_y == $y && $t_m == $m && $t_d == $d;

	# if the last revision solely comprised a date bump, ignore
	my $prev_rev = $t_rev - 1;
	$out = `svn diff -r $prev_rev:$t_rev --diff-cmd=diff -x '-I.Dd ' '$fn'`;
	my $cnt = ($out =~ tr/\n//);

	return unless $cnt > 2;
	return ($t_d, $t_m, $t_y);
}

sub slurp {
	my ($fn) = @_;
	local $/;

	open F, "<", $fn or die "$fn: $!\n";
	my $data = <F>;
	close F;

	return $data;
}

my ($day, $mon, $yr) = (localtime)[3, 4, 5];
$yr += 1900;
$mon++;

my ($d, $m, $y, $sm);

foreach my $fn (@ARGV) {
	my $out = `svn diff --diff-cmd=diff -x '-I\$Id' '$fn'`;
	my $cnt = ($out =~ tr/\n//);
	my $data;

	if ($cnt > 2) {
		# has local changes, bump date
		$data = slurp $fn;
		$y = $yr;
		$m = $mon;
		$d = $day;
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
