/* $Id$ */

/*
 * Variable-sized bitmaps.  Internally, bitmaps are arrays
 * of chars that are realloc(3)'d to different lengths.
 *
 * This API is not thread-safe!
 */

#ifndef _PFL_VBITMAP_H_
#define _PFL_VBITMAP_H_

#include <sys/types.h>

struct psc_vbitmap {
	unsigned char	*vb_start;
	unsigned char	*vb_end;
	unsigned char	*vb_pos;	/* ptr to current slot for speed */
	int		 vb_lastsize;	/* #ents in last byte, for sizes not multiple of NBBY */
	int		 vb_flags;
};

#define vbitmap psc_vbitmap

/* vbitmap flags */
#define PVBF_AUTO	(1 << 0)	/* auto grow bitmap as necessary */
#define PVBF_STATIC	(1 << 1)	/* vbitmap is statically allocated */
#define PVBF_EXTALLOC	(1 << 2)	/* bitmap mem is externally alloc'd */

#define VBITMAP_INIT_AUTO	{ NULL, NULL, NULL, 0, PVBF_AUTO | PVBF_STATIC }

#define psc_vbitmap_new(siz)	psc_vbitmap_newf((siz), 0)

/**
 * psc_vbitmap_free - reclaim memory from a variable-sized bitmap.
 * @vb: variable bitmap.
 */
#define psc_vbitmap_free(vb)						\
	do {								\
		_psc_vbitmap_free(vb);					\
		(vb) = NULL;						\
	} while (0)

#define psc_vbitmap_set(vb, pos)	((void)psc_vbitmap_setval((vb), (pos), 1))
#define psc_vbitmap_xset(vb, pos)	(psc_vbitmap_setval((vb), (pos), 1) == 0)
#define psc_vbitmap_xsetval(vb, pos, v)	(psc_vbitmap_setval((vb), (pos), (v)) != (v))
#define psc_vbitmap_unset(vb, pos)	((void)psc_vbitmap_setval((vb), (pos), 0))

#define psc_vbitmap_nset(vb)		(psc_vbitmap_getsize(vb) - psc_vbitmap_nfree(vb))

#define psc_vbitmap_printbin1(vb) {					\
		unsigned char *PPp;					\
		char *Bbufp, *Bbuf =					\
			PSCALLOC(psc_vbitmap_getsize((vb)) * NBBY + 256); \
									\
		for (PPp = (vb)->vb_start, Bbufp=Bbuf; PPp <= (vb)->vb_end; \
		     PPp++, Bbufp += NBBY+1)				\
			sprintf(Bbufp, "%d%d%d%d%d%d%d%d ",		\
				 (*PPp >> 0) & 1, (*PPp >> 1) & 1,	\
				 (*PPp >> 2) & 1, (*PPp >> 3) & 1,	\
				 (*PPp >> 4) & 1, (*PPp >> 5) & 1,	\
				 (*PPp >> 6) & 1, (*PPp >> 7) & 1);	\
		psc_dbg("vbitmap=%p contents=%s", (vb), Bbuf);		\
		PSCFREE(Bbuf);						\
	}

struct psc_vbitmap *
	 psc_vbitmap_newf(size_t, int);

struct psc_vbitmap *
	 psc_vbitmap_attach(unsigned char *, size_t);

int	 psc_vbitmap_get(const struct psc_vbitmap *, size_t);
void	 psc_vbitmap_clearall(struct psc_vbitmap *);
int	 psc_vbitmap_getncontig(struct psc_vbitmap *, int *);
size_t	 psc_vbitmap_getsize(const struct psc_vbitmap *);
void	 psc_vbitmap_invert(struct psc_vbitmap *);
int	 psc_vbitmap_lcr(struct psc_vbitmap *);
int	 psc_vbitmap_next(struct psc_vbitmap *, size_t *);
int	 psc_vbitmap_nfree(const struct psc_vbitmap *);
int	 psc_vbitmap_resize(struct psc_vbitmap *, size_t);
int	 psc_vbitmap_setval(struct psc_vbitmap *, size_t, int);
void	 psc_vbitmap_setall(struct psc_vbitmap *);
int	 psc_vbitmap_setnextpos(struct psc_vbitmap *, int);
int	 psc_vbitmap_setrange(struct psc_vbitmap *, size_t, size_t);
void	_psc_vbitmap_free(struct psc_vbitmap *);

void	 psc_vbitmap_printbin(const struct psc_vbitmap *);
void	 psc_vbitmap_printhex(const struct psc_vbitmap *);
void	 psc_vbitmap_printstats(struct psc_vbitmap *);
void	 psc_vbitmap_getstats(struct psc_vbitmap *, int *, int *);

#define vbitmap_new		psc_vbitmap_new
#define vbitmap_free		psc_vbitmap_free
#define vbitmap_printbin1	psc_vbitmap_printbin1
#define vbitmap_newf		psc_vbitmap_newf
#define vbitmap_attach		psc_vbitmap_attach
#define vbitmap_get		psc_vbitmap_get
#define vbitmap_clearall	psc_vbitmap_clearall
#define vbitmap_getncontig	psc_vbitmap_getncontig
#define vbitmap_getsize		psc_vbitmap_getsize
#define vbitmap_invert		psc_vbitmap_invert
#define vbitmap_lcr		psc_vbitmap_lcr
#define vbitmap_next		psc_vbitmap_next
#define vbitmap_nfree		psc_vbitmap_nfree
#define vbitmap_resize		psc_vbitmap_resize
#define vbitmap_set		psc_vbitmap_set
#define vbitmap_setall		psc_vbitmap_setall
#define vbitmap_setnextpos	psc_vbitmap_setnextpos
#define vbitmap_setrange	psc_vbitmap_setrange
#define vbitmap_unset		psc_vbitmap_unset
#define vbitmap_xset		psc_vbitmap_xset

#define vbitmap_printbin	psc_vbitmap_printbin
#define vbitmap_printhex	psc_vbitmap_printhex
#define vbitmap_printstats	psc_vbitmap_printstats
#define vbitmap_getstats	psc_vbitmap_getstats

#endif /* _PFL_VBITMAP_H_ */
