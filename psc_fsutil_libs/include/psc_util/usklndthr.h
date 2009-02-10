/* $Id$ */

struct psc_usklndthr {
	void *(*put_startf)(void *);
	void   *put_arg;
};

const char *
	psc_usklndthr_get_name(struct psc_usklndthr *);
int	psc_usklndthr_get_type(struct psc_usklndthr *);
