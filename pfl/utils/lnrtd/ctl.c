/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright (c) 2011-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include "pfl/cdefs.h"
#include "pfl/ctl.h"
#include "pfl/ctlsvr.h"
#include "pfl/service.h"

#include "lnrtd.h"

struct psc_ctlop lrctlops[] = {
	PSC_CTLDEFOPS
};

void
lrctlthr_main(void)
{
	//pflrpc_register_ctlops(lrctlops);

//	psc_ctlparam_register("faults", psc_ctlparam_faults);
	psc_ctlparam_register("log.file", psc_ctlparam_log_file);
	psc_ctlparam_register("log.format", psc_ctlparam_log_format);
	psc_ctlparam_register("log.level", psc_ctlparam_log_level);
	psc_ctlparam_register("pause", psc_ctlparam_pause);
	psc_ctlparam_register("pool", psc_ctlparam_pool);
	psc_ctlparam_register("rlim", psc_ctlparam_rlim);
	psc_ctlparam_register("run", psc_ctlparam_run);

	psc_ctlthr_main(ctlsockfn, lrctlops, nitems(lrctlops), 0,
	    LRTHRT_CTLAC);
}
