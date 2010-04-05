/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#ifndef _PFL_TYPES_H_
#define _PFL_TYPES_H_

#include <inttypes.h>

/* printf(3) specifier modifiers for custom types. */
#define PSCPRIxLNID	PRIx64
#define PSCPRIxOFF	PRIx64
#define PSCPRIdOFF	PRId64
#define PSCPRIxCRC32	"08u"
#define PSCPRIxCRC64	"016"PRIx64

#if defined(__APPLE__) || defined(__OpenBSD__)
# define PSCPRI_PTHRT	"p"
#else
# define PSCPRI_PTHRT	"lu"
#endif

#define PSCPRIuTIMET	"lu"

#ifdef __APPLE__
# define PSCPRIuUTIMET	"06u"
# define PSCPRIdUTIMET	"06d"
#else
# define PSCPRIuUTIMET	"06lu"
# define PSCPRIdUTIMET	"06ld"
#endif

#define PSCPRIuNTIMET	"06lu"
#define PSCPRIdNTIMET	"06ld"

#if defined(__USE_FILE_OFFSET64) || defined(__APPLE__)
# define PSCPRIuINOT	PRIu64
#elif defined(__linux)
# define PSCPRIuINOT	"lu"
#else
# define PSCPRIuINOT	"u"
#endif

#include "psc_util/subsys.h"

#endif /* _PFL_TYPES_H_ */
