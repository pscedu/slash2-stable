#!/usr/bin/perl -W
# $Id$

use strict;
use warnings;
use Getopt::Std;

sub usage {
	warn "usage: $0 [-e] file\n";
	exit 1;
}

my %opts;
getopts("e", \%opts) or usage;
usage unless @ARGV == 1;

open F, "<", $ARGV[0] or die "$ARGV[0]: $!\n";
local $/;
my $data = <F>;
close F;

# debug file ID
print qq{# 1 "$ARGV[0]"\n};

if ($data !~ m!psc_util/log\.h! or
    $ARGV[0] =~ m|/log\.c$| or
    !$opts{e}) {
	print $data;
	exit 0;
}

my $i;

sub advance {
	my ($len) = @_;

	print substr($data, $i, $len);
	$i += $len;
}

for ($i = 0; $i < length $data; ) {
	if (substr($data, $i, 2) eq "/*") {
		# skip comments
		if (substr($data, $i + 2) =~ m[\*/]) {
			advance($+[0] + 2);
		} else {
			advance(length($data) - $i);
		}
	} elsif (substr($data, $i, 2) eq q{//}) {
		if (substr($data, $i + 2) =~ m[\n]) {
			advance($+[0] + 1);
		} else {
			advance(length($data) - $i);
		}
	} elsif (substr($data, $i, 1) eq q{"}) {
		# skip strings
		advance(1);
		my $esc = 0;
		for (; $i < length($data) && $esc == 0 &&
		    substr($data, $i, 1) ne q{"}; advance(1)) {
			if ($esc) {
				$esc = 0;
			} elsif (substr($data, $i, 1) eq "\\") {
				$esc = 1;
				$esc = 0;
			}
		}
		advance(1);
	} elsif (substr($data, $i) =~ /^[^=]\s*\n{\s*\n/s) {
		# catch routine entrance
		advance($+[0] - 1);
		print "PFL_ENTER();";
		advance(1);
	} elsif (substr($data, $i) =~ /^\n}[ \t]*\n/s) {
		# catch implicit 'return'
		my $len = $+[0];
		advance(1);
		print "PFL_RETURNX();";
		advance($len - 1);
	} elsif (substr($data, $i) =~ /^return(;\s*}?\s*)/s) {
		# catch 'return' without an arg
		my $end = $1;
		my $len = $+[0];
		$i += $len;
		print "PFL_RETURNX()$end";
	} elsif (substr($data, $i) =~ /^return\s*(\(\s*".*?"\s*\)|".*?")\s*(;\s*}?\s*)/s) {
		# catch 'return' with string literal arg
		my $rv = $1;
		my $end = $2;
		my $len = $+[0];
		$i += $len;
		print "PFL_RETURN_STRLIT($rv)$end";
	} elsif (substr($data, $i) =~ /^return\b\s*(.*?)\s*(;\s*}?\s*)/s) {
		# catch 'return' with an arg
		my $rv = $1;
		my $end = $2;
		my $len = $+[0];
		$i += $len;
		print "PFL_RETURN($rv)$end";
	} elsif (substr($data, $i) =~ /^(psc_fatalx?|exit|errx?)\s*\(.*?\)\s*(;\s*}?\s*)/s) {
		advance($+[0]);
	} elsif (substr($data, $i) =~ /^\w+/) {
		advance($+[0]);
	} else {
		advance(1);
	}
}
