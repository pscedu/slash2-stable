/* $Id$ */

/*
 * Pool manager routines and definitions.
 *
 * Pools are like free lists but also track allocations for items
 * in circulation among multiple lists.
 *
 * Pools share allocations for items among themselves, so when
 * one greedy pool gets too large, others trim his size down.
 */

struct psc_poolmgr {
	list_cache_t	  ppm_lc;		/* free pool entries */
	int		  ppm_flags;		/* flags */
	int		  ppm_min;		/* min bound of #items */
	int		  ppm_max;		/* max bound of #items */
	int		  ppm_total;		/* #items in circulation */
	void		(*ppm_initf)(void *);	/* entry initialization routine */
};

/* Pool manager flags. */
#define PPMF_AUTO	(1 << 0)	/* this pool automatically resizes */
#define PPMF_REAP	(1 << 1)	/* this pool may reap pigs and be reaped */

#define POOL_LOCK(m)	spinlock(&(m)->ppm_lc.lc_lock)
#define POOL_ULOCK(m)	freelock(&(m)->ppm_lc.lc_lock)

#define POOL_CHECK(m)							\
	do {								\
		psc_assert((m)->ppm_min >= 0);				\
		psc_assert((m)->ppm_max >= 0);				\
		psc_assert((m)->ppm_total >= 0);			\
		psc_assert((m)->ppm_min <= (m)->ppm_total);		\
		psc_assert((m)->ppm_total <= (m)->ppm_max);		\
	} while (0)

/*
 * pool_init - initialize a pool.
 * @m: the pool manager to initialize.
 * @type: type of entry member.
 * @member: name of psclist_head entry member.
 * @flags: pool manager flags.
 * @total: initial number of entries to join pool.
 * @initf: callback routine for initializing a pool entry.
 * @namefmt: printf(3)-style name of pool.
 */
#define pool_init(m, type, member, flags, total, initf, namefmt, ...)	\
	_pool_init((m), offsetof(type, member), sizeof(type), (flags),	\
	    (total), (initf), namefmt, ## __VA_ARGS__)

void
_pool_init(struct psc_poolmgr *m, ptrdiff_t offset, size_t entsize,
    int flags, int total, void (*initf)(void *), const char *namefmt, ...)
{
	va_list ap;

	memset(m, 0, sizeof(&m));
	m->ppm_flags = flags;
	m->ppm_initf = initf;

	va_start(ap, namefmt);
	_lc_reginit(&m->ppm_lc, offset, entsize, namefmt, ap);
	va_end(ap);

	if (total)
		pool_grow(m, total);
}

/*
 * pool_get - grab an item from a pool.
 * @m: the pool manager.
 */
void *
pool_get(struct psc_poolmgr *m)
{
	void *p;

	POOL_LOCK(m);
	p = lc_getnb(&m->ppm_lc);
	/* XXX if below low watermark, allocate or steal some entries */
	POOL_ULOCK(m);
	return (p);
}

/*
 * pool_return - return an item to a pool.
 * @m: the pool manager.
 * @p: item to return.
 */
void
pool_return(struct psc_poolmgr *m, void *p)
{
	POOL_LOCK(m);
	lc_add(&m->ppm_lc, p);
	psc_assert(m->ppm_lc.lc_size <= m->ppm_max);
	/* XXX if above high watermark, free some entries */
	POOL_ULOCK(m);
}

/*
 * pool_grow - increase #items in a pool.
 * @m: the pool manager.
 * @n: #items to add to pool.
 */
static inline int
pool_grow(struct psc_poolmgr *m, int n)
{
	void *p;
	int i;

	psc_assert(n > 0);

	POOL_LOCK(m);
	POOL_CHECK(m);
	if (m->ppm_total == m->ppm_max) {
		POOL_ULOCK(m);
		return (0);
	}
	/* Bound number to add to ppm_max. */
	n = MIN(n, m->ppm_max - m->ppm_size);
	POOL_ULOCK(m);

	for (i = 0; i < n; i++) {
		p = TRY_PSCALLOC(m->ppm_lc.lc_entsize);
		if (p == NULL) {
			errno = ENOMEM;
			return (-1);
		}
		if (m->ppm_initf)
			m->ppm_initf(p);
		POOL_LOCK(m);
		if (m->ppm_total < m->ppm_max) {
			m->ppm_total++;
			lc_add(&m->ppm_lc, p);
		} else
			n = 0; /* break prematurely */
		POOL_ULOCK(m);

		/*
		 * If we prematurely exiting,
		 * we didn't use this item.
		 */
		if (n == 0)
			free(p);
	}
	return (i);
}

/*
 * pool_shrink - decrease #items in a pool.
 * @m: the pool manager.
 * @n: #items to remove from pool.
 */
static inline int
pool_shrink(struct psc_poolmgr *m, int n);
{
	void *p;
	int i;

	psc_assert(n > 0);

	POOL_LOCK(m);
	POOL_CHECK(m);
	if (m->ppm_total == m->ppm_min) {
		POOL_ULOCK(m);
		return (0);
	}
	/* Bound number to add to ppm_min. */
	n = MAX(n, m->ppm_size - m->ppm_min);
	POOL_ULOCK(m);

	for (i = 0; i < n; i++) {
		POOL_LOCK(m);
		if (&m->ppm_total > m->ppm_min) {
			p = lc_getnb(lc);
			psc_assert(p);
			m->ppm_total--;
		} else {
			p = NULL;
			n = 0; /* break prematurely */
		}
		POOL_ULOCK(m);
		free(p);
	}
	return (i);
}
