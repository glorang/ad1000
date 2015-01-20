/* api_display.c                                                              */
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
#include "display.h"

#define DAEMON_MENU "/usr/local/bin/menu"
#define DAEMON_MEDIA_INFO "/usr/local/bin/media_info"

/* exit on signal */
volatile sig_atomic_t stop;
volatile sig_atomic_t paused;

pid_t prev_pid;
void kill_prev(int parent);
int fork_daemon(char *cmd);
void update_brightness(int level);

int main(int argc, char *argv[]) {

        /* socket vars */
        int sock, i;
        struct sockaddr_in server;
        char server_reply[2000] = { 0x00 };

        /* cJSON vars to parse JSON */
        cJSON *c_root, *c_method, *c_result, *c_screensaver;
        char *method;

        /* Open syslog */
        openlog("api_display", LOG_PID|LOG_CONS, LOG_USER);
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




        /* Check if something is currently playing */
        char message[] = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.GetActivePlayers\", \"id\": 1}";
        if(send(sock, message, strlen(message), 0) < 0) { 
                syslog(LOG_ERR, "could not write to XBMC socket!");
        } 

        if(recv(sock , server_reply , 2000 , 0) < 0) { 
                syslog(LOG_ERR, "could not read from XBMC socket!");
        }
        
        /* Parse JSON data to determine if something is playing */
        c_root = cJSON_Parse(server_reply);
        if(c_root != NULL) c_result = cJSON_GetObjectItem(c_root,"result");

        /* Nothing is playing - show menu */
        if(cJSON_GetArraySize(c_result) == 0) { 
                if(!fork_daemon(DAEMON_MENU)) { 
                        syslog(LOG_ERR, "could not fork menu daemon!"); 
                }
        /* Something is playing - show media info */
        } else { 
                if(!fork_daemon(DAEMON_MEDIA_INFO)) { 
                        syslog(LOG_ERR, "could not fork media_info daemon!"); 
                }
        }
        cJSON_Delete(c_root);



        /* Check if screensaver currently active or not */
        char ss_message[] = "{\"jsonrpc\": \"2.0\", \"method\": \"XBMC.GetInfoBooleans\", \"params\": { \"booleans\": [\"System.ScreenSaverActive\"] }, \"id\": 1}";
        if(send(sock, ss_message, strlen(ss_message), 0) < 0) { 
                syslog(LOG_ERR, "could not write to XBMC socket!");
        } 

        if(recv(sock , server_reply , 2000 , 0) < 0) { 
                syslog(LOG_ERR, "could not read from XBMC socket!");
        }
        
        /* Parse JSON data to check current screensaver state */
        c_root = cJSON_Parse(server_reply);
        if(c_root != NULL) c_result = cJSON_GetObjectItem(c_root,"result");
        if(c_result != NULL) c_screensaver = cJSON_GetObjectItem(c_result,"System.ScreenSaverActive");
        if(c_screensaver->type == cJSON_True) {
                update_brightness(0);
        } else {
                update_brightness(2);
        }

        cJSON_Delete(c_root);




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

                /* Check what event came in and act as needed */
                if(strcmp(method, "Playlist.OnClear") == 0) {

                        /* terminater media_info or menu process */
                        kill_prev(getpid());

                } else if(strcmp(method, "Player.OnPlay") == 0) {

                        kill_prev(getpid());
                        if(!fork_daemon(DAEMON_MEDIA_INFO)) { 
                                syslog(LOG_ERR, "could not fork media_info daemon!"); 
                        }

                } else if(strcmp(method, "Player.OnPause") == 0) {

                        kill(prev_pid, SIGUSR1); 
                        update_display("PAUS", 1000);
                        paused = 1;

                } else if(strcmp(method, "Player.OnStop") == 0) {

                        kill_prev(getpid());
                        update_display("STOP", 1000);
                        update_display("", 0);

                        /* Show current selected item on display */
                        if(!fork_daemon(DAEMON_MENU)) {
                                syslog(LOG_ERR, "could not fork menu daemon!");
                        }


                } else if(strcmp(method, "GUI.OnScreensaverActivated") == 0) {

                        update_brightness(0);

                } else if(strcmp(method, "GUI.OnScreensaverDeactivated") == 0) {

                        update_brightness(2);
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

int fork_daemon(char *cmd) {
        pid_t pid = fork();

        if(pid == -1) {
                return -1;
        }

        if(pid == 0) {
                char *cmd_args[] = { cmd, NULL};
                execvp(cmd, cmd_args);
                _exit(0);
        } else { 
                prev_pid = pid;
        }

        return 0;
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
