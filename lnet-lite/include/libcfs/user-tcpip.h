/* $Id$ */

int libcfs_getpeername(int sock_fd, __u32 *ipaddr_p, __u16 *port_p);
int libcfs_sock_bind_to_port(int fd, __u16 port);
int libcfs_sock_connect(int fd, __u32 ip, __u16 port);
int libcfs_sock_create(int *fdp);
int libcfs_sock_set_nagle(int fd, int nagle);
int libcfs_sock_set_bufsiz(int fd, int bufsiz);
int libcfs_sock_writev(int fd, const struct iovec *vector, int count);

int libcfs_fcntl_nonblock(int fd);
