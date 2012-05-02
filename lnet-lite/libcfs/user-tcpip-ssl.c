/* $Id$ */
/* %PSC_COPYRIGHT% */

#include "openssl/err.h"
#include "openssl/ssl.h"

#include <stdio.h>
#include <poll.h>

#include "psc_util/log.h"

#include "libcfs/kp30.h"

SSL_CTX *libcfs_sslctx;

void
libcfs_ssl_logerr(int level, SSL *ssl, int code)
{
#define ERRBUF_LEN 256
	int xcode = SSL_get_error(ssl, code);
	char sbuf[ERRBUF_LEN];

	if (ERR_error_string(ERR_get_error(), sbuf) == NULL)
		psclog_error("unable to get SSL error");
	psclog(level, "ssl error: %s (%d)", sbuf, xcode);
}

void
libcfs_ssl_init(void)
{
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	libcfs_sslctx = SSL_CTX_new(SSLv23_client_method());
	if (libcfs_sslctx == NULL)
		libcfs_ssl_logerr(PLL_FATAL, NULL, 0);
}

ssize_t
libcfs_ssl_sock_readv(struct lnet_xport *lx, const struct iovec *iov,
    int n)
{
	int j, rc = 0;

	for (j = 0; j < n; j++) {
		rc = SSL_read(lx->lx_ssl, iov[j].iov_base,
		    iov[j].iov_len);
		if (rc < 0)
			break;
	}
	return (rc);
}

ssize_t
libcfs_ssl_sock_read(struct lnet_xport *lx, void *buf, size_t n,
    int timeout)
{
	cfs_time_t start_time = cfs_time_current();
	struct pollfd pfd;
	ssize_t rc;

	pfd.fd = lx->lx_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	/* poll(2) measures timeout in msec */
	timeout *= 1000;

	while (n != 0 && timeout > 0) {
		rc = poll(&pfd, 1, timeout);
		if (rc < 0)
			return (-errno);
		if (rc == 0)
			return (-ETIMEDOUT);
		if ((pfd.revents & POLLIN) == 0)
			return (-EIO);

		rc = SSL_read(lx->lx_ssl, buf, n);
		if (rc < 0)
			return (-errno);
		if (rc == 0)
			return (-EIO);

		buf = (char *)buf + rc;
		n -= rc;

		timeout -= cfs_duration_sec(cfs_time_sub(
		    cfs_time_current(), start_time));
	}

	if (n == 0)
		return (0);
	return (-ETIMEDOUT);
}

ssize_t
libcfs_ssl_sock_writev(struct lnet_xport *lx, const struct iovec *iov,
    int n)
{
	int j, rc = 0;

	for (j = 0; j < n; j++) {
		rc = SSL_write(lx->lx_ssl, iov[j].iov_base,
		    iov[j].iov_len);
		if (rc < 0)
			break;
	}
	return (rc);
}

int
libcfs_ssl_sock_close(struct lnet_xport *lx)
{
	int rc;

	rc = SSL_shutdown(lx->lx_ssl);
	SSL_free(lx->lx_ssl);
	lx->lx_ssl = NULL;
	return (rc);
}

int
libcfs_ssl_sock_init(struct lnet_xport *lx)
{
	lx->lx_ssl = SSL_new(libcfs_sslctx);
	if (lx->lx_ssl == NULL)
		libcfs_ssl_logerr(PLL_FATAL, NULL, 0);
	return (0);
}

int
libcfs_ssl_sock_accept(struct lnet_xport *lx, int s)
{
	int rc;

	rc = SSL_accept(lx->lx_ssl);
	if (rc)
		return (rc);
	return (rc);
}

int
libcfs_ssl_sock_connect(struct lnet_xport *lx, int s)
{
	int rc;

	lx->lx_fd = s;
	SSL_set_fd(lx->lx_ssl, s);
	rc = SSL_connect(lx->lx_ssl);
	if (rc != 1)
		libcfs_ssl_logerr(PLL_FATAL, lx->lx_ssl, rc);
	return (0);
}

struct lnet_xport_int libcfs_ssl_lxi = {
	libcfs_ssl_sock_accept,
	libcfs_ssl_sock_close,
	libcfs_ssl_sock_connect,
	libcfs_ssl_sock_init,
	libcfs_ssl_sock_read,
	libcfs_ssl_sock_readv,
	libcfs_ssl_sock_writev
};
