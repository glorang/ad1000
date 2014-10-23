# Makefile for ad1000

# Set compiler and options
CC=/usr/bin/gcc
CFLAGS=-Wall -g
INSTALL=/usr/bin/install
BINDIR=/usr/local/bin

all: lirc_led ad1000 ad_display

ad1000: 
	$(CC) $(CFLAGS) -o ad1000 ad1000.c -lbcm2835 

lirc_led: 
	$(CC) $(CFLAGS) -o lirc_led lirc_led.c -llirc_client

ad_display: 
	$(CC) $(CFLAGS) -o ad_display ad_display.c cJSON.c -lm

clean:
	rm -f ad1000 lirc_led ad_display

install:
	test ! -f ${BINDIR}/ad1000 && $(INSTALL) ad1000 ${BINDIR}
	test ! -f ${BINDIR}/lirc_led && $(INSTALL) lirc_led ${BINDIR}
	test ! -f ${BINDIR}/ad_display && $(INSTALL) ad_display ${BINDIR}

uninstall:
	test -f ${BINDIR}/ad1000 && rm ${BINDIR}/ad1000
	test -f ${BINDIR}/lirc_led && rm ${BINDIR}/lirc_led
	test -f ${BINDIR}/ad_display && rm ${BINDIR}/ad_display
