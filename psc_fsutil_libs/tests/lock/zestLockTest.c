/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "zestLock.h"

#define OBUFSZ 33554432

#define STARTWATCH(t) gettimeofday((&t[0]), NULL)
#define STOPWATCH(t)  gettimeofday((&t[1]), NULL)

double calc_run_time(struct timeval *tv1,
		     struct timeval *tv2)
{
	float t;

	t = ( ((tv2->tv_usec/1000000.0) + tv2->tv_sec) -
	      ((tv1->tv_usec/1000000.0) + tv1->tv_sec) );

	return t;
}


int obytes_used;
int **obuf;
void *ptr;
zest_spinlock_t *lock;
int  *val;
int  *lockcnt;

int num_procs;
int num_locks;

/*
 *  Below is the output tester for this program.

cat /tmp/zestLocktest.out  | perl -e '$c = 1; while (<>) { $l = $_; chomp $l; if ($l  =~ /[0-9]+:([0-9]+)/ ) { if ( $c != $1) { print "$l is bad $1\n";$c = $1; } else { $c++; } } }'
*/

void printHelp(void)
{
	fprintf(stderr, "usage: zestLockTest [-t num_threads] [-n num_locks]\n");
	exit(1);
}

int getOptions(int argc,  char *argv[])
{
#define ARGS "t:n:"
	int c, err = 0;
	optarg = NULL;

	while ( !err && ((c = getopt(argc, argv, ARGS)) != -1))
		switch (c) {

		case 't':
			num_procs = atoi(optarg);
			break;

		case 'n':
			num_locks = atoi(optarg);
			break;

		default :
			printHelp();
		}

	return err;
}


int child_main(int id)
{
	int my_locks = 0;

	while (1) {
		//fprintf(stderr, "pe %d about to spinlock ..\n", getpid());
		spinlock(lock);
		///fprintf(stderr, "pe %d has spinlock val=%d ::%p:: ..\n",
		//			getpid(), *val, val);
		if ( *val > num_locks ) {
			freelock(lock);
			fprintf(stderr, "pe %d done, processed %d locks\n",
				id, my_locks);
			exit(0);
		} else {
			fprintf(stderr, "%p = *obuf %p = obuf  **obuf = %d val %p lockcnt %p lock %p\n",
				*obuf, obuf, **obuf, val, lockcnt, lock);
			fprintf(stderr, "%d:%d\n", getpid(), *val);
			*lockcnt += 1;
			**obuf = *val;
			++*obuf;
			*val += 1;
			//fprintf(stderr, "%p = *obuf %p = obuf  **obuf = %d *val %d\n",
			//	*obuf, obuf, **obuf, *val);
			freelock(lock);
			my_locks++;
			usleep(1);
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int i, *j, *save;
	int pid;
	key_t fid_key = ftok("/tmp", 1);
	int   shmid;
	int old_lockcnt;
	struct timeval tv[2];

	num_locks = 25000;
	num_procs = 4;

	if (getOptions(argc, argv) != 0) {
		printHelp();
	}

	shmid = shmget(fid_key, OBUFSZ,
		       (IPC_CREAT|IPC_EXCL|0660));

	if (shmid < 0) {
		if (errno != EEXIST) {
			err(1, "shmget");
		} else {
			shmid = shmget(fid_key, OBUFSZ ,0660);
			if (shmid < 0)
				err(1, "shmget");
		}
	}

	ptr = shmat(shmid, NULL, 0);
	if (ptr == (void *)-1)
		err(1, "shmat");

	bzero(ptr, OBUFSZ);

	lock = (zest_spinlock_t *)ptr;
	val  = (int *)lock + (sizeof(zest_spinlock_t));

	lockcnt = (int *)val + sizeof(int);

	obuf  = (int**)lockcnt + sizeof(int *);
	save  = *obuf = (int *)obuf + sizeof(int**);


	printf("*obuf=%p :: obuf=%p :: save=%p\n", *obuf, obuf, save);


	*lockcnt = 0;
	*val  = 0;
	LOCK_INIT(lock);
	spinlock(lock);

	old_lockcnt = 0;

	for (i=0; i<num_procs; i++) {
		if ( !(pid = fork()) ) child_main(i);
		//else fprintf(stderr, "forked %d\n", pid);
	}

	// unleash!
	freelock(lock);

	//child_main();

	while (num_procs) {
		pid = wait4(-1, NULL, WNOHANG, NULL);
		if (pid > 0)
			num_procs--;
		else {
			STARTWATCH(tv);
			sleep(2);
			STOPWATCH(tv);
			fprintf(stderr, "%d current lock cnt; LPS %f\n",
				*lockcnt, (double)(*lockcnt - old_lockcnt)/calc_run_time(&tv[0], &tv[1]) );
			old_lockcnt = *lockcnt;
		}
	}

	j = save;
	for (i=0; i < num_locks; j++, i++) {
		if (*j != i) {
			fprintf(stderr, "Error: %d should be %d\n",
				*j, i);
			i = *j;
			exit(1);
		} else {
			fprintf(stderr, "Ok: %d should be %d\n",
                                *j, i);
		}
	}

	shmdt(ptr);
	return 0;
}
