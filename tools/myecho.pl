#!/usr/bin/perl -W
# $Id$

use Getopt::Std;
use strict;
use warnings;

sub usage {
}

my %opts;
getopts("ne", \%opts) or usage;

$, = " ";
print @ARGV;

print "\n" unless $opts{n};
