#!/bin/sh

DEBUG=MYSMTP_DEBUG
BUILDDIR=binaries
SOURCESDIR=src

rm -rf ./${BUILDDIR}s > /dev/null
mkdir -p ./${BUILDDIR}

gcc -c -o ${BUILDDIR}/client.o ${SOURCESDIR}/client.c -D${DEBUG} -Wall -Wextra -isystem ./openssl_poc 
gcc -c -o ${BUILDDIR}/mysmtp.o ${SOURCESDIR}/mysmtp.c -D${DEBUG} -Wall -Wextra -isystem ./openssl_poc
gcc -c -o ${BUILDDIR}/server.o ${SOURCESDIR}/server.c -D${DEBUG} -Wall -Wextra -isystem ./openssl_poc

gcc -o ${BUILDDIR}/client ${BUILDDIR}/client.o ${BUILDDIR}/mysmtp.o -L ./openssl_poc -lssl -lcrypto -ldl
gcc -o ${BUILDDIR}/server ${BUILDDIR}/server.o ${BUILDDIR}/mysmtp.o -L ./openssl_poc -lssl -lcrypto -ldl


