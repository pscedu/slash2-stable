/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <ctype.h>
#include <inttypes.h>

#include "pfl/str.h"

#include "fio.h"
#include "sym.h"

struct symtable *
get_symbol(const char *name)
{
	struct symtable *e;

	BDEBUG("symbol lookup '%s'\n", name);

	for (e = sym_table; e != NULL && e->name != NULL ; e++)
		if (e->name && !strcmp(e->name, name))
			break;

	if (e == NULL || e->name == NULL)
		WARN("Symbol '%s' was not found\n", name);
	return e;
}

struct symtable *
func_lookup(unsigned long addr)
{
	struct symtable *e;

	for (e = sym_table; e != NULL && e->name != NULL ; e++)
		if (e->sym_type && FIO_FUNCTION)
			if ((unsigned long)e->io_func == addr) break;

	if (e == NULL || e->name == NULL)
		WARN("Symbol '%lx' was not found\n", addr);
	return e;
}

char *
func_addr_2_name(unsigned long addr)
{
	struct symtable *e = func_lookup(addr);

	ASSERT(e != NULL);
	return (e->name);
}


#define GETSYM(snam, sym) do {						\
	(sym) = get_symbol(snam);					\
	if ((sym) == NULL) {						\
		WARN("Invalid config entry %s\n", (snam));		\
		ASSERT(0);						\
	}								\
} while (0)


void
store_func(const char *func_name)
{
	struct symtable *sym = get_symbol(func_name);
	int num_iotests = currentGroup->num_iotests;
	struct io_routine *ior = &currentGroup->iotests[num_iotests];

	ASSERT(sym != NULL && ior != NULL);

	BDEBUG("Test Recipe Comp: %s func_ptr %p\n",
	    func_name, sym->io_func);

	ior->io_routine[ior->num_routines] = pfl_strdup(func_name);
	ior->num_routines++;
}

void
store_tok_val(const char *tok, char *val)
{
	struct symtable *e = get_symbol(tok);
	void *ptr = ((char *)currentGroup) + e->offset;
	char floatbuf[17], *s = ptr, *c;
	uint64_t *z = ptr, i = 0;
	int *t = ptr, j;
	float f;

	ASSERT(e != NULL);

	ASSERT(e->sym_type == FIO_VARIABLE ||
	    e->sym_type == FIO_FLAG);

	BDEBUG("val %s tok %s\n", val, tok);
	BDEBUG("sym entry %p, name %s, param_type %d\n",
	    e, e->name, e->sym_param_type);

	switch (e->sym_param_type) {

	case FIOT_STRING:
		strncpy(s, val, e->param);
		BDEBUG("FIOT_STRING Tok '%s' set to '%s'\n",
		    e->name, s);
		break;

	case FIOT_INT:
		*t = strtol(val, NULL, 10);
		BDEBUG("FIOT_INT Tok '%s' set to '%d'\n",
		    e->name, *t);
		break;

	case FIOT_BOOL:
		if ( !strncmp("yes", val, 3) ) {
			*t |= e->param;
			BDEBUG("FIOT_BOOL Option '%s' enabled\n", e->name);
		} else {
			BDEBUG("FIOT_BOOL Option '%s' disabled\n", e->name);
		}
		break;

	case FIOT_FLOAT:
		f = atof(val);
		c = floatbuf;
		j = 0;
		memset(floatbuf, 0, sizeof(floatbuf));
		snprintf(floatbuf, sizeof(floatbuf), "%f", f);

		BDEBUG("float_val %f '%s'\n", f, floatbuf);

		while (*(c++) != '\0')
			if (*c == '.')
				*c = '\0';

		*t = strtol(floatbuf, NULL, 10);

		if ( *c != '\0' )
			*(t+1) = strtol(c, NULL, 10);

		BDEBUG("FIOT_FLOAT Tok '%s' Secs %d Usecs %d \n",
		    e->name, *t, *(t+1));
		break;

	case FIOT_SIZET:
		j = strlen(val);
		c = &val[j-1];

		switch (tolower(*c)) {
		case 'b':
			i = UINT64_C(1);
			break;
		case 'k':
			i = UINT64_C(1024);
			break;
		case 'm':
			i = UINT64_C(1024)*1024;
			break;
		case 'g':
			i = UINT64_C(1024)*1024*1024;
			break;
		case 't':
			i = UINT64_C(1024)*1024*1024*1024;
			break;
		default:
			WARN("Sizeval '%c' is not valid\n", *c);
			ASSERT(0);
		}

		BDEBUG("ival   = %"PRIu64"\n", i);

		val[j-1] = '\0';
		*z = i * strtoull(val, NULL, 10);

		BDEBUG("FIOT_SIZET Tok '%s' set to '%"PRIu64"'\n",
		    e->name, *z);
		break;

	default:
		WARN("Invalid Token '%s'\n", e->name);
		ASSERT(0);
	}
}
