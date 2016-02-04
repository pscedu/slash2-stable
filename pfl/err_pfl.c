/* $Id$ */
/* %ISC_COPYRIGHT% */

#include <stdlib.h>
#include <string.h>

#include "pfl/cdefs.h"

const char *
sys_strerror(int rc)
{
	return (strerror(rc));
}

#include "pfl/err.h"

char *pfl_errstrs[] = {
/*  0 */ "Bad message",
/*  1 */ "Key has expired",
/*  2 */ "No connection to peer",
/*  3 */ "Operation already in progress",
/*  4 */ "Operation not supported",
/*  5 */ "Function not implemented",
/*  6 */ "Operation canceled",
/*  7 */ "Stale file handle",
/*  8 */ "Bad magic",
/*  9 */ "Required key not available",
/* 10 */ "Invalid checksum",
/* 11 */ "Operation timed out",
	  NULL
};

const char *
pfl_strerror(int error)
{
	error = abs(error);

	if (error >= _PFLERR_START &&
	    error < _PFLERR_START + nitems(pfl_errstrs))
		return (pfl_errstrs[error - _PFLERR_START]);
	return (sys_strerror(error));
}