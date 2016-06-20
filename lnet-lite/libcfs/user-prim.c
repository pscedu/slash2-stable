/*
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.lustre.org/lustre/docs/GPLv2.pdf
 *
 * Please contact Xyratex Technology, Ltd., Langstone Road, Havant, Hampshire.
 * PO9 1SA, U.K. or visit www.xyratex.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright (c) 2002, 2013, Xyratex Technology, Ltd . All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Some portions of Lustre® software are subject to copyrights help by Intel Corp.
 * Copyright (c) 2011-2013 Intel Corporation, Inc.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre® and the Lustre logo are registered trademarks of
 * Xyratex Technology, Ltd  in the United States and/or other countries.
 *
 * lnet/libcfs/user-prim.c
 *
 * Implementations of portable APIs for liblustre
 *
 * Author: Nikita Danilov <nikita@clusterfs.com>
 */


/*
 * liblustre is single-threaded, so most "synchronization" APIs are trivial.
 */

#ifndef __KERNEL__

#include <libcfs/libcfs.h>
#include <libcfs/kp30.h>
#include <libcfs/user-prim.h>

#include <sys/mman.h>
#ifndef  __CYGWIN__
#include <stdint.h>
#ifdef HAVE_ASM_PAGE_H
#include <asm/page.h>
#endif
#ifdef HAVE_SYS_USER_H
#include <sys/user.h>
#endif
#else
#include <sys/types.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef	HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

/*
 * Sleep channel. No-op implementation.
 */

void cfs_waitq_init(struct cfs_waitq *waitq)
{
        LASSERT(waitq != NULL);
        (void)waitq;
}

void cfs_waitlink_init(struct cfs_waitlink *link)
{
        LASSERT(link != NULL);
        (void)link;
}

void cfs_waitq_add(struct cfs_waitq *waitq, struct cfs_waitlink *link)
{
        LASSERT(waitq != NULL);
        LASSERT(link != NULL);
        (void)waitq;
        (void)link;
}

void cfs_waitq_add_exclusive(struct cfs_waitq *waitq, struct cfs_waitlink *link)
{
        LASSERT(waitq != NULL);
        LASSERT(link != NULL);
        (void)waitq;
        (void)link;
}

void cfs_waitq_forward(struct cfs_waitlink *link, struct cfs_waitq *waitq)
{
        LASSERT(waitq != NULL);
        LASSERT(link != NULL);
        (void)waitq;
        (void)link;
}

void cfs_waitq_del(struct cfs_waitq *waitq, struct cfs_waitlink *link)
{
        LASSERT(waitq != NULL);
        LASSERT(link != NULL);
        (void)waitq;
        (void)link;
}

int cfs_waitq_active(struct cfs_waitq *waitq)
{
        LASSERT(waitq != NULL);
        (void)waitq;
        return 0;
}

void cfs_waitq_signal(struct cfs_waitq *waitq)
{
        LASSERT(waitq != NULL);
        (void)waitq;
}

void cfs_waitq_signal_nr(struct cfs_waitq *waitq, __unusedx int nr)
{
        LASSERT(waitq != NULL);
        (void)waitq;
}

void cfs_waitq_broadcast(struct cfs_waitq *waitq)
{
        LASSERT(waitq != NULL);
        (void)waitq;
}

void cfs_waitq_wait(struct cfs_waitlink *link, __unusedx int state)
{
        LASSERT(link != NULL);
        (void)link;
}

int64_t cfs_waitq_timedwait(struct cfs_waitlink *link, __unusedx int state, __unusedx int64_t timeout)
{
        LASSERT(link != NULL);
        (void)link;
        return 0;
}

uid_t cfs_curproc_uid(void)
{
        return getuid();
}

/*
 * Use environment variables to turn on some socket features.
 */
int cfs_parse_int_tunable(int *value, char *name)
{
        char    *env = getenv(name);
        char    *end;

        if (env == NULL)
                return 0;

        *value = strtoull(env, &end, 0);
        if (*end == 0)
                return 0;

        CERROR("Can't parse tunable %s=%s\n", name, env);
        return -EINVAL;
}

/*
 * Allocator
 */

cfs_page_t *cfs_alloc_page(__unusedx unsigned int flags)
{
        cfs_page_t *pg = cfs_alloc(sizeof(*pg), 0);

        if (!pg)
                return NULL;
        pg->addr = cfs_alloc(CFS_PAGE_SIZE, 0);

        if (!pg->addr) {
                cfs_free(pg);
                return NULL;
        }
        return pg;
}

void cfs_free_page(cfs_page_t *pg)
{
        cfs_free(pg->addr);
        cfs_free(pg);
}

void *cfs_page_address(cfs_page_t *pg)
{
        return pg->addr;
}

void *cfs_kmap(cfs_page_t *pg)
{
        return pg->addr;
}

void cfs_kunmap(__unusedx cfs_page_t *pg)
{
}

/*
 * SLAB allocator
 */

cfs_mem_cache_t *
cfs_mem_cache_create(const char *name, size_t objsize, __unusedx size_t off, __unusedx unsigned long flags)
{
        cfs_mem_cache_t *c;

        c = cfs_alloc(sizeof(*c), 0);
        if (!c)
                return NULL;
        c->size = objsize;
        CDEBUG(D_MALLOC, "alloc slab cache %s at %p, objsize %d\n",
               name, c, (int)objsize);
        return c;
}

int cfs_mem_cache_destroy(cfs_mem_cache_t *c)
{
        CDEBUG(D_MALLOC, "destroy slab cache %p, objsize %u\n", c, c->size);
        cfs_free(c);
        return 0;
}

void *cfs_mem_cache_alloc(cfs_mem_cache_t *c, int gfp)
{
        return cfs_alloc(c->size, gfp);
}

void cfs_mem_cache_free(__unusedx cfs_mem_cache_t *c, void *addr)
{
        cfs_free(addr);
}

void cfs_enter_debugger(void)
{
        /*
         * nothing for now.
         */
}

void cfs_daemonize(__unusedx char *str)
{
        return;
}

int cfs_daemonize_ctxt(__unusedx char *str)
{
        return 0;
}

cfs_sigset_t cfs_block_allsigs(void)
{
        cfs_sigset_t   all;
        cfs_sigset_t   old;
        int            rc;

        sigfillset(&all);
        rc = sigprocmask(SIG_SETMASK, &all, &old);
        LASSERT(rc == 0);

        return old;
}

cfs_sigset_t cfs_block_sigs(cfs_sigset_t blocks)
{
        cfs_sigset_t   old;
        int   rc;
        
        rc = sigprocmask(SIG_SETMASK, &blocks, &old);
        LASSERT (rc == 0);

        return old;
}

void cfs_restore_sigs(cfs_sigset_t old)
{
        int   rc = sigprocmask(SIG_SETMASK, &old, NULL);

        LASSERT (rc == 0);
}

int cfs_signal_pending(void)
{
        cfs_sigset_t    empty;
        cfs_sigset_t    set;
        int  rc;

        rc = sigpending(&set);
        LASSERT (rc == 0);

        sigemptyset(&empty);

        return !memcmp(&empty, &set, sizeof(set));
}

void cfs_clear_sigpending(void)
{
        return;
}

#ifdef __linux__

/*
 * In glibc (NOT in Linux, so check above is not right), implement
 * stack-back-tracing through backtrace() function.
 */
#include <execinfo.h>

void cfs_stack_trace_fill(struct cfs_stack_trace *trace)
{
        backtrace(trace->frame, sizeof_array(trace->frame));
}

void *cfs_stack_trace_frame(struct cfs_stack_trace *trace, int frame_no)
{
        if (0 <= frame_no && frame_no < (int)sizeof_array(trace->frame))
                return trace->frame[frame_no];
        else
                return NULL;
}

#else

void cfs_stack_trace_fill(__unusedx struct cfs_stack_trace *trace)
{}
void *cfs_stack_trace_frame(__unusedx struct cfs_stack_trace *trace, __unusedx int frame_no)
{
        return NULL;
}

/* __linux__ */
#endif

void lbug_with_loc(const char *file, const char *func, const int line)
{
        /* No libcfs_catastrophe in userspace! */
        libcfs_debug_msg(NULL, 0, D_EMERG, file, func, line, "LBUG\n");
        abort();
}

/* !__KERNEL__ */
#endif

/*
 * Local variables:
 * c-indentation-style: "K&R"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 80
 * scroll-step: 1
 * End:
 */
