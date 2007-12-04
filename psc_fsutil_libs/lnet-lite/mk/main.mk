# $Id$

include ${LLROOTDIR}/mk/local.mk

INCLUDES+=	-I.
CFLAGS+=	-Wall -W -g ${INCLUDES} ${DEFINES}

# Default to build a binary, but may be overridden after
# this file has been included, e.g. for a ${LIBRARY}.
TARGET?=	${PROG}

include ${MAINMK}
