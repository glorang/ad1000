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
#include "ad1000.h"

/* exit on signal */
volatile sig_atomic_t stop;
volatile sig_atomic_t paused;

int main(int argc, char *argv[]) {

        /* Open syslog */
        openlog("timer", LOG_PID|LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "daemon starting up");

        /* Catch shutdown signals */
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        signal(SIGTERM, init_exit);
        signal(SIGUSR1, toggle_pause);

        char errormsg[100];
        /* arg parsing */
        if(argc < 3) { 
                sprintf(errormsg, "paramc = %d, argv1 %s argv2 %s\n", argc, argv[1], argv[2]);
                syslog(LOG_INFO, errormsg);
                syslog(LOG_ERR, "not enough parameters provided"); 
                exit(-1); 
        }

        //char *type = sscanf("%s\n", argv[1]);
        int cur_sec = strtol(argv[2], NULL, 10);
        int cur_min = 0;
        int slept_total = 0;

        if(cur_sec > 60) {
                cur_min = cur_sec / 60;
                cur_sec -= (cur_min*60);
        }

        /* Main loop - increase timing counter */
        while(stop == 0) { 

                if(strcmp(argv[1], "TIMER") == 0) { 
                        char msg[4] = { 0x00 };
                        sprintf(msg, "%02d.%02d", cur_min, cur_sec);
                        if(paused != 1) { 
                                slept_total += update_display(msg);
                                cur_sec++;
                        }
                        if(cur_sec == 60) { cur_sec=0; cur_min++; }
                }

                if(strcmp(argv[1], "CLOCK") == 0) { 
                        time_t t = time(NULL);
                        struct tm tm = *localtime(&t);
                        char msg[4] = { 0x00 };
                        sprintf(msg, "%02d.%02d", tm.tm_hour, tm.tm_min);
                        slept_total += update_display(msg);
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
        /* error message */
        char errormsg[100];

        /* Open display device */
        fp_disp = fopen(DEV_DISP, "w");
        if(!fp_disp) {
                sprintf(errormsg, "Could not open display device file %s\n", DEV_DISP);
                syslog(LOG_WARNING, errormsg);
        }

        fprintf(fp_disp, "%s\n", text);
        fflush(fp_disp);

        /* Close file pointer */
        fclose(fp_disp);

        sleep(1);
        return 1;
}

void init_exit(int signum) {
        stop = 1;
}

void toggle_pause() {
        paused = (paused == 0) ? 1 : 0;
}
