# $Id: Makefile 705 2007-03-01 16:31:35Z yanovich $

ROOTDIR	= ..
include ${ROOTDIR}/mk/defs.mk


PROG	= zconfig_test
SRCS    = zestYaccConfig.y
SRCS    += zestLexConfig.l
SRCS	+= zestConfigSym.c
SRCS	+= zestConfigTest.c
SRCS	+= ../zestion/log.c
CFLAGS  += -I. -I../zestion
LDFLAGS += ${LIBL}

CLEANFILES +=  y.tab.h

include ${MAINMK}

test:	${PROG}
