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
void update_brightness(int level);

int main(int argc, char *argv[]) {

        /* socket vars */
        int sock, i;
        struct sockaddr_in server;
        char server_reply[2000] = { 0x00 };

        /* cJSON vars to parse JSON */
        cJSON *c_root, *c_method;
        char *method;

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
                        /* terminater timer process */
                        kill_prev(getpid());
               } else if(strcmp(method, "Player.OnPlay") == 0) {

                        kill_prev(getpid());

                        pid = fork();
                        if(pid == -1) {
                                syslog(LOG_ERR, "fork failed!");
                        }

                        if(pid == 0) {
                                char *cmd = "/usr/local/bin/timer";
                                char *cmd_args[] = { cmd, NULL};
                                execvp(cmd, cmd_args);
                                _exit(0);
                        } else { 
                                prev_pid = pid;
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

void update_brightness(int level) {
        FILE *fp_brightness;
        fp_brightness = fopen(DEV_DISP_BRIGHTNESS, "w");
        if(fp_brightness) { 
                fprintf(fp_brightness, "%d\n", level);
                fflush(fp_brightness);
                fclose(fp_brightness);
        }
}
