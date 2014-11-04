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
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include "cJSON.h"
#include "ad1000.h"

/* exit on signal */
volatile sig_atomic_t stop;
volatile sig_atomic_t paused;

pid_t prev_pid;
void kill_prev(int parent);
void fork_menu();

int main(int argc, char *argv[]) {

        /* socket vars */
        int sock, i;
        struct sockaddr_in server;
        char server_reply[2000] = { 0x00 };

        /* total tracks */ 
        int playlist_item_count = 0;
        /* current track */
        int current_track;

        /* cJSON vars to parse JSON */
        cJSON *c_root, *c_params, *c_data, *c_method, *c_item, *c_track, *c_artist, *c_artist_item, *c_title, *c_type;
        char *method, *type;
        char ptitle[100] = { 0x00 };

        /* pid to fork off timer helper */
        pid_t pid;

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

        /* Empty display in case there is still something on it for whatever reason */
        update_display("", 0);

        /* Show current selected item on display */
        fork_menu();

        /* Main loop - Wait for data to arrive */    
        while(stop == 0) { 
                if(recv(sock , server_reply , 2000 , 0) < 0) {
                        syslog(LOG_ERR, "could not retrieve data from socket");                
                        break;
                } 

                /* Data received - parse JSON */
                c_root = cJSON_Parse(server_reply);
                if(c_root != NULL) c_method = cJSON_GetObjectItem(c_root, "method");
                if(c_method != NULL) method = c_method->valuestring;

                /* Check what event came in and update display as needed */
                if(strcmp(method, "Playlist.OnClear") == 0) {
                        /* reset vars */
                        playlist_item_count = 0;
                        /* terminater timer process */
                        kill_prev(getpid());
                } else if(strcmp(method, "Playlist.OnAdd") == 0) {
                        playlist_item_count++;
                } else if(strcmp(method, "Player.OnPlay") == 0) {

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

                        /* don't do anything when type is unkown (happens when you press play on a folder instead of item */
                        if(strcmp(type, "unknown") == 0) continue;

                        /* kill previous timer process when we're starting up or track changed */
                        if( (strcmp(c_title->valuestring, ptitle) != 0) || paused == 0){
                                kill_prev(getpid());

                                /* if we're playing a movie show title at the beginning and show clock while playing */
                                if(strcmp(type, "movie") == 0) {

                                        if(c_title != NULL) {
                                                strcpy(ptitle, c_title->valuestring);
                                        }

                                        pid = fork();
                                        if(pid == 0) {
                                                char *cmd = "/usr/local/bin/timer";
                                                char *cmd_args[] = { cmd, "CLOCK", c_title->valuestring, NULL};
                                                execvp(cmd, cmd_args);
                                                _exit(0);
                                        } else { 
                                                prev_pid = pid;
                                        }

                                /* show title, currenttrack.totaltracks and start counter */
                                } else if(strcmp(type, "song") == 0) {

                                        char artist_title[1000] = { 0x00 };

                                        if(c_artist != NULL) {
                                                char *disp_artist = c_artist_item->valuestring;
                                                strcpy(artist_title, disp_artist);
                                        }

                                        if(c_title != NULL) {
                                                char *disp_title = c_title->valuestring; 
                                                strcat(artist_title, " - ");
                                                strcat(artist_title, disp_title);
                                                strcpy(ptitle, disp_title);
                                        }

                                        if(c_track != NULL) {
                                                current_track = c_track->valueint;
                                        } else {
                                                current_track = 1;
                                        }

                                        pid = fork();

                                        if(pid == -1) {
                                                syslog(LOG_ERR, "fork failed!");
                                        }

                                        if(pid == 0) {
                                                char *cmd = "/usr/local/bin/timer";
                                                char ct[4], tt[4];
                                                sprintf(ct, "%d", current_track);
                                                sprintf(tt, "%d", playlist_item_count);
                                                char *cmd_args[] = { cmd, "TIMER", artist_title, ct, tt, NULL};
                                                execvp(cmd, cmd_args);
                                                _exit(0);
                                        } else { 
                                                prev_pid = pid;
                                        }
                                }                         
                        } else {
                                /* if we were previously paused just continue current timer by sending SIGUSR1 */
                                kill(prev_pid, SIGUSR1);
                                paused = 0;
                        }

                } else if(strcmp(method, "Player.OnPause") == 0) {
                        kill(prev_pid, SIGUSR1); 
                        update_display("PAUS", 1000);
                        paused = 1;
                } else if(strcmp(method, "Player.OnStop") == 0) {
                        kill_prev(getpid());
                        update_display("STOP", 1000);
                        update_display("", 0);
                        fork_menu();
                }

                cJSON_Delete(c_root);

                /* zero server_reply again */
                for(i=0;i<sizeof(server_reply);i++) { server_reply[i] = 0x00; }
        }
     
        syslog(LOG_INFO, "caught exit signal - shutting down");

        /* Close syslog */
        closelog();

        /* Close socket */
        close(sock);

        exit(EXIT_SUCCESS);
}

void fork_menu() {

        pid_t pid = fork();
        if(pid == 0) {
                char *cmd = "/usr/local/bin/menu";
                char *cmd_args[] = { cmd, NULL};
                execvp(cmd, cmd_args);
                _exit(0);
        } else { 
                prev_pid = pid;
        }
}


void update_display(char *text, int delay_ms) {
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

        usleep(delay_ms*1000);
}

void init_exit(int signum) {
        stop = 1;
}

void kill_prev(int parent)  {
        /* don't kill our parent process */
        if(prev_pid != parent && prev_pid != 0) { 
                kill(prev_pid, SIGTERM); 
                waitpid(prev_pid, NULL, 0);
        }
}
