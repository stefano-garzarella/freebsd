# $FreeBSD$

PROG=	kgdb${GDB_SUFFIX}
SRCS=	main.c kld.c kthr.c trgt.c trgt_${TARGET_CPUARCH}.c
WARNS?=	2

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a \
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

DPADD=	${GDBLIBS} ${BULIBS} ${LIBKVM} ${LIBM} ${LIBREADLINE} ${LIBTERMCAPW} \
	${LIBGNUREGEX}
LDADD=	${GDBLIBS} ${BULIBS} -lkvm${GDB_SUFFIX} -lm ${LDREADLINE} -ltermcapw \
	-lgnuregex

.if defined(GDB_CROSS_DEBUGGER)
CFLAGS+= -Wl,-export-dynamic
.endif

.include <bsd.prog.mk>
