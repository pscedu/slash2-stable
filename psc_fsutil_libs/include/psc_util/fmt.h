/* $Id$ */

#define PSCFMT_HUMAN_BUFSIZ	8	/* 1000.00M */
#define PSCFMT_RATIO_BUFSIZ	6	/* 99.99% */

void	psc_fmt_human(char buf[], double);
void	psc_fmt_ratio(char buf[], int, int);
