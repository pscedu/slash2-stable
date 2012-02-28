#!/usr/local/bin/perl

#[1330381035:740627 sliricthr19:0x801c15400:mem dynarray.c _psc_dynarray_resize 67] alloc()=0x80a952060 sz=8 fl=0
#[1330381035:740843 sliricthr19:0x801c15400:mem dynarray.c _psc_dynarray_resize 67] realloc(0x80a952060)=0x822125d60 sz=16 fl=0
#[1330381035:741191 sliricthr19:0x801c15400:mem dynarray.c psc_dynarray_free 180] free(0x822125d60)

my %h;

while (<>) {
    chomp;
    (@a) = split ' ', $_;

    if ($a[5] =~ /alloc\(\)=(0x[a-f0-9]+)/) {
	#print "alloc $1\n";
	$h{$1} = $1;
	$h{$1}{sz} = $a[6];
	$h{$1}{func} = "$a[3]-" . $a[4];
	$h{$1}{freed} = 0;

    } elsif ($a[5] =~ /realloc\((0x[a-f0-9]+)\)=(0x[a-f0-9]+)/) {
	#print "realloc $1 -> $2\n";
	if (!defined $h{$1}) {
	    $h{$1} = $1;
	}
	    
	$h{$1}{freed} = 1;

    } elsif ($a[5] =~ /free\((0x[a-f0-9]+)\)/) {
	#print "free $1\n";
	if (!defined $h{$1}) {
	    $h{$1} = $1;
	}

	$h{$1}{freed} = 1;
    }
}

foreach $k (keys %h) {
    if ($h{$k}{freed} eq 0) {
	print "$h{$k} $h{$k}{sz} func=$h{$k}{func}\n";
    } else {
	#print "FREED $h{$k} $h{$k}{sz} func=$h{$k}{func} \n";
    }
}
