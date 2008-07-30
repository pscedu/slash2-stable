/* $Id$ */

#ifndef _PFL_VBITMAP_H_
#define _PFL_VBITMAP_H_

#include <sys/types.h>

struct vbitmap {
	unsigned char	*vb_start;
	unsigned char	*vb_end;
	unsigned char	*vb_pos;	/* ptr to current slot for speed */
	int		 vb_lastsize;	/* #ents in last byte, for sizes not multiple of NBBY */
};

struct vbitmap *vbitmap_new(size_t);
void		vbitmap_free(struct vbitmap *);
void		vbitmap_unset(struct vbitmap *, size_t);
void		vbitmap_set(struct vbitmap *, size_t);
int		vbitmap_next(struct vbitmap *, size_t *);
int		vbitmap_resize(struct vbitmap *, size_t);
size_t		vbitmap_getsize(const struct vbitmap *);
int		vbitmap_get(const struct vbitmap *, size_t);
int		vbitmap_nfree(const struct vbitmap *);
int		vbitmap_lcr(const struct vbitmap *);
int		vbitmap_getncontig(struct vbitmap *, int *);
struct vbitmap *vbitmap_attach(char *, size_t);

void		vbitmap_printbin(const struct vbitmap *);
void		vbitmap_printhex(const struct vbitmap *);
void		vbitmap_printstats(const struct vbitmap *);
void		vbitmap_getstats(const struct vbitmap *, int *, int *);

#endif /* _PFL_VBITMAP_H_ */
