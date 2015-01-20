/* display.c                                                                  */
/*                                                                            */
/* General functions for display (marquee text & brightness)                  */
/*                                                                            */
/* Author : Geert Lorang <geert |AT| lorang.be> - 2015-01-20                  */
/*                                                                            */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "ad1000.h"
#include "display.h"

volatile sig_atomic_t stop;
volatile sig_atomic_t paused;

/* Function to update brightness level */
void update_brightness(int level) {

        /* file pointer for brightness */
        FILE *fp_brightness;
	
	/* Open brightness device */
        fp_brightness = fopen(DEV_DISP_BRIGHTNESS, "w");

	/* Set brightness */
        if(fp_brightness) {
                fprintf(fp_brightness, "%d\n", level);
                fflush(fp_brightness);
                fclose(fp_brightness);       
	}
}

/* Function to update display with 'text' and keep it there for 'delay_ms' ms */
/* If 'text' is longer than 4 characters text will marquee over the display */
void update_display(char *text, int delay_ms) {

        /* file pointer for display */
        FILE *fp_disp;
        /* marquee counters */
        int i,k;
        /* dots in text counter */
        int dots;
        /* display text */
        char disp[5]; disp[4] = '\0';

        /* Open display device */
        fp_disp = fopen(DEV_DISP, "w");
        if(fp_disp) {
                /* calculate dots in text */
                char *p=text;
                for (dots=0; p[dots]; p[dots]=='.' ? dots++ : *p++);

                if((strlen(text)-dots) <= 4) {
                        fprintf(fp_disp, "%s\n", text);
                        fflush(fp_disp);
                } else {
                        /* marquee */
                        /* add 4 spaces at the end to complete the marquee effect */
                        int text_len = strlen(text) + 5;
                        char new_text[text_len]; new_text[text_len] = '\0';
                        strcpy(new_text, text);
                        strcat(new_text, "    ");
                        /* Marquee each 4 charachters and sleep 0.2s */
                        for(i=0;i<strlen(new_text);i++) {

                                /* in case we're stopped or paused was pressed when we're marqueeing the title return immediately */
                                if(paused == 1 || stop == 1) {
                                        fprintf(fp_disp, "\n");
                                        fflush(fp_disp);
                                        fclose(fp_disp);
                                        return;
                                }

                                for(k=0;k<4;k++) {
                                        disp[k] = new_text[i+k];
                                }

                                fprintf(fp_disp, "%s\n", disp);
                                fflush(fp_disp);
                                usleep(200000);
                        }
                }

                usleep(delay_ms*1000);

                /* Close file pointer */
                fclose(fp_disp);
        }
}
