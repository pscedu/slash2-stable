/* $Id$ */

#ifndef __SEND_RECV_H__
#define __SEND_RECV_H__

struct iovec;

#define psc_sock_read(s, buf, nb, timo)		psc_sock_io((s), (buf), (nb), (timo), 0)
#define psc_sock_write(s, buf, nb, timo)	psc_sock_io((s), (buf), (nb), (timo), 1)
#define psc_sock_readv(s, iov, niov, timo)	psc_sock_iov((s), (iov), (niov), (timo), 0)
#define psc_sock_writev(s, iov, niov, timo)	psc_sock_iov((s), (iov), (niov), (timo), 1)

int psc_sock_io(int, void *, int, int, int);
int psc_sock_iov(int, struct iovec *, int, int, int);

#endif /* __SEND_RECV_H__ */
