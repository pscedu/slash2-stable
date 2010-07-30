# $Id$
# %PSC_COPYRIGHT%
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

1;
