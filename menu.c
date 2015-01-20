/* menu.c                                                                     */
/*                                                                            */
/* Display current menu item on display                                       */
/*                                                                            */
/* Author : Geert Lorang <geert |AT| lorang.be> - 2014-11-04                  */
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

int main(int argc, char *argv[]) {

        /* display vars */
        char disp[5] = { 0x00 };
        char p_disp[5] = { 0x00 };

        /* socket vars */
        int sock, i;
        struct sockaddr_in server;
        char server_reply[2000] = { 0x00 };

        /* cJSON vars to parse JSON */
        cJSON *c_root, *c_id, *c_result, *c_cc, *c_label;

        /* Open syslog */
        openlog("menu", LOG_PID|LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "daemon starting up");

        /* Catch shutdown signals */
        prctl(PR_SET_PDEATHSIG, SIGTERM);
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

         
        /* Main loop */
        while(stop == 0) { 

                /* Enter endless loop by requesting each second current menu item */
                char message[] = "{\"jsonrpc\": \"2.0\", \"method\": \"GUI.GetProperties\", \"params\": {\"properties\":[\"currentcontrol\"]}, \"id\": 1}";

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
                
                /* parse label */
                if(c_root != NULL) c_result = cJSON_GetObjectItem(c_root,"result");
                if(c_result != NULL) c_cc = cJSON_GetObjectItem(c_result,"currentcontrol");
                if(c_cc != NULL) c_label = cJSON_GetObjectItem(c_cc,"label");
                
                if(c_label != NULL) { 
                        c_label->valuestring[0] == '[' ? strncpy(disp, c_label->valuestring+1, 4) : strncpy(disp, c_label->valuestring, 4);

                        if(strcmp(disp, p_disp) != 0) { 
                                strncpy(p_disp, disp, 4);
                                update_display(disp, 0);
                        }
                }
                
                cJSON_Delete(c_root);

                usleep(500000);

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

void init_exit(int signum) {
        stop = 1;
}
