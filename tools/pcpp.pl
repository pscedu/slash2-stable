#!/usr/bin/perl -W
# $Id$

use strict;
use warnings;
use Getopt::Std;
use File::Basename;

sub usage {
	warn "usage: $0 [-e] file\n";
	exit 1;
}

my %opts;
getopts("e", \%opts) or usage;
usage unless @ARGV == 1;

my $fn = $ARGV[0];

open F, "<", $fn or die "$fn: $!\n";
local $/;
my $data = <F>;
close F;

# debug file ID
print qq{# 1 "$fn"\n};

if ($data !~ m!psc_util/log\.h! or
    basename($fn) eq "log.c" or
    basename($fn) eq "subsys.c" or
    basename($fn) eq "thread.c" or
    !$opts{e}) {
	print $data;
	exit 0;
}

my $i;
my $lvl = 0;
my $foff;

sub advance {
	my ($len) = @_;

	print substr($data, $i, $len);
	$i += $len;
}

sub get_containing_func {
	my $s = reverse substr($data,0,$i);
	if ($s =~ /^\s*{\s*\).*?\((\w+)/s) {
		return scalar reverse $1;
	}
	return "";
}

sub containing_func_is_dead {
	return 0 unless defined $foff;
	my $j = $foff;
	while (--$j > 0) {
		last if substr($data, $j, 1) eq ";";
	}
	return substr($data, $j, $foff - $j) =~ /\b__dead\b/;
}

for ($i = 0; $i < length $data; ) {
	if (substr($data, $i, 1) eq "#") {
		# skip preprocessor
		advance(1);
		my $esc = 0;
		for (; $i < length($data); advance(1)) {
			if ($esc) {
				$esc = 0;
			} elsif (substr($data, $i, 1) eq "\\") {
				$esc = 1;
			} elsif (substr($data, $i, 1) eq "\n") {
				last;
			}
		}
		advance(1);
	} elsif (substr($data, $i, 2) eq "/*") {
		# skip multi-line comments
		if (substr($data, $i + 2) =~ m[\*/]) {
			advance($+[0] + 2);
		} else {
			advance(length($data) - $i);
		}
	} elsif (substr($data, $i, 2) eq q{//}) {
		# skip single-line comments
		if (substr($data, $i + 2) =~ m[\n]) {
			advance($+[0] + 1);
		} else {
			advance(length($data) - $i);
		}
	} elsif (substr($data, $i, 1) eq q{"}) {
		# skip strings
		advance(1);
		my $esc = 0;
		for (; $i < length($data); advance(1)) {
			if ($esc) {
				$esc = 0;
			} elsif (substr($data, $i, 1) eq "\\") {
				$esc = 1;
			} elsif (substr($data, $i, 1) eq q{"}) {
				last;
			}
		}
		advance(1);
	} elsif ($lvl == 0 && substr($data, $i) =~ /^[^=]\s*\n{\s*\n/s) {
		# catch routine entrance
		advance($+[0] - 1);
		print "PFL_ENTER();" unless get_containing_func() eq "main";
		advance(1);
		$lvl++;
		$foff = $i;
	} elsif (substr($data, $i) =~ /^return(;\s*}?\s*)/s) {
		# catch 'return' without an arg
		my $end = $1;
		my $len = $+[0];
		$i += $len;
		print "PFL_RETURNX()$end";
		$lvl-- if $end =~ /}/;
	} elsif (substr($data, $i) =~ /^return\s*(\(\s*".*?"\s*\)|".*?")\s*(;\s*}?\s*)/s) {
		# catch 'return' with string literal arg
		my $rv = $1;
		my $end = $2;
		my $len = $+[0];
		$i += $len;
		print "PFL_RETURN_STRLIT($rv)$end";
		$lvl-- if $end =~ /}/;
	} elsif (substr($data, $i) =~ /^return\b\s*(.*?)\s*(;\s*}?\s*)/s) {
		# catch 'return' with an arg
		my $rv = $1;
		my $end = $2;
		my $len = $+[0];
		$i += $len;
		print "PFL_RETURN($rv)$end";
		$lvl-- if $end =~ /}/;
	} elsif ($lvl == 1 && substr($data, $i) =~ /^(?:psc_fatalx?|exit|errx?)\s*\(.*?\)\s*(;\s*}?\s*)/s) {
		my $end = $1;
		advance($+[0]);
		$lvl-- if $end =~ /}/;
	} elsif (substr($data, $i) =~ /^\w+/) {
		advance($+[0]);
	} elsif (substr($data, $i, 1) eq "{") {
		$lvl++;
		advance(1);
	} elsif (substr($data, $i, 1) eq "}") {
		$lvl--;
		if ($lvl == 0) {
			$foff = undef;
			if (substr($data, $i + 1) =~ /^\s*\n/s) {
				# catch implicit 'return'
				print "PFL_RETURNX();" unless containing_func_is_dead();
			}
		}
		advance(1);
	} else {
		advance(1);
	}
}
