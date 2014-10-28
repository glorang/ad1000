/* timer.c                                                                    */
/*                                                                            */
/* Timer module for ad_display.c                                              */ 
/*                                                                            */
/* Author : Geert Lorang <geert |AT| lorang.be> - 2014-10-25                  */
/*                                                                            */

#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include "cJSON.h"
#include "ad1000.h"

/* exit on signal */
volatile sig_atomic_t stop;
volatile sig_atomic_t paused;

/* toggle pause */
void toggle_pause();

int main(int argc, char *argv[]) {

        /* arg parsing */
        if(argc < 3) { 
                syslog(LOG_ERR, "not enough parameters provided"); 
                exit(-1); 
        }
        char *method = (argv[1] == NULL) ? "" : argv[1];
        int cur_track = (argv[3] == NULL) ? 1 : strtol(argv[3], NULL, 10);
        int tot_track = (argv[4] == NULL) ? 1 : strtol(argv[4], NULL, 10);
        
        /* only show track and title once */
        int track_shown = 0; 
        int title_shown = 0;

        /* socket vars */
        int sock, i;
        struct sockaddr_in server;
        char server_reply[2000] = { 0x00 };

        /* cJSON vars to parse JSON */
        cJSON *c_root, *c_id, *c_result, *c_time, *c_milli, *c_minutes, *c_seconds;

        /* Open syslog */
        openlog("timer", LOG_PID|LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "daemon starting up");

        /* Catch shutdown signals */
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        signal(SIGTERM, init_exit);
        signal(SIGUSR1, toggle_pause);

        /* Open socket */
        sock = socket(AF_INET , SOCK_STREAM , 0);
        if (sock == -1) {
                syslog(LOG_ERR, "could not create socket");
        }

        /* Connect to XBMC JSON-RPC API */
        server.sin_addr.s_addr = inet_addr("127.0.0.1");
        server.sin_family = AF_INET;
        server.sin_port = htons(9090);
        connect(sock , (struct sockaddr *)&server , sizeof(server));

         
        /* Main loop - increase timing counter */
        while(stop == 0) { 

                /* Show current_track.total_tracks */
                if(track_shown == 0) { 
                        track_shown = 1;
                        char msg[4] = { 0x00 };
                        sprintf(msg, "%02d.%02d", cur_track, tot_track);
                        update_display(msg, 1000);
                }

                /* Marquee title */
                if(title_shown == 0) { title_shown = 1; update_display(argv[2], 1000); }

                /* If we're a timer, request time @ API and update display */ 
                if(strcmp(method, "TIMER") == 0) { 
                        if(paused != 1) { 

                                /* Enter endless loop by requesting each second current position */
                                char message[] = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.GetProperties\", \"params\": { \"properties\": [\"time\"], \"playerid\": 0 }, \"id\": \"AudioGetItem\"}";
                                if(send(sock, message, strlen(message), 0) < 0) {
                                        syslog(LOG_ERR, "Could not write to socket");
                                }

                                /* read back data */
                                if(recv(sock , server_reply , 2000 , 0) < 0) {
                                        syslog(LOG_ERR, "could not retrieve data from socket");
                                        break;
                                }

                                /* Parse JSON data */
                                c_root = cJSON_Parse(server_reply);
                                if(c_root != NULL) c_id = cJSON_GetObjectItem(c_root,"id");

                                /* The API returned something else than we're interested in, skip iteration */
                                if(c_id == NULL) continue;

                                /* parse minutes, seconds, and millieseconds */
                                if(c_root != NULL) c_result = cJSON_GetObjectItem(c_root,"result");
                                if(c_result != NULL) c_time = cJSON_GetObjectItem(c_result,"time");
                                if(c_time != NULL) c_minutes = cJSON_GetObjectItem(c_time,"minutes");
                                if(c_time != NULL) c_seconds = cJSON_GetObjectItem(c_time,"seconds");
                                if(c_time != NULL) c_milli = cJSON_GetObjectItem(c_time,"milliseconds");

                                if(c_minutes != NULL && c_seconds != NULL && c_milli != NULL) {
                                        char disp[4] = { 0x00 };
                                        sprintf(disp, "%02d.%02d", c_minutes->valueint, c_seconds->valueint);
                                        int sleep_ms = (c_milli->valueint < 300) ? 800 : 200;
                                        if((c_minutes->valueint+c_seconds->valueint) > 0)
                                                update_display(disp, sleep_ms);
                                        //printf("%02d:%02d:%02d - sleeping for : %d\n", c_minutes->valueint, c_seconds->valueint, c_milli->valueint, sleep_ms);
                                }

                                cJSON_Delete(c_root);
                        } else {
                                /* wait until we're waken from pause */
                                sleep(1);
                        }

                }

                /* If we're a clock count from 1000 to 0 and explode, just kidding, show a clock */
                if(strcmp(method, "CLOCK") == 0) { 
                        time_t t = time(NULL);
                        struct tm tm = *localtime(&t);
                        char msg[4] = { 0x00 };
                        sprintf(msg, "%02d.%02d", tm.tm_hour, tm.tm_min);
                        update_display(msg, 0);
                        sleep(60 - tm.tm_sec);
                }

                /* zero server_reply again */
                for(i=0;i<sizeof(server_reply);i++) { server_reply[i] = 0x00; }

        }
     
        syslog(LOG_INFO, "caught exit signal - shutting down");

        /* Close socket */
        close(sock);

        /* Clear display */
        update_display("", 0);

        /* Close syslog */
        closelog();
        exit(0);
}

void update_display(char *text, int delay_ms) {

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

void init_exit(int signum) {
        stop = 1;
}

void toggle_pause() {
        paused = (paused == 0) ? 1 : 0;
}
