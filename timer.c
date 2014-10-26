/* timer.c                                                                    */
/*                                                                            */
/* Timer module for ad_display.c                                              */ 
/*                                                                            */
/* Author : Geert Lorang <geert |AT| lorang.be> - 2014-10-25                  */
/*                                                                            */

#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>
#include <math.h>
#include "ad1000.h"

/* exit on signal */
volatile sig_atomic_t stop;
volatile sig_atomic_t paused;

/* toggle pause */
void toggle_pause();

int main(int argc, char *argv[]) {

        /* Open syslog */
        openlog("timer", LOG_PID|LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "daemon starting up");

        /* Catch shutdown signals */
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        signal(SIGTERM, init_exit);
        signal(SIGUSR1, toggle_pause);

        /* arg parsing */
        if(argc < 3) { 
                syslog(LOG_ERR, "not enough parameters provided"); 
                exit(-1); 
        }

        int cur_track = (argv[3] == NULL) ? 1 : strtol(argv[3], NULL, 10);
        int tot_track = (argv[4] == NULL) ? 1 : strtol(argv[4], NULL, 10);

        /* time tracking vars */
        int cur_sec = 0;
        int cur_min = 0;
        
        /* only show track and title once */
        int track_shown = 0; 
        int title_shown = 0; 

        /* Main loop - increase timing counter */
        while(stop == 0) { 

                if(track_shown == 0) { 
                        char msg[4] = { 0x00 };
                        sprintf(msg, "%02d.%02d", cur_track, tot_track);
                        cur_sec += update_display(msg);
                        track_shown = 1;
                }

                if(title_shown == 0) { cur_sec += update_display(argv[2]); title_shown = 1; }

                if(strcmp(argv[1], "TIMER") == 0) { 
                        if(paused != 1) { 
                                char msg[4] = { 0x00 };
                                sprintf(msg, "%02d.%02d", cur_min, cur_sec);
                                update_display(msg);
                                cur_sec++;
                                if(cur_sec == 60) { cur_sec=0; cur_min++; }
                        }
                }

                if(strcmp(argv[1], "CLOCK") == 0) { 
                        time_t t = time(NULL);
                        struct tm tm = *localtime(&t);
                        char msg[4] = { 0x00 };
                        sprintf(msg, "%02d.%02d", tm.tm_hour, tm.tm_min);
                        update_display(msg);
                        sleep(60 - tm.tm_sec);
                }
        }
     
        syslog(LOG_INFO, "caught exit signal - shutting down");

        /* Clear display */
        update_display("");

        /* Close syslog */
        closelog();
        exit(0);
}

int update_display(char *text) {

        /* file pointer for display */
        FILE *fp_disp;
        /* marquee counters */
        int i,k;
        /* dots in text counter */
        int dots;
        /* display text */
        char disp[5]; disp[4] = '\0';
        /* error message */
        char errormsg[100];

        double slept;

        /* Open display device */
        fp_disp = fopen(DEV_DISP, "w");
        if(!fp_disp) {
                sprintf(errormsg, "Could not open display device file %s\n", DEV_DISP);
                syslog(LOG_WARNING, errormsg);
        }

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
                        if(paused == 1) return ceil(slept);
                        if(stop == 1) return ceil(slept);

                        for(k=0;k<4;k++) {
                                disp[k] = new_text[i+k];
                        }

                        fprintf(fp_disp, "%s\n", disp);
                        fflush(fp_disp);
                        usleep(200000); slept+=0.20;
                }
        }

        /* Close file pointer */
        fclose(fp_disp);

        /* sleep up to next 'whole second' */
        usleep((ceil(slept) - slept) * 1000000);

        sleep(1);
        return ceil(slept)+1;
}

void init_exit(int signum) {
        stop = 1;
}

void toggle_pause() {
        paused = (paused == 0) ? 1 : 0;
}
