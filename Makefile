# Makefile for ad1000

# Set compiler and options
CC=/usr/bin/gcc
CFLAGS=-Wall -g
INSTALL=/usr/bin/install
BINDIR=/usr/local/bin

all: lirc_led ad1000 api_display media_info menu

ad1000: 
	$(CC) $(CFLAGS) -o ad1000 ad1000.c -lbcm2835 

lirc_led: 
	$(CC) $(CFLAGS) -o lirc_led lirc_led.c -llirc_client

api_display: 
	$(CC) $(CFLAGS) -o api_display api_display.c cJSON.c display.c -lm

media_info: 
	$(CC) $(CFLAGS) -o media_info media_info.c cJSON.c display.c -lm

menu: 
	$(CC) $(CFLAGS) -o menu menu.c cJSON.c display.c -lm

clean:
	rm -f ad1000 lirc_led api_display media_info menu

install:
	test ! -f ${BINDIR}/ad1000 && $(INSTALL) ad1000 ${BINDIR}
	test ! -f ${BINDIR}/lirc_led && $(INSTALL) lirc_led ${BINDIR}
	test ! -f ${BINDIR}/api_display && $(INSTALL) api_display ${BINDIR}
	test ! -f ${BINDIR}/media_info && $(INSTALL) media_info ${BINDIR}
	test ! -f ${BINDIR}/menu && $(INSTALL) menu ${BINDIR}

uninstall:
	test -f ${BINDIR}/ad1000 && rm ${BINDIR}/ad1000
	test -f ${BINDIR}/lirc_led && rm ${BINDIR}/lirc_led
	test -f ${BINDIR}/api_display && rm ${BINDIR}/api_display
	test -f ${BINDIR}/media_info && rm ${BINDIR}/media_info
	test -f ${BINDIR}/menu && rm ${BINDIR}/menu
