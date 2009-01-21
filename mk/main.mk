
include ${SFT_BASE}/mk/local.mk

INCLUDES+=      -I${PFL_BASE}/include
INCLUDES+=      -I${SFT_BASE} -I.

CFLAGS+=        ${INCLUDES} ${DEFINES}
YFLAGS+=        -d -o $@
TARGET?=        ${PROG}

include ${MAINMK}


