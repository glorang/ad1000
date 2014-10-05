/* lirc_led.c                                                                 */
/*                                                                            */
/* Dummy LIRC client to flash LED on IR signal                                */ 
/* Needs additional work on timings                                           */
/*                                                                            */
/* Author : Geert Lorang <geert |AT| lorang.be> - 2014-09-29                  */
/*                                                                            */
/* Compile with gcc -o lirc lirc.c -l lirc_client                             */
/*                                                                            */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lirc/lirc_client.h>

#define DEV_LED "/dev/ad1000/led2"

int main(int argc, char * argv[]) {

char *code;
FILE *fp_led = fopen(DEV_LED, "w");
int keycode, sequence;
char key, remote;

// Open LED device file
if(!fp_led) { 
        printf("Could not open %s\n", DEV_LED);
        return -1;
}

// Initiate LIRC. 
if(lirc_init("lirc",1) == -1) {
        printf("Could not init LIRC\n");
	return -1;
}

// Each time we receive an IR code, blink LED
while(lirc_nextcode(&code)==0)
{
        //If code = NULL, meaning nothing was returned from LIRC socket,
        //then skip lines below and start while loop again.
        if(code == NULL) continue;  

                /* Only blink on first occurence to save CPU cycles */
                if(sscanf(code, "%d %d %s %s",&keycode, &sequence, key, remote)) {
                        if(sequence == 0) {
                                fprintf(fp_led, "%d\n", 1);
                                fflush(fp_led);

                                usleep(100000);

                                fprintf(fp_led, "%d\n", 0);
                                fflush(fp_led);
                         }
                }

        //Need to free up code before the next loop
        free(code);
}

// Close LIRC connection
lirc_deinit();

// Close LED device file
fclose(fp_led);

return 0;
}

