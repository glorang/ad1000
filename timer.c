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
#include "display.h"

/* exit on signal */
volatile sig_atomic_t stop;
volatile sig_atomic_t paused;

/* toggle pause */
void toggle_pause();
int writeReadSocket(int sock, char *message, char *server_reply);

int main(int argc, char *argv[]) {

        int method = 0;
        
        /* only show startup info once */
        int info_shown = 0;

        /* socket vars */
        int sock, i;
        struct sockaddr_in server;
        char *server_reply;
        server_reply = calloc(1, 2000); 

        /* cJSON vars to parse JSON */
        cJSON *c_root, *c_id, *c_result, *c_time, *c_milli, *c_minutes, *c_seconds, *c_type, *c_label, *c_item, *c_arraysub, *c_artist, *c_artistarray, *c_title, *c_items;

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

         
        /* Main loop */
        while(stop == 0) { 

                /* At startup we show some info about the media that is starting */
                /* For Audio : current_track / total_tracks ; artist - title */
                /* For Video : filename */

                if(info_shown == 0) { 
                        info_shown = 1; 
                        char message[] = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.GetActivePlayers\", \"id\": 1}";
                        writeReadSocket(sock, message, server_reply); 

                        /* Parse JSON data to determine if we're playing audio or video */
                        c_root = cJSON_Parse(server_reply);

                        /* Get player type */
                        if(c_root != NULL) c_result = cJSON_GetObjectItem(c_root,"result");
                        if(c_result != NULL) c_arraysub = cJSON_GetArrayItem(c_result, 0);
                        if(c_arraysub != NULL) c_type = cJSON_GetObjectItem(c_arraysub, "type");

                        if(c_type != NULL) {
                                if(strcmp(c_type->valuestring, "audio") == 0) { method = METHOD_AUDIO; } 
                                if(strcmp(c_type->valuestring, "video") == 0) { method = METHOD_VIDEO; } 
                        }

                        cJSON_Delete(c_root);

                        /* if type is audio, show current_track / total_tracks and after that marquee artist - title */
                        if(method == METHOD_AUDIO) {
                                char playlist_query[] = "{\"jsonrpc\": \"2.0\", \"method\": \"Playlist.GetItems\", \"params\": { \"properties\": [],  \"playlistid\": 0 }, \"id\": 1}";
                                writeReadSocket(sock, playlist_query, server_reply);
                        
                                /* Parse JSON data */
                                c_root = cJSON_Parse(server_reply);
                                if(c_root != NULL) c_result = cJSON_GetObjectItem(c_root,"result");
                                if(c_result != NULL) c_items = cJSON_GetObjectItem(c_result,"items");

                                /* Store ids returned by API in track_ids[] */
                                int track_count, i;
                                track_count = cJSON_GetArraySize(c_items);
                                int track_ids[track_count]; 

                                for (i=0;i<track_count;i++) {
                                                cJSON *subitem=cJSON_GetArrayItem(c_items,i);
                                                cJSON *item = cJSON_GetObjectItem(subitem, "id");
                                                track_ids[i] = (item == NULL) ? 1 : item->valueint;
                                }

                                cJSON_Delete(c_root);

                                char current_item_query[] = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.GetItem\", \"params\": { \"properties\": [\"title\", \"artist\"], \"playerid\": 0 }, \"id\": \"AudioGetItem\"}";
                                writeReadSocket(sock, current_item_query, server_reply);

                                /* Parse JSON data */
                                c_root = cJSON_Parse(server_reply);
                                if(c_root != NULL) c_result = cJSON_GetObjectItem(c_root,"result");
                                if(c_result != NULL) c_item = cJSON_GetObjectItem(c_result,"item");
                                if(c_item != NULL) c_id = cJSON_GetObjectItem(c_item,"id");
                                if(c_item != NULL) c_artist = cJSON_GetObjectItem(c_item,"artist");
                                if(c_item != NULL) c_title = cJSON_GetObjectItem(c_item,"title");
                                if(c_artist != NULL) c_artistarray = cJSON_GetArrayItem(c_artist, 0);

                                /* Display playlist position */
                                if(c_id != NULL) {
                                        int current_track = 1;
                                        for(i=0;i<track_count;i++) {
                                                if(c_id->valueint == track_ids[i]) {
                                                        current_track = i+1;
                                                        break;
                                                }
                                        }

                                        char disp[4] = { 0x00 };
                                        sprintf(disp, "%02d.%02d", current_track, track_count);
                                        update_display(disp, 1000);
                                }

                                /* Display artist - title */
                                char artist_title[1000] = { 0x00 };
                                if(c_artistarray != NULL) {
                                        char *disp_artist = c_artistarray->valuestring;
                                        strcpy(artist_title, disp_artist);
                                }

                                if(c_title != NULL) {
                                        char *disp_title = c_title->valuestring; 
                                        strcat(artist_title, " - ");
                                        strcat(artist_title, disp_title);
                                } 

                                update_display(artist_title, 1000);
                                cJSON_Delete(c_root);
                        }

                        /* if type is video, update display with title */
                        if(method == METHOD_VIDEO) {
                                char message[] = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.GetItem\", \"params\": { \"properties\": [\"title\"], \"playerid\": 1 }, \"id\": \"VideoGetItem\"}";
                                writeReadSocket(sock, message, server_reply);

                                /* Parse JSON data */
                                c_root = cJSON_Parse(server_reply);
                                if(c_root != NULL) c_result = cJSON_GetObjectItem(c_root,"result");
                                if(c_result != NULL) c_item = cJSON_GetObjectItem(c_result,"item");

                                /* Update display with title */
                                if(c_item != NULL) { 
                                        c_label = cJSON_GetObjectItem(c_item,"label");
                                        update_display(c_label->valuestring, 1000); 
                                }

                                cJSON_Delete(c_root);
                        }
                }

                if(paused != 1) { 
                        /* If we're a timer, request time @ API and update display */ 
                        if(method == METHOD_AUDIO) {

                                /* Enter endless loop by requesting each second current position */
                                char message[] = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.GetProperties\", \"params\": { \"properties\": [\"time\"], \"playerid\": 0 }, \"id\": \"AudioGetItem\"}";
                                writeReadSocket(sock, message, server_reply);

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

                        }

                        /* If we're a clock count from 1000 to 0 and explode, just kidding, show a clock */
                        if(method == METHOD_VIDEO) {
                                time_t t = time(NULL);
                                struct tm tm = *localtime(&t);
                                char msg[4] = { 0x00 };
                                sprintf(msg, "%02d.%02d", tm.tm_hour, tm.tm_min);
                                update_display(msg, 0);
                                sleep(60 - tm.tm_sec);
                        }

                } else {
                        /* wait until we're waken from pause */
                        sleep(1);
                }

                /* zero server_reply again */
                for(i=0;i<sizeof(server_reply);i++) { server_reply[i] = 0x00; }

        }
     
        syslog(LOG_INFO, "caught exit signal - shutting down");

        /* Free server_reply */
        free(server_reply);

        /* Close socket */
        close(sock);

        /* Clear display */
        update_display("", 0);

        /* Close syslog */
        closelog();
        exit(0);
}

int writeReadSocket(int sock, char *message, char *server_reply) { 

        /* Write message to socket */
        if(send(sock, message, strlen(message), 0) < 0) { return -1; }

        /* read back data */
        if(recv(sock , server_reply , 2000 , 0) < 0) { return -1; }

        return 0;
}

void init_exit(int signum) {
        stop = 1;
}

void toggle_pause() {
        paused = (paused == 0) ? 1 : 0;
}
