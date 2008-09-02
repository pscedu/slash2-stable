#ifndef __SEND_RECV_H
#define __SEND_RECV_H 1

#define psc_sock_read(s, b, n, t) psc_sock_io(s, b, n, t, 0)
#define psc_sock_write(s, b, n, t) psc_sock_io(s, b, n, t, 1)

#endif
