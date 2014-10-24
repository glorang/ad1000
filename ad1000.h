/*                                        */
/* Constants                              */
/*                                        */

/* Device files */
#define DEV_DIR "/dev/ad1000/"
#define DEV_LED1 DEV_DIR "led1"
#define DEV_LED2 DEV_DIR "led2"
#define DEV_LED3 DEV_DIR "led3"
#define DEV_DISP DEV_DIR "disp"
#define DEV_DISP_BRIGHTNESS DEV_DIR "disp_brightness"

/* key scanning timings */
#define DELAY_TIME 	   10000       /* 0.01 second */
#define MIN_SLEEP 	  170000       /* 0.17 second */
#define MAX_SLEEP      	 1000000       /* 1 second */
#define KEY_SCAN_LOW 	10000000       /* 10 seconds */

/* LIRC packet size */
#define PACKET_SIZE 256

/*                                        */
/* function definitions                   */
/*                                        */

/* function to exit each daemon */
extern void init_exit(int signum);

/* create/remove device files */
extern int create_devs();
extern int remove_devs();

/* SPI start/stop functions */
extern int spi_init();
extern void spi_end();

/* Send update to display/leds over SPI bus */
extern void spi_update();

/* Set Display on/off */
extern void setDisplayOn(int brightness);
extern void setDisplayOff();

/* write XBMC API data on display (ad_display.c) */
extern void update_display(char *text);
