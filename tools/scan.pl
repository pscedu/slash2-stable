#!/usr/bin/env perl
#
# File: scan.pl, only partial names are printed to keep all information of a file on
#                one line for easy processing with shell commands such as sort(1).
#                For the same reason, other file attributs such as size is not listed.
#
# Initial Date: 09/12/2012
#
use warnings;
use strict;
use Digest::MD5;
use Cwd;
use File::Basename;

sub md5sum {
    my $file = shift;
    my $digest;

    # Perl stops on first error in the eval block and returns error code in $@.
    eval {
        open(FILE, $file);
        my $ctx = Digest::MD5->new;
        $ctx->addfile(*FILE);
        $digest = $ctx->hexdigest;
        close(FILE);
    };
    if ($@) {
        # md5sum can't handle files with huge sizes (EiB) 
        $digest = "--------------------------------";
    }
    return $digest;

}

sub ScanDirectory {
    my $workdir  = shift; 
    my $loghandle = shift;
    my $enterdir = &cwd;

    chdir($workdir) or die "\nUnable to enter dir $workdir: $!\n";

    opendir(DIR, ".") or die "Unable to open $workdir: $!\n";
    my @names = readdir(DIR);
    closedir(DIR);
 
    foreach my $name (@names){
        next if ($name eq "."); 
        next if ($name eq "..");

        my $md5;
        my @stat = stat($name);
        my $inode = $stat[1];
        my $namestring = &cwd."/".$name;

        if (-d $name) {
           $md5 = "                                ";
        } else {
            $md5 = md5sum($name);
        }

        # print the last 60 characters of each pathname.

        if (length($namestring) > 60) {
            $namestring = substr($namestring, -60, 60);
            $namestring = "... " . $namestring;
        }

        printf $loghandle "%20s  %s  %s\n", $inode, $md5, $namestring;
        if (-d $name) {
            ScanDirectory($name, $loghandle);
        }
    }
    chdir($enterdir) or die "\nUnable to enter dir $enterdir: $!\n";
}

my $startdir;
my $logname;
my $loghandle;

my $argcnt = $#ARGV + 1;
my $overwrite;

my $start;
my $stop;
my $start_time;
my $stop_time;

if ($argcnt != 1 && $argcnt != 2) {
    die "Usage: scan.pl [-f] directory\n";
}

if ($argcnt == 1) {

  $overwrite = 0;
  $startdir = $ARGV[0];
  $logname = $ARGV[0];

} else {

    if ($ARGV[0] ne "-f") {
        die "Usage: scan.pl [-f] directory\n";
    }

    $overwrite = 1;
    $startdir = $ARGV[1];
    $logname = $ARGV[1];

}

# Remove any starting and trailing slash(es).

# $logname =~ s/\/$//;
# $logname =~ s/^\///;

if ($logname eq ".") {
    $logname = &cwd; 
}
$logname = basename($logname);

my($day, $month, $year)=(localtime)[3,4,5];

$year += 1900;
$month += 1;

$logname = $logname . "-" . $month. "-". $day . "-" . $year . ".list";
if (-e $logname) {
    if ($overwrite != 1) {
        die "Oops! A file called '$logname' already exists.\n" 
    }
}

print "Results will be saved in file $logname\n";

open($loghandle, ">$logname") or die "\nCan't open file $logname: $!\n";

$start_time = time;
$start = localtime;
print $loghandle "Script started on $start. ";
print $loghandle "Filename: $logname\n\n";

&ScanDirectory($startdir, $loghandle);

$stop_time = time;
$stop = localtime;
print $loghandle "\nScript stopped on $stop. ";

my $diff = $stop_time - $start_time;
 
printf $loghandle "Total elapsed time %02d:%02d:%02d.\n", int($diff/3600), int(($diff%3600)/60), int($diff%60);
printf "Total elapsed time %02d:%02d:%02d.\n", int($diff/3600), int(($diff%3600)/60), int($diff%60);

exit 0;
