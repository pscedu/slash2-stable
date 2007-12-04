# $Id$

include ${PROOTDIR}/mk/local.mk

INCLUDES+=	-I${PROOTDIR}/include -I.
CFLAGS+=	-Wall -W -g ${INCLUDES} ${DEFINES}
YFLAGS+=	-d -o $@

# Default to build a binary, but may be overridden after
# this file has been included, e.g. for a ${LIBRARY}.
TARGET?=	${PROG}

include ${PROOTDIR}/../mk/main.mk
