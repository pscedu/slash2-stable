#!/usr/bin/env perl
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

use strict;
use warnings;
use PFL::Getoptv;
use File::Basename;

sub usage {
	warn "usage: $0 [-fx] [-H hack] file\n";
	exit 1;
}

sub fatal {
	warn "$0: ", @_, "\n";
	exit 1;
}

my %hacks = (
	yyerrlab	=> 0,
	yylex_return	=> 0,
	yysyntax_error	=> 0,
	yytext		=> 0,
);
my %opts;
getoptv("fH:x", \%opts) or usage;
usage unless @ARGV == 1;

if ($opts{H}) {
	foreach my $hack (@{ $opts{H} }) {
		die "$0: invalid hack: $hack" unless exists $hacks{$hack};
		$hacks{$hack} = 1;
	}
}

my $fn = $ARGV[0];

open F, "<", $fn or die "$fn: $!\n";
local $/;
my $data = <F>;
close F;

# debug file ID
print qq{# 1 "$fn"\n};

if (!$opts{f} and (
    basename($fn) eq "alloc.c" or
    basename($fn) eq "dynarray.c" or
    basename($fn) eq "hashtbl.c" or
    basename($fn) eq "init.c" or
    basename($fn) eq "lib-move.c" or
    basename($fn) eq "lockedlist.c" or
    basename($fn) eq "log.c" or
    basename($fn) eq "subsys.c" or
    basename($fn) eq "thread.c" or
    basename($fn) eq "typedump.c" or
    basename($fn) eq "waitq.c" or
    $opts{x})) {
	print $data;
	exit 0;
}

my $pfl = $data =~ m!pfl/log\.h! || $opts{f};

my $i;
my $lvl = 0;
my $foff;
my $linenr = 0;
my $pci;
my $pci_sufx = "";

sub advance {
	my ($len) = @_;

	my $str = substr($data, $i, $len);
	print $str;
	my @m = $str =~ /\n/g;
	$linenr += @m;
	$i += $len;
}

sub get_containing_func {
	my $plevel = 0;
	my $j;
	for ($j = $foff; $j > 0; $j--) {
		if (substr($data, $j, 1) eq ")") {
			$plevel++;
		} elsif (substr($data, $j, 1) eq "(") {
			if (--$plevel == 0) {
				$j--;
				last;
			}
		}
	}
	$j-- while substr($data, $j, 1) =~ /\s/;
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
	$pci = @av > 0 && $av[0] eq "const struct pfl_callerinfo *pci";
	if ($pci) {
		shift @av if $pci;
		$pci_sufx = "_PCI";
	} else {
		$pci_sufx = "";
	}
	for (@av) {
		# void (*foo)(const char *, int)
		unless (s/^.*?\(\s*\*(.*?)\).*/$1/s) {
			# __unusedx const char *foo[BLAH]
			s/\[.*//s;
			s/^.*?(\w+)$/$1/s;
		}
	}
	pop @av if @av > 1 && $av[$#av] eq "...";
	return @av;
}

sub get_containing_tag {
	my $blevel = 1;
	my ($j);
	for ($j = $i - 1; $j > 0; $j--) {
		if (substr($data, $j, 1) eq '"') {
			for ($j--; $j > 0; $j--) {
				last if substr($data, $j, 1) eq '"' and
				    substr($data, $j - 1, 1) ne "\\";
			}
		} elsif (substr($data, $j, 1) eq "'") {
			for ($j--; $j > 0; $j--) {
				last if substr($data, $j, 1) eq "'" and
				    substr($data, $j - 1, 1) ne "\\";
			}
		} elsif (substr($data, $j, 1) eq "}") {
			$blevel++;
		} elsif (substr($data, $j, 1) eq "{") {
			if (--$blevel == 0) {
				my $k;
				for ($j--; $j > 0; $j--) {
					last if substr($data, $j, 1) !~ /\s/;
				}
				for ($k = $j; $k > 0; $k--) {
					last if substr($data, $k-1, 1) !~ /[a-z0-9_]/i;
				}
				return substr($data, $k, $j - $k + 1);
			}
		}
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

sub dec_level {
	$foff = undef unless --$lvl;
	fatal "brace level $lvl < 0 at $linenr: $ARGV[0]" if $lvl < 0;
}

sub count_newlines {
	my ($s) = @_;
	return "/*$s*/" if $s =~ /\b(?:NOTREACHED|FALLTHROUGH)\b/;
	return join '', $s =~ /\\?\n/g;
}

$data =~ s{/\*(.*?)\*/}{ count_newlines($1) }egs;

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
	} elsif ($lvl == 0 && substr($data, $i) =~ /^([^=]\s*\n)({\s*\n)/s) {
		my $plen = $+[1];
		my $slen = $+[2] - $-[2];

		# catch routine entrance
		warn "nested function?\n" if defined $foff;
		$foff = $i;
		my @args = get_func_args();
		advance($plen);
		advance($slen - 1);
		print qq{_PFL_START_PCI(pci); } if $pci;
		unless (get_containing_func() eq "main" or !$pfl) {
			print qq{psclog_trace("enter %s};
			my $endstr = "";
			foreach my $arg (@args) {
				print " $arg=%p:%ld";
				$endstr .= ", (void *)(unsigned long)$arg, (long)$arg";
			}
			print qq{", __func__$endstr);};
		}
		advance(1);
		$lvl++;
	} elsif (substr($data, $i) =~ /^return(\s*;\s*}?)/s) {
		# catch 'return' without an arg
		my $end = $1;
		$i += $-[1];
		my $elen = $+[1] - $-[1];
		if ($pfl) {
			print "PFL_RETURNX$pci_sufx()";
		} elsif ($pci) {
			print "_PFL_RETURN_PCI()";
		} else {
			print "return";
		}
		advance($elen);
		dec_level() if $end =~ /}/;
	} elsif (substr($data, $i) =~ /^return(\s*(?:\(\s*".*?"\s*\)|".*?"))(\s*;\s*}?)/s) {
		# catch 'return' with string literal arg
		my $rv = $1;
		my $end = $2;
		$i += $-[1];
		my $rvlen = $+[1] - $-[1];
		my $elen = $+[2] - $-[2];
		if ($pfl) {
			print "PFL_RETURN_STR$pci_sufx("
		} elsif ($pci) {
			print "_PFL_RETURN_PCI(";
		} else {
			print "return (";
		}
		advance($rvlen);
		print ")";
		advance($elen);
		dec_level() if $end =~ /}/;
	} elsif (substr($data, $i) =~ /^return\b(\s*(?:\(\s*\d+\s*\)|\d+))(\s*;\s*}?)/s) {
		# catch 'return' with numeric literal arg
		my $rv = $1;
		my $end = $2;
		$i += $-[1];
		my $rvlen = $+[1] - $-[1];
		my $elen = $+[2] - $-[2];
		if ($pfl) {
			print "PFL_RETURN_LIT$pci_sufx(";
		} elsif ($pci) {
			print "_PFL_RETURN_PCI(";
		} else {
			print "return (";
		}
		advance($rvlen);
		print ")";
		advance($elen);
		dec_level() if $end =~ /}/;
	} elsif (substr($data, $i) =~ /^return\b(\s*.*?)(\s*;\s*}?)/s) {
		# catch 'return' with an arg
		my $rv = $1;
		my $end = $2;
		$i += $-[1];
		my $rvlen = $+[1] - $-[1];
		my $elen = $+[2] - $-[2];
		my $skiplen = 0;

		my $tag = "PFL_RETURN";
		if ($rv =~ /^\s*\(\s*PCPP_STR\s*\((.*)\)\s*\)$/) {
			$i += $-[1];
			$rvlen -= $-[1];
			$skiplen = $+[0] - $+[1];
			$rvlen -= $skiplen;
			$tag = "PFL_RETURN_STR";
		} elsif ($rv =~ /^\s*\(?\s*yytext\s*\)?\s*$/ && $hacks{yytext}) {
			$tag = "PFL_RETURN_STR";
		}

		if ($pfl) {
			print "$tag$pci_sufx(";
		} elsif ($pci) {
			print "_PFL_RETURN_PCI(";
		} else {
			print "return (";
		}
		advance($rvlen);
		print ")";
		$i += $skiplen;
		advance($elen);
		dec_level() if $end =~ /}/;
	} elsif ($lvl == 1 && substr($data, $i) =~ /^(?:psc_fatalx?|exit|errx?)\s*\([^;]*?\)\s*;\s*}/s) {
		# XXX this pattern skips psc_fatal("foo; bar")
		# because of the embedded semi-colon

		# skip no return conditions
		advance($+[0]);
		dec_level();
	} elsif ($lvl == 1 && substr($data, $i) =~ /^goto\s*\w+\s*;\s*}/s) {
		# skip no return conditions
		advance($+[0]);
		dec_level();
	} elsif ($lvl == 1 && substr($data, $i) =~ m[^\s*/\*\s*NOTREACHED\s*\*/\s*}]s) {
		# skip no return conditions
		advance($+[0]);
		dec_level();
	} elsif ($hacks{yyerrlab} && $lvl == 1 && substr($data, $i) =~ /^\n\s*yyerrlab:\s*$/m) {
		advance($+[0]);
		print "if (0) goto yyerrlab;";
		$hacks{yyerrlab} = 0;
	} elsif (substr($data, $i) =~ /^\w+/) {
		advance($+[0]);
	} elsif (substr($data, $i, 1) eq "{") {
		$lvl++;
		advance(1);
	} elsif (substr($data, $i, 1) eq "}") {
		if ($lvl == 1 && defined $foff) {
			if (substr($data, $i + 1) =~ /^\s*\n/s) {
				# catch implicit 'return'
				for (;;) {
					last if containing_func_is_dead;
					last if $hacks{yysyntax_error} and
					    get_containing_func eq "yysyntax_error";
					last if $hacks{yylex_return} and
					    get_containing_tag eq "YY_DECL";
					if ($pfl) {
						print "PFL_RETURNX$pci_sufx();";
					} elsif ($pci) {
						print "_PFL_END_PCI();";
					}
					last;
				}
			}
		}
		advance(1);
		dec_level();
	} else {
		advance(1);
	}
}
