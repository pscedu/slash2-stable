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

use POSIX qw(:errno_h);
use IPC::Open3;
use Symbol;
use strict;
use warnings;

sub fatal {
	# Prepend a newline to leave the "(notgdb)" prompt.
	die "\n", $@, ": $!\n";
}

sub slurp {
	my ($fh) = @_;
	my $rd_set = 0;
	my $off = 0;
	my ($data, $rv);

	vec($rd_set, fileno($fh), 1) = 1;
	while (select $rd_set, undef, undef, 0) {
		$rv = sysread $fh, $data, 4096, $off;
		fatal "sysread" if $rv == -1;
		last if $rv == 0;
		$off += $rv;
	}
	return undef unless $data and $rv;
	return ($data);
}

die "usage: $0 program [arg ...]\n" unless @ARGV;

my $prog = shift;

$| = 1;

my $sigint = 0;
my $rc = 1;
my $infh;
my $outfh;
my $pid = open3($infh, $outfh, 0, "gdb -q -f $prog");
open TTY, "<", "/dev/tty" or die "/dev/tty: $!\n";

my @cmds = (
	"set prompt (gdb)\\n",
	"set annotate 3",
	"set height 0",
	"set confirm off",
	"run @ARGV",
);

$SIG{INT} = sub { $sigint = 1; };

for (;;) {
	my $rd_set = 0;
	vec($rd_set, fileno(TTY), 1) = 1;
	vec($rd_set, fileno($outfh), 1) = 1;
	my $er_set = $rd_set;
	my $nfd = select($rd_set, undef, $er_set, undef);

	if ($sigint) {
		$sigint = 0;
		kill "SIGINT", $pid or warn "kill $pid: $!\n";
	}

	if ($nfd == -1) {
		# Probably from a signal
		fatal "select" unless $! == EINTR;
		next;
	}

	my ($data, $ln, @lns);
	if (vec($rd_set, fileno(TTY), 1)) {
		$data = slurp(*TTY);
		unless (defined $data) {
			print "^D\n";
			last;
		}
		print $infh $data or last;
	} elsif (vec($rd_set, fileno($outfh), 1)) {
		$data = slurp($outfh);
		defined $data or last;

		my $do_cmd = 0;
		@lns = split /\n/, $data;
		foreach $ln (@lns) {
			if (($ln =~ /^\032\032exited (\d+)/ ||
			    $ln =~ /Program exited with code (\d+)/) &&
			    ($rc = $1) == 0) {
				push @cmds, "quit";
				$do_cmd = 1;
				next;
			}
			if ($ln =~ m!^\(gdb\)!) {
				$do_cmd = 1;
				next;
			}
			next if $ln =~ /^\s*$/;
			print "$ln\n" unless $ln =~ /^\032\032/;
		}

		if ($do_cmd) {
			print "(notgdb) ";
			if (@cmds) {
				my $cmd = shift @cmds;
				print "$cmd\n";
				print $infh "$cmd\n";
			}
		}
	} elsif ($er_set) {
		last;
	}
}
close $infh;
close $outfh;
close TTY;

wait;

exit $rc;
