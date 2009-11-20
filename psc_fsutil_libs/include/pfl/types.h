/* $Id$ */

#ifndef _PFL_TYPES_H_
#define _PFL_TYPES_H_

#include <inttypes.h>

/* printf(3) specifier modifiers for custom types. */
#define PSCPRIxLNID	PRIx64
#define PSCPRIxOFF	PRIx64
#define PSCPRIdOFF	PRId64
#define PSCPRIxCRC32	"08u"
#define PSCPRIxCRC64	"016"PRIx64

#include "psc_util/subsys.h"

#endif /* _PFL_TYPES_H_ */
