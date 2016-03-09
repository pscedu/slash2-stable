#!/usr/bin/perl -W
# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2015-2016, Google, Inc.
# Copyright 2009-2015, Pittsburgh Supercomputing Center
# All rights reserved.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
# PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
# --------------------------------------------------------------------
# %END_LICENSE%

use Getopt::Std;
use Net::SMTP;
use POSIX qw(mktime strftime);
use warnings;
use strict;

my %opts;
getopts("mN", \%opts) or die "usage: $0 [-mN] testname logbase\n";

my ($testname, $logbase) = @ARGV;

sub avg {
	my ($r) = @_;
	my $sum = 0;
	my $i;
	foreach $i (@$r) {
		$sum += $i;
	}
	return $sum / (@$r || 1);
}

sub getstats {
	my ($dat, $date, $t_tm, $ndays) = @_;
	my @tms;
	open F, "<", $dat or die "$dat: $!\n";
	while (<F>) {
		# 2009-03-12t16:52
		if (/^(\d\d\d\d)-(\d\d)-(\d\d)t\d\d:\d\d(?=\s)/ and
		    $& ne $date and $t_tm - mktime(0, 0, 0, $3, $2, $1) <=
		    $ndays * 24 * 60 * 60) {
			my $t = $';
			$t =~ s/^\s+|\s+$//g;
			push @tms, $t;
		}
	}
	close F;
	return \@tms;
}

sub fmtres {
	my ($tm, $r_stats) = @_;

	if (@$r_stats) {
		my $avg = avg $r_stats;
		return sprintf "%3d %5d %6d %4d%%", scalar(@$r_stats),
		    $avg, $avg - $tm, ($avg-$tm)/$avg * 100,
	}
	return sprintf "%3d %5s %6s %5s", 0, "-", "-", "-";
}

sub show_results {
	my ($dat, $date, $tm) = @_;

	my ($t_yr, $t_mon, $t_day) =
	    ($date =~ /^(\d\d\d\d)-(\d\d)-(\d\d)t\d\d:\d\d$/) or
	    die "date $date: bad format\n";
	my $t_tm = mktime(0, 0, 0, $3, $2, $1);

	# average last 2, 8, and 30 days' stats and compare
	my $d_st = getstats $dat, $date, $t_tm, 2;
	my $w_st = getstats $dat, $date, $t_tm, 8;
	my $m_st = getstats $dat, $date, $t_tm, 30;

	my $tlabel = $dat;
	$tlabel =~ s!.*/!!;

	return sprintf " %-15s %5d|%s|%s|%s\n", $tlabel, $tm,
	    fmtres($tm, $d_st), fmtres($tm, $w_st), fmtres($tm, $m_st);
}

my $report = "timing stats (negative means longer, positive means improvement)\n";
$report .= sprintf " %-15s %5s+%22s+%22s+%22s\n", "", "",
    "-------- day ---------",
    "-------- week --------",
    "------- month --------";

my $fm="%3s %5s %6s %5s";
$report .= sprintf " %-15s %5s|$fm|$fm|$fm\n", "test", "time",
    '#t', qw(avgtm tmdiff %impr),
    '#t', qw(avgtm tmdiff %impr),
    '#t', qw(avgtm tmdiff %impr);
$report .= " " .
    "---------------------" .
    "+----------------------" .
    "+----------------------" .
    "+----------------------\n";

my $date = strftime("%Ft%R", localtime);
while (<STDIN>) {
	my ($name, $tm) = split;
	my $dat = "$logbase/$testname.$name";
	unless ($opts{N}) {
		open DAT, ">>", $dat or die "$dat: $!\n";
		print DAT "$date $tm\n";
		close DAT;
	}
	$report .= show_results $dat, $date, $tm;
}

print "\n\n", $report;

if ($opts{m}) {
	my $smtp = Net::SMTP->new('mailer.psc.edu');
	$smtp->mail('zest-devel@psc.edu');
	$smtp->to('zest-devel@psc.edu');
	$smtp->data();
	$smtp->datasend("To: zest-devel\@psc.edu\n");
	$smtp->datasend("Subject: tsuite report\n\n");
	$smtp->datasend("$report");
	$smtp->dataend();
	$smtp->quit;
}

exit 0;
