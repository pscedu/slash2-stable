# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2015-2016, Google, Inc.
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
package PFL::Getoptv;

use Getopt::Std;
use Exporter;
use strict;
use warnings;

our @ISA = qw(Exporter);
our @EXPORT = qw(getoptv);

sub getoptv {
	my ($optstr, $ropts) = @_;
	my @av = @ARGV;

	getopts($optstr, $ropts) or return 0;

	for (my $j = 0; $j < length($optstr); $j++) {
		delete $ropts->{$1}, $j++ if
		    substr($optstr, $j, 2) =~ /^([a-zA-Z0-9]):/;
	}

 ARG:	for (my $i = 0; $i < @av; $i++) {
		my $s = $av[$i];
		next unless $s =~ /^-/;
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
					$i++;
				} else {
					$arg = substr($s, $k + 1);
				}
				$ropts->{$ch} = [] unless exists $ropts->{$ch};
				push @{ $ropts->{$ch} }, $arg;
				next ARG;
			}
		}
	}
	return 1;
}

1;
