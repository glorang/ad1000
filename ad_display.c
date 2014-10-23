/* ad_display.c                                                               */
/*                                                                            */
/* XBMC JSON-RPC API interface to ad1000 display                              */
/*                                                                            */
/* Author : Geert Lorang <geert |AT| lorang.be> - 2014-10-23                  */
/* cJSON (c) 2009 Dave Gamble - See LICENSE for more information              */
/*                                                                            */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include "cJSON.h"

#define DEV_DISP "/dev/ad1000/disp"

/* function definitions */
void init_exit(int signum);
void update_display(char *text);

/* exit on signal */
volatile sig_atomic_t stop;

int main(int argc, char *argv[]) {

        /* socket vars */
        int sock;
        struct sockaddr_in server;
        char server_reply[2000];

        /* total tracks */ 
        int playlist_item_count = 0;
        /* current track */
        int current_track;
        /* currenttrack.totaltracks */ 
        char disp[8]; 

        /* cJSON vars to parse JSON */
        cJSON *c_root, *c_params, *c_data, *c_method, *c_item, *c_track, *c_artist, *c_artist_item, *c_title, *c_type;
        char *method, *type;

        /* Open syslog */
        openlog("ad_display", LOG_PID|LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "daemon starting up");

        /* Catch shutdown signals */
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        signal(SIGABRT, init_exit);
        signal(SIGTERM, init_exit);

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


        /* Main loop - Wait for data to arrive */    
        while(stop == 0) { 
                if(recv(sock , server_reply , 2000 , 0) < 0) {
                        syslog(LOG_ERR, "could not retreive data from socket");                
                        break;
                } 

                /* Data received - parse JSON */
                c_root = cJSON_Parse(server_reply);
                if(c_root != NULL) c_method = cJSON_GetObjectItem(c_root, "method");
                if(c_method != NULL) method = c_method->valuestring;


                /* Check what event came in and update display as needed */
                if(strcmp(method, "Playlist.OnClear") == 0) {
                        playlist_item_count = 0;
                } else if(strcmp(method, "Playlist.OnAdd") == 0) {
                        playlist_item_count++;
                } else if(strcmp(method, "Player.OnPlay") == 0) {
                        update_display("PLAY");
                        /* parse track, artist and title */
                        if(c_root != NULL) c_params = cJSON_GetObjectItem(c_root,"params");
                        if(c_params != NULL) c_data = cJSON_GetObjectItem(c_params,"data");
                        if(c_data != NULL) c_item = cJSON_GetObjectItem(c_data,"item");
                        if(c_item != NULL) c_type = cJSON_GetObjectItem(c_item,"type");
                        if(c_item != NULL) c_track = cJSON_GetObjectItem(c_item,"track");
                        if(c_item != NULL) c_title = cJSON_GetObjectItem(c_item,"title");
                        if(c_item != NULL) c_artist = cJSON_GetObjectItem(c_item, "artist");
                        if(c_artist != NULL) c_artist_item = cJSON_GetArrayItem(c_artist, 0);
                
                        type = (c_type == NULL) ? "" : c_type->valuestring;

                        /* if we're playing a movie just show title at the beginning */
                        if(strcmp(type, "movie") == 0) {
                                if(c_title != NULL) update_display(c_title->valuestring); 

                        /* show title and keep currenttrack.totaltracks while song is playing */
                        } else if(strcmp(type, "song") == 0) {

                                if(c_artist != NULL && c_title != NULL) {                 
                                        char *disp_artist = c_artist_item->valuestring;
                                        char *disp_title = c_title->valuestring; 
                                        int len = strlen(disp_artist) + strlen(disp_title) + 4;
                                        char artist_title[len]; artist_title[len] = '\0';
                                        strcpy(artist_title, disp_artist);
                                        strcat(artist_title, " - ");
                                        strcat(artist_title, disp_title);
                                        update_display(artist_title);
                                }

                                current_track = (c_track == NULL) ? 1 : c_track->valueint;
                                sprintf(disp, "%02d.%02d", current_track, playlist_item_count);
                                update_display(disp);
                        }


                } else if(strcmp(method, "Player.OnPause") == 0) {
                        update_display("PAUS");
                } else if(strcmp(method, "Player.OnStop") == 0) {
                        update_display("STOP");
                        update_display("");
                }
        }
     
        syslog(LOG_INFO, "caught exit signal - shutting down");

        /* Close syslog */
        closelog();

        /* Close socket */
        close(sock);

        exit(EXIT_SUCCESS);
}

void update_display(char *text) {

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
                return;
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
                        for(k=0;k<4;k++) {
                                disp[k] = new_text[i+k];
                        }

                        fprintf(fp_disp, "%s\n", disp);
                        fflush(fp_disp);
                        usleep(200000);
                }
        }

        /* Close file pointer */
        fclose(fp_disp);

        sleep(2);
}

void init_exit(int signum) {
        stop = 1;
}
