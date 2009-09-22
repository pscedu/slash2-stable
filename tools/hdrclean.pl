#!/usr/bin/perl -W
# $Id$

use Getopt::Std;
use strict;
use warnings;

sub usage {
	die "usage: $0 file ...\n";
}

sub errx {
	die "$0: ", @_, "\n";
}

sub err {
	die "$0: ", @_, ": $!\n";
}

my %opts;
getopts("", \%opts) or usage();
usage() unless @ARGV;

my (@funcs, @structs, @typedefs, @defs, @unions, @enums);
my (@srcs, @hdrs);

foreach my $file (@ARGV) {
	if ($file =~ /\.c$/) {
		push @srcs, $file;
	} elsif ($file =~ /\.h$/) {
		push @hdrs, $file;
	} else {
		errx "$file: unknown file type";
	}
}

foreach my $hdr (@hdrs) {
	local $/;
	open HDR, "<", $hdr or err $hdr;
	my $line = <HDR>;
	close HDR;

	1 while $line =~ s/{[^{}]*?}//gs;
	$line =~ s/\\\n//gs;
	$line =~ s/\n\n+/\n/gs;
	$line =~ s!/\*.*?\*/!!gs;
	$line =~ s/^\s*#\s*(?:if|ifdef|ifndef|include|endif|else)\b.*//gm;
	$line =~ s/\n(?=\w+\()/ /gs;

	while ($line =~ s/^\s*#\s*define\s+(\w+).*//m) {
		my $def = $1;
		push @defs, $def unless $def =~ /^_/;
	}

	while ($line =~ s/^\s*struct\s+(\w+)\s*;//m) {
		push @structs, $1;
	}

	while ($line =~ s/^\s*typedef\s+(?:(?:struct|union)\s+)?(\w+)\s+(.*)//m) {
		my $name = $2;
		push @typedefs, $name =~ /(\w+)/;;
	}

	while ($line =~ s/^.*?(\w+)\s*\(\s*[^*].*//m) {
		push @funcs, $1;
	}
}

sub uniq {
	local $_;
	my @uniq;
	my $last = undef;

	foreach (@_) {
		unless (defined $last and $last eq $_) {
			push @uniq, $_;
			$last = $_;
		}
	}
	return @uniq;
}

@funcs = uniq sort @funcs;
@structs = uniq sort @structs;

foreach my $src (@srcs) {
	open SRC, "<", $src or err $src;
	while (defined (my $line = <SRC>)) {
	}
	close SRC;
}
