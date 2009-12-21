/* $Id$ */

#ifndef _PFL_TREEUTIL_H_
#define _PFL_TREEUTIL_H_

#define PSC_SPLAY_XREMOVE(name, head, elm)				\
	do {								\
		if (name ## _SPLAY_REMOVE((head), (elm)) != (elm))	\
			psc_fatalx("SPLAY_REMOVE: item not found");	\
	} while (0)

#define PSC_SPLAY_XINSERT(name, head, elm)				\
	do {								\
		if (name ## _SPLAY_INSERT((head), (elm)))		\
			psc_fatalx("SPLAY_INSERT: item with key "	\
			    "already exists");				\
	} while (0)

#endif /* _PFL_TREEUTIL_H_ */
