# $FreeBSD$
# Originally from	$NetBSD: Makefile,v 1.21 1997/10/26 22:08:38 lukem Exp $
#
# Notes:
# - We don't use the libc strerror/sys_errlist because the string table is
#   quite large.
#

MAN=

.include <src.opts.mk>
MK_SSP=		no

LIBSTAND_SRC=	${.CURDIR}/../../../lib/libstand
LIBC_SRC=	${LIBSTAND_SRC}/../libc

.PATH:		${LIBSTAND_SRC}
LIB=		stand
INTERNALLIB=
MK_PROFILE=	no
NO_PIC=

WARNS?=		0

CFLAGS+= -ffreestanding -Wformat
CFLAGS+= -I${LIBSTAND_SRC}

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
CFLAGS.gcc+=	-mpreferred-stack-boundary=2
CFLAGS+=	-mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-sse3 -msoft-float
.endif
.if ${MACHINE} == "pc98"
CFLAGS+=	-Os
.endif
.if ${MACHINE_CPUARCH} == "powerpc"
CFLAGS+=	-msoft-float -D_STANDALONE -DNETIF_DEBUG
.endif
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -I.
.endif
.if ${MACHINE_CPUARCH} == "arm"
CFLAGS+=	-msoft-float -D_STANDALONE
.endif
.if ${MACHINE_CPUARCH} == "mips"
CFLAGS+=	-G0 -fno-pic -mno-abicalls
.endif

# standalone components and stuff we have modified locally
SRCS+=	gzguts.h zutil.h __main.c assert.c bcd.c bswap.c environment.c getopt.c gets.c \
	globals.c pager.c printf.c strdup.c strerror.c strtol.c strtoul.c random.c \
	sbrk.c twiddle.c zalloc.c zalloc_malloc.c

# private (pruned) versions of libc string functions
SRCS+=	strcasecmp.c

.PATH: ${LIBC_SRC}/net

SRCS+= ntoh.c

# string functions from libc
.PATH: ${LIBC_SRC}/string
SRCS+=	bcmp.c bcopy.c bzero.c ffs.c memccpy.c memchr.c memcmp.c memcpy.c \
	memmove.c memset.c qdivrem.c strcat.c strchr.c strcmp.c strcpy.c \
	strcspn.c strlen.c strncat.c strncmp.c strncpy.c strpbrk.c \
	strrchr.c strsep.c strspn.c strstr.c strtok.c swab.c
.if ${MACHINE_CPUARCH} == "arm"
.PATH: ${LIBC_SRC}/arm/gen

# Compiler support functions
.PATH: ${LIBSTAND_SRC}/../../contrib/compiler-rt/lib/builtins/
# __clzsi2 and ctzsi2 for various builtin functions
SRCS+=	clzsi2.c ctzsi2.c
# Divide and modulus functions called by the compiler
SRCS+=	 divmoddi4.c  divmodsi4.c  divdi3.c  divsi3.c  moddi3.c  modsi3.c
SRCS+=	udivmoddi4.c udivmodsi4.c udivdi3.c udivsi3.c umoddi3.c umodsi3.c

.PATH: ${LIBSTAND_SRC}/../../contrib/compiler-rt/lib/builtins/arm/
SRCS+=	aeabi_idivmod.S aeabi_ldivmod.S aeabi_uidivmod.S aeabi_uldivmod.S
SRCS+=	aeabi_memcmp.S aeabi_memcpy.S aeabi_memmove.S aeabi_memset.S

.endif
.if ${MACHINE_CPUARCH} == "powerpc"
.PATH: ${LIBC_SRC}/quad
SRCS+=	ashldi3.c ashrdi3.c
SRCS+=	syncicache.c
.endif

# uuid functions from libc
.PATH: ${LIBC_SRC}/uuid
SRCS+= uuid_equal.c uuid_is_nil.c

# _setjmp/_longjmp
.if ${MACHINE_CPUARCH} == "amd64"
.PATH: ${LIBSTAND_SRC}/i386
.else
.PATH: ${LIBSTAND_SRC}/${MACHINE_CPUARCH}
.endif
SRCS+=	_setjmp.S

# decompression functionality from libbz2
# NOTE: to actually test this functionality after libbz2 upgrade compile
# loader(8) with LOADER_BZIP2_SUPPORT defined
.PATH: ${LIBSTAND_SRC}/../../contrib/bzip2
CFLAGS+= -DBZ_NO_STDIO -DBZ_NO_COMPRESS
SRCS+=	libstand_bzlib_private.h

.for file in bzlib.c crctable.c decompress.c huffman.c randtable.c
SRCS+=	_${file}
CLEANFILES+=	_${file}

_${file}: ${file}
	sed "s|bzlib_private\.h|libstand_bzlib_private.h|" \
	    ${.ALLSRC} > ${.TARGET}
.endfor

CLEANFILES+= libstand_bzlib_private.h
libstand_bzlib_private.h: bzlib_private.h
	sed -e 's|<stdlib.h>|"stand.h"|' \
		${.ALLSRC} > ${.TARGET}

# decompression functionality from libz
.PATH: ${LIBSTAND_SRC}/../libz
CFLAGS+=-DHAVE_MEMCPY -I${LIBSTAND_SRC}/../libz
SRCS+=	adler32.c crc32.c libstand_zutil.h libstand_gzguts.h

.for file in infback.c inffast.c inflate.c inftrees.c zutil.c
SRCS+=	_${file}
CLEANFILES+=	_${file}

_${file}: ${file}
	sed -e "s|zutil\.h|libstand_zutil.h|" \
	    -e "s|gzguts\.h|libstand_gzguts.h|" \
	    ${.ALLSRC} > ${.TARGET}
.endfor

# depend on stand.h being able to be included multiple times
.for file in zutil.h gzguts.h
CLEANFILES+= libstand_${file}
libstand_${file}: ${file}
	sed -e 's|<fcntl.h>|"stand.h"|' \
	    -e 's|<stddef.h>|"stand.h"|' \
	    -e 's|<string.h>|"stand.h"|' \
	    -e 's|<stdio.h>|"stand.h"|' \
	    -e 's|<stdlib.h>|"stand.h"|' \
	    ${.ALLSRC} > ${.TARGET}
.endfor

# io routines
SRCS+=	closeall.c dev.c ioctl.c nullfs.c stat.c \
	fstat.c close.c lseek.c open.c read.c write.c readdir.c

# network routines
SRCS+=	arp.c ether.c inet_ntoa.c in_cksum.c net.c udp.c netif.c rpc.c

# network info services:
SRCS+=	bootp.c rarp.c bootparam.c

# boot filesystems
SRCS+=	ufs.c nfs.c cd9660.c tftp.c gzipfs.c bzipfs.c
SRCS+=	dosfs.c ext2fs.c
SRCS+=	splitfs.c
SRCS+=	pkgfs.c
.if ${MK_NAND} != "no"
SRCS+=	nandfs.c
.endif

.include <bsd.lib.mk>

.if ${MACHINE_CPUARCH} == "amd64"
CLEANFILES+= machine
beforedepend ${OBJS}: machine
machine:
	ln -fs ${.CURDIR}/../../i386/include machine
.endif
