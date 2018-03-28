11/17/2017
----------

Get rid of CRC update RPC and also rework slab reap locking.  This cuts down the
CPU usage and significantly reduce the key-expired message seen on a slow sliod.
The bigfile.sh test time drops from 28 hours to 20 hours on the slow sliod.

10/05/2017
----------

Now pages can be cached longer than a bmap's lifetime, also get rid of the global
lock of LRU lists.  As a result, I got big performance improvement. 

07/19/2017
----------

Odtable cleanup/unification/simplification (get rid of receipt allocation). Fix
a bmap sequence number invalid corner case.  Some pool locking clean up.

07/03/2017
----------

We now support partial truncation.

06/07/2017
----------

Merge bits from development trees: clean up per-thread memory usage, etc.

05/30/2017
----------

Name space rewrite.  We now support use-after-unlink.

04/07/2017
----------

Release 1.18:

A bunch of good fixes went in, including: 15% improvement replicating a kernel tree, disable write
data corruption fix and enhancement, UID/GID mapping do-over, crashes, hangs, etc.

01/20/2017
---------

Merge bits from development trees after some testing (including bigfile.sh).

01/10/2017
----------

Release 1.17: new replication engine mostly in place and a subtle data corruption fix (NULL bytes).

10/26/2016
----------

zhihui@illusion2: /local/src/p$ slmctl -sop | egrep "rate|repl-s"
opstat                                          avg rate      max rate      cur rate         total
repl-sched                                       33.1M/s        4.6G/s       13.5M/s         75.1T
repl-schedwk-err                                     0/s          17/s           0/s           736
repl-schedwk-ok                                     18/s         167/s          20/s       4684949
repl-success                                        18/s         167/s          20/s       4684949
repl-success-aggr                                52.8M/s        3.8G/s       16.7M/s         75.0T
zhihui@illusion2: /local/src/p$ slmctl -p sys | tail -2
sys.uptime                                              5d21h44m
sys.version                                             42659
zhihui@illusion2: /local/src/p$ 

10/18/2016
----------

Rlease 1.16.

At this change, replication seems to be stable and more efficient than before. Many bugs in
batch RPC code have been fixed.  All my tests has passed except one possible regression:

bash-4.2# msctl -sc
resource                                      host type    flags stkvers txcr  #ref      uptime
===============================================================================================
PSC1
  orange                            orange.psc.edu mds      -OW-   16346    8    10    0d19h55m
  lemon                              lemon.psc.edu serial   ----   16346    8     1    --------
  lime                                lime.psc.edu serial   -O--   16346    8     1    0d22h42m

The refernce count should be one.  However, it is possible that I have never tested the case that
produces the reference count leak.

10/07/2016
----------

Some incremental improvements of replication performance.

09/23/2016
----------

More busy locking fallout to deal with the same deadlock (fcmh BUSY and bmap LOADING).

09/22/2016
----------

Fix more fallout due to fcmh BUSY clean up, including a deadlock in replication code.
Also better replication accounting.

09/17/2016
----------

Partial reclaim should be working. For example:

$ msctl repl-remove:lemon@PSC1:5-7 /zzh-slash2/zhihui/bigfile.txt 

Also clean up the FCMH BUSY business.

08/01/2016
----------

Release 1.15

Another batch of changes, including a workaround/fix on ZFS ARC code, disable invalidation
of dentry on the client, and batch RPC improvements/fixes.

06/12/2016
----------

Release 1.14.  I did all the test, including the checklist, for this release.

06/07/2016
----------

Start a check list before pushing to the -stable tree (in addition to 4 parallel tests). The
list is currently as follows:

	1. Build sliod on FreeBSD 9.0.

	2. If zfs-fuse is changed, try to create a new ZFS pool.

	3. Check out two slash2 source trees, one in slash2, the other in a different file system.
	   Then do a recursive diff -dur -x .git comparison.  This can catch readlink regressions.

	4. Check out a slash2 source tree on one IOS, replicate it to another IOS. Then take the
           first IOS offline and build the source tree. This makes sure that msctl functionality
	   and basic replication work.

	   It also makes sure that the client can skip the down IOS quickly.

	5. Force MDS to always return DIO bmap lease (sys.force_dio=1) and do a self build.

	   self-build non DIO mode:

		real	3m46.075s
		user	4m42.471s
		sys	1m33.065s

	   self-build DIO mode:

		real	4m55.931s
		user	4m45.384s
		sys	1m35.727s

	   Also make sure that this test finishes within 4 minutes to detect possible performance
	   regression.

	6. After all tests are done, run slmctl -sc and check the reference count of resources to
	   make sure there are no leaks.

06/03/2016
----------

Two major changes: (1) rework csvc handling code (2) enhance slmctl -s thread output.

05/19/2016
----------

zhihui@illusion2: ~$ slmctl -p sys | tail -2
sys.uptime                                     2d18h29m
sys.version                                    41063

[zhihui@br011 ~]$ slmctl -p sys | tail -2
sys.uptime                                     38d5h59m
sys.version                                    40977

05/15/2016
----------

Major changes: (1) Rewrite the way slash2 client allocates 32K buffers, this greatly reduces
RSS usage. (2) Fix an old bug in which we use random lock to send message to socket. This
breaks a simple replication query command. (3) Other small fixes and improvments (EIO can 
cause endless loop, add uptime in msctl -sc).

My current practice to push to this stable tree (update 06/10/2016):

	- Linux kernel 4.2 build (make -j 3)
	- iozone -a in a loop
	- tar-ssh in a loop
	- self build in a loop

All tests are driven manually. It usually finishes in 10+ hours (the kernel build lasts longest). Then
I remove all files with rm -rvf *, which takes 30+ minutes. After that, I repeat the above 4 parallel
tests and let them run for a couple of hours.  Then, I kill all tests and wipe out all files again with 
rm -rvf *.  Finally, I run though my check list (documented on 06/07/2016).

05/11/2016
----------

If I turn on PICKLE_HAVE_FSANITIZE and do the following on an empty file

zhihui@yuzu: ~/projects-yuzu/slash2/msctl$ ./msctl -R repl-status  /zzh-slash2/zhihui/bbbb 

It crashes on the following line:

295         spinlock(&mrsq.mrsq_lock);
296         while (mrsq.mrsq_rc == 0) {
297                 psc_waitq_wait(&mrsq.mrsq_waitq, &mrsq.mrsq_lock);  <-- HERE
298                 spinlock(&mrsq.mrsq_lock);
299         }

This is not due to stack size. It is broken since v1.13.  Maybe it is due to the use of -R?

	05/15/2016: This bug is fixed, it is due to misuse of socket lock by non-controller 
	threads.

05/01/2016
----------

Pull in some thread code changes in an effort to reduce RSS.  Add sys.rss.  I 
did testing for a long time:

bash-4.2# msctl -p sys.uptime
parameter                                      value
================================================================================
sys.uptime                                     0d12h45m


04/25/2016
----------

Release 1.13.

04/18/2016
----------

I tried to use slash2-merge.sh to add mfio tree. The files end up at
the top directory.  I had to revert the merge with the following.

 $ git revert -m 1 dd73378cbcf888367c05ef891104ca6022a50efc
 $ git push

Then I use the steps in slash2-build.sh to re-do the merge.

04/12/2016
----------

Move v1.12 tag as follows:

	$ git tag -d v1.12
	$ git push --delete origin v1.12
	$ git tag -a v1.12 -m "Release 1.12"
	$ git push --tags

04/12/2016
----------

Release 1.12.  The highlights of this release are:

	* batch RPC rework
	* Client side RPC retry rework
	* ENOMEM fix for zfs-fuse on the MDS
	* Mininum space reservation on the IOS
	* Miscellaneous fixes and improvements.

03/18/2016
----------

Release 1.11.

03/16/2016
----------

All client RPCs are throttled. Fix a MDS crash, adjust timeout for bulk RPC, etc.

03/14/2016
----------

I did 5 hours of four way testing on a client before the following commit:

commit 0da7d49befa8fe89d4a36449fbea8eb6353208a5
Author: Jared Yanovich <jaredy@google.com>
Date:   Mon Mar 14 17:54:04 2016 -0400

    i think this might be the the elusive csvc ref leak

03/09/2016
----------

The default log level is currently broken.  Also I see "tar: Exiting with failure status due to 
previous errors" with tar-ssh over a large kernel build tree (with objects in it).  But this push 
contains a lease sequence number fix that should make the following test work much better:

	$ iozone -u 4 -l 1 -i 0 -i 1 -r 256 -s 10240000 -b result -e

I did pass the above test two in a row on my test fruits.

03/07/2016
----------

Release 1.10.

03/04/2016
----------

In addition to four-way four-hour test, I also replicate two slash2 source trees and build them at
the target IOS.

02/17/2016
----------

Release 1.09. Jared's test suite is code complete. A race condition in the name cache code is fixed.
The low water mark (minseq) issue after MDS restart is fixed. A workaround of ACL issue is in. Now 
a pool max of zero means infinity.  I will tag the commit with the following command:

	$ git tag -a v1.09 -m "Release 1.09"
	$ git push --tags

02/08/2016
----------

Release 1.08. Ajdust various pool sizes on MDS, sliod, and mount_slash. Adjust ZFS ARC size on slashd. I
use two clients, each of them runs my four-way multiple hour tests. I was able to remove 2+ million
files.

01/28/2016
----------

Release 1.07. This release contains a potential fix that solves the ZFS-fuse umem/vmem allocation
failure when we try to send a full snapshot on a MDS. Also, slash2.so is now compiled by default.

01/20/2016
----------

Release 1.06. The main motivation is to disable partial truncation completely before it
is ready.  Also pick up a fix in the flapping detector in pfl_daemon.sh and a potential
fix for live update with wokfs.

01/16/2016
----------

Release 1.05.  I was able to use wokfs to start a slash2 client.  However, live update
seems to have issues - don't use it at least for now. Here is how I used wokfs:

bash-4.2# SLASH_MDS_NID="orange@PSC1" PREF_IOS="lemon@PSC1" gdb mount_wokfs -q 
Reading symbols from /home/zhihui/projects-yuzu/wokfs/mount_wokfs/mount_wokfs...done.
(gdb) r -L "insert 0 /home/zhihui/projects-stable/slash2/mount_slash/slash2.so slcfg=/home/zhihui/two-ios.conf,datadir=/var/lib/slash2-zhihui" -U /zzh-slash2 2> log.txt

01/15/2016
----------

zhihui@yuzu: ~/projects-stable-yuzu-2016-01-15$ cat slash2/mk/local.mk 
SLASH_OPTIONS+=module

01/13/2016
----------

Release 1.04.  We update two slash2 system with this bits.

01/09/2016
----------

Release 1.03

I did eight hours of 4-way parallel tests over the span of two days.  This release includes
a test suite, two replication fixes, initial work on partial truncation, and other fixes.

12/19/2015
----------

I did two kernel 4.2 compile (make -j 3) in parallel without any problem:

real    218m55.008s
user    54m44.695s
sys     11m17.265s

12/12/2015
----------

Merge development git trees. Fix some minor journal issues and a MDS crash.  Now MDS can
create a default bmap on-disk table if it does not exist.
  
12/11/2015
----------

Command I use to compare the stable tree and the sum of development trees:

zhihui@yuzu: ~$ diff -dru -x .git projects-yuzu-2015-12-11/ projects-stable-yuzu-2015-12-11/
Only in projects-stable-yuzu-2015-12-11/: KNOWN-ISSUES.txt
Only in projects-stable-yuzu-2015-12-11/: README.stable
Only in projects-stable-yuzu-2015-12-11/: slash2-merge.sh
Only in projects-stable-yuzu-2015-12-11/: slash2-update.sh

12/07/2015
----------

Merge development git trees. The big feature included in this merge is continuous 
development.

11/09/2015
----------

The slash2-stable git tree is a merge of of the following five git trees:

	* https://github.com/pscedu/pfl
	* https://github.com/pscedu/slash2
	* https://github.com/pscedu/zfs-fuse
	* https://github.com/pscedu/sft
	* https://github.com/pscedu/distrib.fuse

Only the first three trees are essential for slash2 software.

The goals of this git tree are as follows:

	* Always keep a stable tree regardless of the code churn at 
          the developement trees.

	* Provide a history of stable slash2 bits that can be binary
          searched for regression.

All history are preserved. This tree can be tagged or branched if necessary.

The following commands can be used to pull commits from development trees:

$ git remote add pfl https://github.com/pscedu/pfl
$ git pull -X theirs --no-edit pfl master

$ git remote add slash2 https://github.com/pscedu/slash2
$ git pull -X subtree=slash2 --no-edit slash2 master

$ git remote add zfs-fuse https://github.com/pscedu/zfs-fuse
$ git pull -X subtree=zfs-fuse --no-edit zfs-fuse master


Note: Any change to this tree that does not meet the current stability test
      will be reverted UNCONDITIONALLY.

The current stability bar is defined as follows:

	- iozone -a
	- slash2 self build (wipe the tree after each build)
	- Linux kernel build
	- tar over ssh with a large directory

All the above tests are run in parallel for at least four hours without a glitch.
