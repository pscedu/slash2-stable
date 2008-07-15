/* $Id$ */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include <errno.h>

#include "psc_util/assert.h"
#include "psc_util/log.h"
#include "psc_util/semaphore.h"


/*
 * new locking interface, lckType needs to be
 *	something like (LOCK) or (ULOCK)
 */
void
psc_sem_op(int sem, int type)
{
	struct sembuf sbuf;

	if (type & PSEMS_LOCK)
		sbuf.sem_op = -1;
	else
		sbuf.sem_op = 1;

	sbuf.sem_flg = SEM_UNDO;
	sbuf.sem_num = 0;

	if (semop(sem, &sbuf, 1) == -1)
		psc_fatal("semop");
}

/*
 * Init a semaphore based on the ftok of the handed
 *	string.	The string should be a file name or a FID
 *	This a cool way of performing this action.. supposedly
 *	Stevens was the person who came up with it.
 */
int
psc_sem_init(key_t key)
{
	int sem, semval;
	union semun semvar;
	struct sembuf op_lock[2] = {
		{ 2, 0, 0 },	/* wait for lock to equal 0 */
		{ 2, 1, 0 },	/* set sem 2 to 1 (locking it) */
	};

	struct sembuf op_unlock = { 2, -1, 0 }; /* unlock sem 2 */

 try_again:
	psc_dbg("try: semget() key %u", key);

	/* SEMGET needs permissions, preferably the mode of the file */
	sem = semget(key, 3, IPC_CREAT | 0700);

	psc_assert(sem >= 0);

	psc_dbg("try: semop() sem %u", sem);

	/* need to get and init semaphore */
	if (semop(sem, op_lock, 2) == -1) {
		psc_assert(errno == EINVAL);
		goto try_again;
	}

	psc_dbg("try: semctl() sem 1");

	semval = semctl(sem, 1, GETVAL, 0);

	psc_assert(semval >= 0);

	if ( !semval) {
		psc_dbg("init the sems!");

		/* the semaphore has not yet been initialized */
		semvar.val = 1;

		semval = semctl(sem, 0, SETVAL, semvar);
		psc_assert(semval >= 0);

		semval = semctl(sem, 1, SETVAL, semvar);
		psc_assert(semval >= 0);
	}

	/* unlock the lock 2 */
	psc_assert(!semop(sem, &op_unlock, 1));

	psc_dbg("semop() unlock completed.. returning %u", sem);
	return sem;
}
