/* $Id: zestConfigSym.c 2189 2007-11-07 22:18:18Z yanovich $ */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zestAssert.h"
#include "zestConfigSym.h"
#include "zestLog.h"
#include "zestSymtable.h"

struct symtable *
get_symbol(const char *name)
{
	struct symtable *e = NULL;

	znotify("symbol lookup '%s'", name);

	for (e = sym_table; e != NULL && e->name != NULL ; e++) {
		if (e->name && !strcmp(e->name, name))
			break;
	}
	if (e == NULL || e->name == NULL) {
		zwarnx("Symbol '%s' was not found", name);
	}
	return e;
}

#define GETSYM(snam, sym)						\
	do {								\
		sym = get_symbol(snam);					\
		if (sym == NULL)					\
			zfatalx("Invalid config entry %s", snam);	\
	} while (0)

void
store_tok_val(const char *tok, char *val)
{
	struct symtable *e;
	void              *ptr;

	e = get_symbol(tok);
	zest_assert(e != NULL);

	ptr = e->offset + (char *)zestNodeProfile;

	zest_assert(e->sym_type == ZEST_VARIABLE ||
		    e->sym_type == ZEST_FLAG);

	znotify("val %s tok %s", val, tok);
	znotify("sym entry %p, name %s, param_type %d",
	       e, e->name, e->sym_param_type);

	switch (e->sym_param_type) {

	case ZEST_STRING:
		strncpy(ptr, val, e->param);
		((char *)ptr)[e->param - 1] = '\0';
		ztrace("ZEST_STRING Tok '%s' set to '%s'",
		       e->name, (char *)ptr);
		break;

	case ZEST_HEXU64:
		*(u64 *)ptr = strtoull(val, NULL, 16);
		ztrace("ZEST_HEXU64 Tok '%s' set to '%"ZLPX64"'",
		       e->name, (u64)*(u64 *)(ptr));
		break;

	case ZEST_INT:
		*(long *)ptr = strtol(val, NULL, 10);
		ztrace("ZEST_INT Tok '%s' set to '%ld'",
		       e->name, (long)*(long *)(ptr));
		break;

	case ZEST_BOOL:
		if ( !strncmp("yes", val, 3) ) {
			*(int *)ptr |= e->param;
			ztrace("ZEST_BOOL Option '%s' enabled", e->name);

		} else
			ztrace("ZEST_BOOL Option '%s' disabled", e->name);
		break;

	case ZEST_FLOAT:
		{
			char   *c, floatbuf[17];
			float   f;

			f = atof(val);
			c = floatbuf;

			bzero(floatbuf, 16);
			snprintf(floatbuf, 16, "%f", f);

			ztrace("float_val %f '%s'",
				f, floatbuf);

			while (*(c++) != '\0')
				if (*c == '.') *c = '\0';

			*(long *)ptr = strtol(floatbuf, NULL, 10);

			if ( *c != '\0' ) {
				*(long *)(ptr+1) = strtol(c, NULL, 10);
			}
			ztrace("ZEST_FLOAT Tok '%s' Secs %ld Usecs %lu",
				e->name, *(long *)ptr, *(long *)(ptr+1));
		}
		break;

	case ZEST_SIZET:
		{
			u64   i;
			int   j;
			char *c;

			j = strlen(val);
			c = &val[j-1];

			switch (tolower(*c)) {
			case 'b':
				i = (u64)1;
				break;
			case 'k':
				i = (u64)1024;
				break;
			case 'm':
				i = (u64)1024*1024;
				break;
			case 'g':
				i = (u64)1024*1024*1024;
				break;
			case 't':
				i = (u64)1024*1024*1024*1024;
				break;
			default:
				zfatalx("Sizeval '%c' is not valid", *c);
			}
			ztrace("ival   = %"ZLPU64, i);

			*c = '\0';
			*(u64 *)ptr = (u64)(i * strtoull(val, NULL, 10));

			ztrace("ZEST_SIZET Tok '%s' set to '%"ZLPU64"'",
				e->name, *(u64 *)ptr);
		}
		break;

	default:
		zfatalx("Invalid Token '%s'", e->name);
	}
}
