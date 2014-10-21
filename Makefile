# Makefile for ad1000 and lirc daemons

# Set compiler and options
CC=/usr/bin/gcc
CFLAGS=-Wall
INSTALL=/usr/bin/install
BINDIR=/usr/local/bin

all: lirc_led ad1000

ad1000: 
	$(CC) $(CFLAGS) -o ad1000 ad1000.c -lbcm2835 -llirc_client

lirc_led: 
	$(CC) $(CFLAGS) -o lirc_led lirc_led.c -llirc_client

clean:
	rm -f ad1000 lirc_led

install:
	test ! -f ${BINDIR}/ad1000 && $(INSTALL) ad1000 ${BINDIR}
	test ! -f ${BINDIR}/lirc_led && $(INSTALL) lirc_led ${BINDIR}

uninstall:
	test -f ${BINDIR}/ad1000 && rm ${BINDIR}/ad1000
	test -f ${BINDIR}/lirc_led && rm ${BINDIR}/lirc_led
