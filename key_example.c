/* key_example.c */
/* Quick hack of key implementation, key reading works \o/ */

#include <bcm2835.h>
#include <stdio.h>

const char * keynames[] = { "KEY_LEFT", "KEY_OK", "KEY_RIGHT", "KEY_UP", 
                            "KEY_DOWN", "KEY_MENU", "KEY_PAUSE", "KEY_RECORD",
                            "KEY_PREV", "KEY_PLAY", "KEY_NEXT", "KEY_BACK",
                            "KEY_STOP", "KEY_EXIT", "KEY_POWER"
                          };

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


int main(int argc, char **argv)
{
        if (!bcm2835_init())
                return 1;
   

        // Setup SPI connection & settings
        bcm2835_spi_begin();
        bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST); // no effect
        bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
        bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_65536);
        bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
        bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

        int i;

        while(1) {
                /* read key data */
                char read[]  =  { 0x42, 0xFF, 0xFF, 0xFF };         
                bcm2835_spi_transfern(read, sizeof(read));

                /* lookup keycode */
                for(i=0;i<15;i++) {
                        if(read[1] == keycodes[i][0] && read[2] == keycodes[i][1]  && read[3] == keycodes[i][2]) {
                                printf("key found : %s\n", keynames[i]);
                                break;
                        }
                }

                bcm2835_delay(170);

        }

        bcm2835_spi_end();
        bcm2835_close();
        return 0;
}

