/* ad1000.c                                                                   */
/*                                                                            */
/* Driver for Telenet AD1000 Front Panel - v0.1                               */
/* This pseudo-driver can control the 4x7 segment display and the 3 leds      */
/* IR is handled by LIRC                                                      */
/*                                                                            */
/* Author : Geert Lorang <geert |AT| lorang.be> - 2014-09-28                  */
/*                                                                            */
/* TODO: Proper stop/start / daemonize                                        */
/*                                                                            */
/* Compile with: gcc ad1000.c -o ad1000 -l bcm2835                            */
/* Stop with: kill $(pidof ad1000)                                            */
/*                                                                            */
/* Note that bcm2835/SPI0 does not support changing byte order (LSB/MSB)      */
/* This means you need to shift each address and/or value manually            */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <bcm2835.h>

/* constant definitions */
/* device files */
#define DEV_DIR "/dev/ad1000/"
#define DEV_LED1 DEV_DIR "led1"
#define DEV_LED2 DEV_DIR "led2"
#define DEV_LED3 DEV_DIR "led3"
#define DEV_DISP DEV_DIR "disp"
#define DEV_DISP_BRIGHTNESS DEV_DIR "disp_brightness"


/* key scanning timings */
/* When we start we read in keydata each MAX_SLEEP (1s) seconds */
/* As soon as a button is pressed we scan for keydata each MIN_SLEEP (0.17s) seconds */
/* After KEY_SCAN_LOW (10s) we scan again every MAX_SLEEP (1s) seconds */
/* This reduces CPU usage drastically, writing every 0.01 on the SPI bus increases CPU usage too much */
#define DELAY_TIME 10000        /* 0.01 second */
#define MIN_SLEEP 170000        /* 0.17 second */
#define MAX_SLEEP 1000000       /* 1 second */
#define KEY_SCAN_LOW 10000000    /* 10 seconds */

/* Brightness levels (low to high) */
const char brightness_levels[] = { 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1 };

/* Define all digits an hex symbols */
const char digits[] = { 0xFC, 0x60, 0xDA, 0xF2,         /* 0 1 2 3 */
                        0x66, 0xB6, 0xBE, 0xE0,         /* 4 5 6 7 */
                        0xFE, 0xF6, 0x02                /* 8 9 -   */
};

/* Define key names of buttons */
/* Those should match with the config in lircd.conf */
const char * keynames[] = { 
         "KEY_LEFT", "KEY_OK",   "KEY_RIGHT", "KEY_UP", 
         "KEY_DOWN", "KEY_MENU", "KEY_PAUSE", "KEY_RECORD",
         "KEY_PREV", "KEY_PLAY", "KEY_NEXT",  "KEY_BACK",
         "KEY_STOP", "KEY_EXIT", "KEY_POWER"
};

/* Define scancode of each button */
const int keycodes[15][3] = { 
         { 0x00, 0x00, 0x80 },       // left   
         { 0x00, 0x00, 0x40 },       // ok     
         { 0x00, 0x00, 0x04 },       // right  
         { 0x04, 0x00, 0x00 },       // up     
         { 0x00, 0x04, 0x00 },       // down   
         { 0x02, 0x00, 0x00 },       // menu   
         { 0x00, 0x40, 0x00 },       // pause  
         { 0x00, 0x02, 0x00 },       // record 
         { 0x40, 0x00, 0x00 },       // prev   
         { 0x08, 0x00, 0x00 },       // play   
         { 0x00, 0x08, 0x00 },       // next   
         { 0x00, 0x00, 0x20 },       // back   
         { 0x00, 0x80, 0x00 },       // stop   
         { 0x00, 0x20, 0x00 },       // exit   
         { 0x80, 0x00, 0x00 }        // power   
};


/*                                                                                   */
/*                 ????  DIG1  PWRG  DIG2  PWRR  DIG3  ????  DIG4  ????  LEDS, ????  */
char display[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*                                                                                   */
/* The PT6958 has 11 * 8 bits of memory available. Those are mapped in the display[] */
/* variable which will be send in it's entire each time somethings needs an update   */
/*                                                                                   */
/* DIG1, DIG2, DIG3 and DIG4 control the 4 x 7 segements displays (DISP)             */
/*                                                                                   */
/* PWRG (Green) and PWRR (Red) control the powerled (LED1).                          */
/* Writing 0xF0 will enable this led.                                                */
/*                                                                                   */
/* LEDS controls the red & green led (LED2 and LED3)                                 */
/* Since they are controlled in a single byte this is how you enable them:           */
/*   0xC0 = green led (1100 0000)                                                    */ 
/*   0x30 = red led   (0011 0000)                                                    */ 
/*   0xF0 = both leds (1111 0000)                                                    */
/*                                                                                   */
/* ???? is probably unused                                                           */


/* default brightness */
int brightness = 2;

/* function definitions */
int create_devs(); 
int remove_devs(); 
void cleanup(); 
int spi_init();
void spi_update();
void spi_end();
void setDisplayOn(int brightness);
void setDisplayOff();

int main() {

        /* Create pseudo device files to control our frontpanel */
        if(create_devs() != 0) {
                return -1;
        } 

        /* Cleanup on exit */
        signal(SIGINT, cleanup);
        signal(SIGABRT, cleanup);
        signal(SIGTERM, cleanup);

        if(spi_init() != 0) {
                printf("Could not initialize SPI driver!\n");
                return -1;
        }

        /* File pointers for each FIFO */
        FILE *fp_led1, *fp_led2, *fp_led3, *fp_disp, *fp_dispbr;
        fp_led1 = fdopen(open(DEV_LED1, O_RDONLY | O_NONBLOCK), "r");
        fp_led2 = fdopen(open(DEV_LED2, O_RDONLY | O_NONBLOCK), "r");
        fp_led3 = fdopen(open(DEV_LED3, O_RDONLY | O_NONBLOCK), "r");
        fp_disp = fdopen(open(DEV_DISP, O_RDONLY | O_NONBLOCK), "r");
        fp_dispbr = fdopen(open(DEV_DISP_BRIGHTNESS, O_RDONLY | O_NONBLOCK), "r");

        if(fp_led1 < 0 || fp_led2 < 0 || fp_led3 < 0 || fp_disp < 0 || fp_dispbr < 0) {
                /* Could not open all FIFOs */
                return -1;
        }

        /* buffers to store input */
        char buffer[8] = { 0x00 } ;
        int led1;
        int led2;
        int led3;
       
        /* int to store bytes read, we only need this for the display text as */
        /* as only this buffer has a possible variable length */
        int bytes_read = 0;

        /* int to lookup key index of pressed button */
        int row;

        /* variables for irsend */
        char command[100];
        int count = 0;
        int prev = -1;

        /* time slept */
        int slept = 0;
        /* delay keyscanning by 'sleepfor' */
        int sleepfor = MAX_SLEEP;

        /* keep track if recently a button was pressed */
        int ks_active = 0;
        int ks_time = 0;


        /* Main loop - read all FIFOs and act as needed */
        while(1) {

                /* here we read in the keys and perform some timing magic */

                /* reset ks_active after KEY_SCAN_LOW time has passed */
                if(ks_time == (KEY_SCAN_LOW)) { ks_time = 0; ks_active = 0; sleepfor = MAX_SLEEP; }
                /* increase ks_time with DELAY_TIME */
                if(ks_active) { ks_time += DELAY_TIME; } 

                //printf("slept %d sleptfor %d kstime %d ksactive %d\n", slept,sleepfor,ks_time, ks_active);

                if(slept == sleepfor) {
                        /* read in key data */
                        char read[]  =  { 0x42, 0xFF, 0xFF, 0xFF };         
                        bcm2835_spi_transfern(read, 4);

                        /* a button was pressed, we set 'sleepfor' to MIN_SLEEP to increase response times on the buttons */
                        if(read[1] != 0x00 || read[2]  != 0x00 || read[3] != 0x00) { 
                                ks_active = 1;
                                sleepfor = MIN_SLEEP; 

                                for(row=0;row<15;row++) {
                                        if(read[1] == keycodes[row][0] && read[2] == keycodes[row][1]  && read[3] == keycodes[row][2]) {

                                                /* keep track of how many times same button is pressed */
                                                if(prev == row) { count++; } else { prev = row; count=0; }
                                                
                                                /* send fake lirc simulate command */
                                                /* FIXME: we use system() - maybe better to write to LIRCd socket? */
                                                /* FIXME: first code in irsend command  is deprecated, but maybe we need it? */
                                                /* FIXME: how to get IR remote name from lirc_client?! */

                                                sprintf(command, "irsend simulate \"0000000000000000 %02x %s DEFAULT\"", count, keynames[row]);
                                                system(command);

                                                /* sleep a bit before reading in next key sequence */
                                                /* this will also delay led/display update for 0.2s, but user is pressing buttons */
                                                usleep(200000);
                                                break;
                                        }
                                }
                        } 
                        slept = 0;
                }

                /* Process LED1 FIFO */
                if(fread(buffer, 2, 1, fp_led1)) {
                        if(sscanf(buffer, "%d", &led1)) {
                                display[2] = 0x00;
                                display[4] = 0x00;

                                switch(led1) {
                                        /* off */ 
                                        case 0: display[2] = 0x00; display[4] = 0x00; break;
                                        /* green */
                                        case 1: display[2] = 0xF0; break;
                                        /* red */
                                        case 2: display[4] = 0xF0; break;
                                        /* orange */
                                        case 3: display[2] = 0xF0; display[4] = 0xF0; break;
                                }

                                spi_update();
                        }
                }

                /* Process LED2 FIFO */
                if(fread(buffer, 2, 1, fp_led2)) {
                        if(sscanf(buffer, "%d", &led2)) {
                                switch(led2) {
                                        /* Both leds 2 & 3 are controlled by a single byte */
                                        /* flip bits 6 & 7 */

                                        /* off */
                                        case 0: display[9] &= ~(1 << 6); display[9] &= ~(1 << 7); break;
                                        /* on */
                                        case 1: display[9] |= 1 << 6;  display[9] |= 1 << 7; break;
                                }
                                spi_update();
                        }
                }

                /* Process LED3 FIFO */
                if(fread(buffer, 2, 1, fp_led3)) {
                        if(sscanf(buffer, "%d", &led3)) {
                                switch(led3) {
                                        /* Both leds 2 & 3 are controlled by a single byte */
                                        /* flip bits 4 & 5 */

                                        /* off */
                                        case 0: display[9] &= ~(1 << 4); display[9] &= ~(1 << 5); break;
                                        /* on */
                                        case 1: display[9] |= 1 << 4; display[9] |= 1 << 5; break;
                                }
                                spi_update();
                        }
                }

                /* Process Brightness FIFO */
                if(fread(buffer, 2, 1, fp_dispbr)) {
                        if(sscanf(buffer, "%d", &brightness)) {
                                /* 8 brightness levels are available, make sure we don't go out-of-index */
                                if(brightness >= 0 && brightness <= 7) {
                                       setDisplayOn(brightness);
                                }
                        }
                }

                /* Process Display FIFO */
                if( (bytes_read = fread(buffer, 1, 9, fp_disp)) ) {
                        /* We can get variable length input. e.g : */
                        /* 234 or 1.2.3.4. or 12.34 */
                        /* so create new buffer with exact length */
                        char result[bytes_read+1];
                        result[bytes_read] = '\0';
                        memcpy(result, buffer, bytes_read);

                        /* we start with the most right digit */
                        int current_digit = 3;

                        /* array for each digit */
                        char dig[4] = { 0x00 } ;
                    
                        int i, num;

                        /* We loop over the input in reverse order */
                        for(i=bytes_read-1;i>=0;i--) {
                                /* Set current digit to to it's binary representation as defined in digits[] */
                                if(result[i] >= 0x30 && result[i] <= 0x39) {
                                        num = result[i] - 0x30;
                                        dig[current_digit] += digits[num];
                                        current_digit--;
                                } else if(result[i] == '-') {
                                        dig[current_digit] += digits[10];
                                        current_digit--;
                                /* to enable the dot you need to flip the last bit (0000 0001) of it's value in digits[] */
                                } else if(result[i] == '.') {
                                        dig[current_digit] += 0x01;
                                }
                        }

                        display[1] = dig[0];
                        display[3] = dig[1];
                        display[5] = dig[2];
                        display[7] = dig[3];
                        setDisplayOn(brightness);
                        spi_update();
                }
                
                /* Sleep DELAY_TIME */
                usleep(DELAY_TIME);
                slept+=DELAY_TIME; 
        }
        return 0;
}


int create_devs() {
        umask(0);
        if(mkdir(DEV_DIR, 0755) != 0) { printf("Could not create %s\n", DEV_DIR); return -1; }
        if(mknod(DEV_LED1, S_IFIFO|0622, 0) != 0) { printf("Could not create %s\n", DEV_LED1); return -1; }
        if(mknod(DEV_LED2, S_IFIFO|0622, 0) != 0) { printf("Could not create %s\n", DEV_LED2); return -1; }
        if(mknod(DEV_LED3, S_IFIFO|0622, 0) != 0) { printf("Could not create %s\n", DEV_LED3); return -1; }
        if(mknod(DEV_DISP, S_IFIFO|0622, 0) != 0) { printf("Could not create %s\n", DEV_DISP); return -1; }
        if(mknod(DEV_DISP_BRIGHTNESS, S_IFIFO|0622, 0) != 0) { printf("Could not create %s\n", DEV_DISP_BRIGHTNESS); return -1; }
        return 0; 
}

int remove_devs() {
        if(unlink(DEV_LED1) != 0) { printf("Could not remove %s\n", DEV_LED1); }
        if(unlink(DEV_LED2) != 0) { printf("Could not remove %s\n", DEV_LED2); }
        if(unlink(DEV_LED3) != 0) { printf("Could not remove %s\n", DEV_LED3); }
        if(unlink(DEV_DISP) != 0) { printf("Could not remove %s\n", DEV_DISP); }
        if(unlink(DEV_DISP_BRIGHTNESS) != 0) { printf("Could not remove %s\n", DEV_DISP_BRIGHTNESS); }
        if(rmdir(DEV_DIR) != 0) {  printf("Could not remove %s\n", DEV_DIR); }
        return 0; 
}

void cleanup() {
        printf("Caught exit signal - cleaning up!\n");
        setDisplayOff();
        spi_end();
        remove_devs();
        exit(0);
}

int spi_init() {
        if (!bcm2835_init())
                return 1;
        
        /* Setup SPI connection & settings */
        bcm2835_spi_begin();
        bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
        bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
        bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_65536);
        bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
        bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

        /* Normal operation, incr addr, write to display */
        bcm2835_spi_transfer(0x02);

        /* Clear memory */
        bcm2835_spi_transfer(0x03);
        char clearmem[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        bcm2835_spi_transfern(clearmem, sizeof(clearmem));

        /* Set default brightness, display on */
        setDisplayOn(2);

        /* Set start address */
        bcm2835_spi_transfer(0x03);

        return 0;
}

void spi_update() {
        bcm2835_spi_transfern(display, 11);
}

void spi_end() {
        bcm2835_spi_end();
        bcm2835_close();
}

void setDisplayOff() {
        bcm2835_spi_transfer(0x01);
}

void setDisplayOn(int brightness) {
        bcm2835_spi_transfer(brightness_levels[brightness]);
}
