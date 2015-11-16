#!/usr/bin/perl -W
# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2015, Google, Inc.
# Copyright 2010-2015, Pittsburgh Supercomputing Center
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

# mdproc - Preprocessor for mdoc manual pages, doing things such as
# expanding templates and updating dates.

# Better algorithm:
#	use file mtime as long as mtime != ctime
#	and svn diff -I\$Id yields isn't empty
# XXX if file was copied from a revision, switch to that file and
#	continue backwards

use strict;
use warnings;
use PFL::Getoptv;

sub usage {
	warn "usage: $0 [-D var=value] file ...\n";
	exit 1;
}

my %opts;
getoptv("D:", \%opts) or usage;
my %incvars = map { die "bad variable format: $_" unless /=/; ($`, $') } @{ $opts{D} };

my @m = qw(0 January February March April May June July
    August September October November December);

my %m;
@m{@m} = (1 .. @m);

sub get_unless_last_rev {
	my ($fn, $d, $m, $y) = @_;

	my @out = `git log --date=short --pretty=format:%ad \Q$fn\E`;
	my $ln = shift @out;
	my ($yr, $mon, $day) = $ln =~ /^(\d+)-0*(\d+)+-0*(\d+)$/ or return;
	return ($day, $mon, $yr);
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
	my @out = `git status --porcelain \Q$fn\E`;
	my $data;

	if (@out) {
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

	my $T = qr[^\.\\"\s*]m;

	# parse MODULES
	my %mods;
	if ($data =~ /$T%PFL_MODULES\s*(.*)\s*%/m) {
		my @mods = split /\s+/, $1;
		@mods{@mods} = (1) x @mods;
	}

	my $DATA = { };

	sub eval_data {
		my ($str) = @_;
		my @c = grep { /$T/ } split /(?<=\n)/, $str;
		my @t = eval join '', map { /$T/; $' } @c;
		warn "error in eval: $@\n@c\n-----------------------\n"
		    if $@;
		return (\@c, @t);
	}

	sub expand_include {
		my ($fn, $p) = @_;
		$fn =~ s/\$(\w+)/$incvars{$1}/ge;

		open INC, "<", $fn or die "$fn: $!\n";
		my @lines = <INC>;
		close INC;

		shift @lines if $lines[0] =~ /$T\$Id/;

		my ($code, %av) = eval_data($p);
		@{$DATA}{keys %av} = values %av;
		return join '', @$code, @lines;
	}

	# process includes
	$data =~ s/($T%PFL_INCLUDE\s+(.*?)\s*{\n)(.*?)($T}%)/
	    $1 . expand_include($2, $3) . $4/gems;

	sub build_list {
		my %k = @_;
		my $str = "";
		my $mac = "Cm";
		if (exists $k{MACRO}) {
			$mac = $k{MACRO} if $k{MACRO};
			delete $k{MACRO};
		}

		foreach my $k (sort keys %k) {
			$str .= ".It ";
			$str .= "Xo\n.Sm off\n." if $k =~ /\n/;
			$str .= "$mac $k\n";
			$str .= ".Sm on\n.Xc\n" if $k =~ /\n/;
			$str .= $k{$k};
			$str .= "\n" if $k{$k} && $str !~ /\n$/;
		}
		return $str;
	}

	sub expand_list {
		my $mac = shift;
		my (undef, %k) = eval_data($_[0]);
		return build_list(MACRO => $mac, %k);
	}

	# process lists
	$data =~ s/$T%PFL_LIST\s*(\w*)\s*{\n(.*?)$T}%\n/expand_list($1, $2)/gems;

	sub expand_expr {
		my (undef, @t) = eval_data($_[0]);
		return join('', @t) . "\n";
	}

	# process expressions
	$data =~ s/$T%PFL_EXPR\s*{\n(.*?)$T}%\n/expand_expr($1)/gems;

	# overwrite
	open F, ">", $fn or die "$fn: $!\n";
	print F $data;
	close F;
} continue {
	system("groff -z $fn");
}
