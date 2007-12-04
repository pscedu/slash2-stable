OBJS = $(patsubst %.c,%.o,$(filter %.c,${SRCS}))
OBJS+= $(patsubst %.y,%.o,$(filter %.y,${SRCS}))
OBJS+= $(patsubst %.l,%.o,$(filter %.l,${SRCS}))

_YACCINTM = $(patsubst %.y,%.c,$(filter %.y,${SRCS}))
_LEXINTM  = $(patsubst %.l,%.c,$(filter %.l,${SRCS}))

.y.c:
	${YACC} ${YFLAGS} $<

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

lib:	${LIBRARY}

${LIBRARY}:	${OBJS}
	${AR} ${ARFLAGS} $@ $^
