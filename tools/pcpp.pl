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

use strict;
use warnings;
use PFL::Getoptv;
use File::Basename;

sub usage {
	warn "usage: $0 [-x] file\n";
	exit 1;
}

sub fatal {
	warn "$0: ", @_, "\n";
	exit 1;
}

my %hacks = (
	yytext => 0,
);
my %opts;
getoptv("xH:", \%opts) or usage;
usage unless @ARGV == 1;

if ($opts{H}) {
	foreach my $hack (@{ $opts{H} }) {
		die "$0: invalid hack: $hack" unless exists $hacks{$hack};
		$hacks{$hack} = 1;
	}
}

my $fn = $ARGV[0];
my $host = `uname`;
chomp $host;

open F, "<", $fn or die "$fn: $!\n";
local $/;
my $data = <F>;
close F;

# debug file ID
print qq{# 1 "$fn"\n};

if ($data !~ m!psc_util/log\.h! or
    basename($fn) eq "alloc.c" or
    basename($fn) eq "dynarray.c" or
    basename($fn) eq "hashtbl.c" or
    basename($fn) eq "init.c" or
    basename($fn) eq "lockedlist.c" or
    basename($fn) eq "log.c" or
    basename($fn) eq "subsys.c" or
    basename($fn) eq "thread.c" or
    basename($fn) eq "typedump.c" or
    $opts{x}) {
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
	my $plevel = 0;
	my $j;
	for ($j = $i; $j > 0; $j--) {
		if (substr($data, $j, 1) eq ")") {
			$plevel++;
		} elsif (substr($data, $j, 1) eq "(") {
			if (--$plevel == 0) {
				$j--;
				last;
			}
		}
	}
	my $len = 1;
	$j--, $len++ while substr($data, $j - 1, 1) =~ /[a-zA-Z0-9_]/;
	return substr($data, $j, $len);
}

sub get_func_args {
	my $plevel = 0;
	my ($j, $k, @av);
	for ($j = $i; $j > 0; $j--) {
		if (substr($data, $j, 1) eq "," && $plevel == 1) {
			unshift @av, substr($data, $j + 1, $k - $j);
			$k = $j - 1;
		} elsif (substr($data, $j, 1) eq ")") {
			$k = $j - 1 if ++$plevel == 1;
		} elsif (substr($data, $j, 1) eq "(") {
			if (--$plevel == 0) {
				unshift @av, substr($data, $j + 1, $k - $j) if
				    $k - $j;
				last;
			}
		}
	}
	local $_;
	s/^\s+|\s+$//g for @av;
	@av = () if @av == 1 && $av[0] eq "void";
	for (@av) {
		# void (*foo)(const char *, int)
		unless (s/^.*?\(\s*\*(.*?)\).*/$1/s) {
			# __unusedx const char *foo[BLAH]
			s/\[.*//;
			s/^.*?(\w+)$/$1/;
		}
	}
	pop @av if @av > 1 && $av[$#av] eq "...";
	return @av;
}

sub containing_func_is_dead {
	return 0 unless defined $foff;
	my $j = $foff;
	while (--$j > 0) {
		last if substr($data, $j, 1) eq ";";
	}
	return substr($data, $j, $foff - $j) =~ /\b__dead\b/;
}

sub dec_level {
	$lvl--;
	fatal "$lvl < 0" if $lvl < 0;
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
	} elsif (substr($data, $i, 1) =~ /['"]/) {
		my $ch = $&;
		# skip strings
		advance(1);
		my $esc = 0;
		for (; $i < length($data); advance(1)) {
			if ($esc) {
				$esc = 0;
			} elsif (substr($data, $i, 1) eq "\\") {
				$esc = 1;
			} elsif (substr($data, $i, 1) eq $ch) {
				last;
			}
		}
		advance(1);
	} elsif ($lvl == 0 && substr($data, $i) =~ /^[^=]\s*\n{\s*\n/s) {
		# catch routine entrance
		advance($+[0] - 1);
		unless (get_containing_func() eq "main") {
			print qq{psc_trace("enter};
			my @args = get_func_args();
			my $endstr = "";
			foreach my $arg (@args) {
				print " $arg=%p:%ld";
				$endstr .= ", (void *)(unsigned long)$arg, (long)$arg";
			}
			print qq{"$endstr);};
		}
		advance(1);
		$lvl++;
		$foff = $i;
	} elsif (substr($data, $i) =~ /^return(\s*;\s*}?\s*)/s) {
		# catch 'return' without an arg
		my $end = $1;
		my $len = $+[0];
		$i += $len;
		print "PFL_RETURNX()$end";
		dec_level() if $end =~ /}/;
	} elsif (substr($data, $i) =~ /^return(\s*(?:\(\s*".*?"\s*\)|".*?"))(\s*;\s*}?\s*)/s) {
		# catch 'return' with string literal arg
		my $rv = $1;
		my $end = $2;
		my $len = $+[0];
		$i += $len;
		print "PFL_RETURN_STR($rv)$end";
		dec_level() if $end =~ /}/;
	} elsif (substr($data, $i) =~ /^return(\s*(?:\(\s*\d+\s*\)|\d+))(\s*;\s*}?\s*)/s) {
		# catch 'return' with numeric literal arg
		my $rv = $1;
		my $end = $2;
		my $len = $+[0];
		$i += $len;
		print "PFL_RETURN_LIT($rv)$end";
		dec_level() if $end =~ /}/;
	} elsif (substr($data, $i) =~ /^return\b(\s*.*?)(\s*;\s*}?\s*)/s) {
		# catch 'return' with an arg
		my $rv = $1;
		my $end = $2;
		my $len = $+[0];
		$i += $len;

		$rv = "(PCPP_STR($rv))" if $rv =~ /^\s*\(?\s*yytext\s*\)?\s*$/ && $hacks{yytext};

		my $tag = "PFL_RETURN";
		if ($rv =~ /^\s*\(\s*PCPP_STR\s*\((.*)\)\s*\)$/) {
			$rv = $1;
			$tag = "PFL_RETURN_STR";
		}

		$rv = "((void *)NULL)" if
		    $rv =~ /^\s*\(NULL\)\s*$/ and $host eq "OpenBSD";

		print "$tag($rv)$end";
		dec_level() if $end =~ /}/;
	} elsif ($lvl == 1 && substr($data, $i) =~ /^(?:psc_fatalx?|exit|errx?)\s*\([^;]*?\)\s*;\s*}\s*/s) {
		# XXX this pattern skips psc_fatal("foo; bar")
		# because of the embedded semi-colon

		# skip no return conditions
		advance($+[0]);
		dec_level();
	} elsif ($lvl == 1 && substr($data, $i) =~ /^goto\s*\w+\s*;\s*}\s*/s) {
		# skip no return conditions
		advance($+[0]);
		dec_level();
	} elsif ($lvl == 1 && substr($data, $i) =~ m[^\s*/\*\s*NOTREACHED\s*\*/\s*}\s*]s) {
		# skip no return conditions
		advance($+[0]);
		dec_level();
	} elsif (substr($data, $i) =~ /^\w+/) {
		advance($+[0]);
	} elsif (substr($data, $i, 1) eq "{") {
		$lvl++;
		advance(1);
	} elsif (substr($data, $i, 1) eq "}") {
		dec_level();
		if ($lvl == 0) {
			if (substr($data, $i + 1) =~ /^\s*\n/s) {
				# catch implicit 'return'
				print "PFL_RETURNX();" unless containing_func_is_dead();
			}
			$foff = undef;
		}
		advance(1);
	} else {
		advance(1);
	}
}
