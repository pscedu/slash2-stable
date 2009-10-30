/* $Id$ */

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

#define vbitmap_new(siz)	vbitmap_newf((siz), 0)

/**
 * vbitmap_free - reclaim memory from a variable-sized bitmap.
 * @vb: variable bitmap.
 */
#define vbitmap_free(vb)					\
	do {							\
		_vbitmap_free(vb);				\
		(vb) = NULL;					\
	} while (0)

#define vbitmap_printbin1(vb) {						\
		unsigned char *PPp;					\
		char *Bbufp, *Bbuf =					\
			PSCALLOC(vbitmap_getsize((vb)) * NBBY + 256); \
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
	vbitmap_newf(size_t, int);

struct psc_vbitmap *
	vbitmap_attach(unsigned char *, size_t);

int	vbitmap_get(const struct vbitmap *, size_t);
int	vbitmap_getncontig(struct vbitmap *, int *);
size_t	vbitmap_getsize(const struct vbitmap *);
int	vbitmap_lcr(const struct vbitmap *);
int	vbitmap_next(struct vbitmap *, size_t *);
int	vbitmap_nfree(const struct vbitmap *);
int	vbitmap_resize(struct vbitmap *, size_t);
void	vbitmap_set(struct vbitmap *, size_t);
int	vbitmap_setnextpos(struct vbitmap *, int);
void	vbitmap_unset(struct vbitmap *, size_t);
int	vbitmap_xset(struct vbitmap *, size_t);
void	vbitmap_invert(struct vbitmap *);
void	vbitmap_setall(struct vbitmap *);
void	_vbitmap_free(struct vbitmap *);

void	vbitmap_printbin(const struct vbitmap *);
void	vbitmap_printhex(const struct vbitmap *);
void	vbitmap_printstats(const struct vbitmap *);
void	vbitmap_getstats(const struct vbitmap *, int *, int *);

#endif /* _PFL_VBITMAP_H_ */
