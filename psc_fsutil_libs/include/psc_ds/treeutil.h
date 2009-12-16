/* $Id$ */

#define PSC_SPLAY_XREMOVE(name, head, elm)				\
	do {								\
		if (name ## _SPLAY_REMOVE((head), (elm)) != (elm))	\
			psc_fatalx("splay inconsistency");		\
	} while (0)
