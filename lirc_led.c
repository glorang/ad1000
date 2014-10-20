/* lirc_led.c                                                                 */
/*                                                                            */
/* Dummy LIRC client to flash LED on IR signal                                */ 
/*                                                                            */
/* Author : Geert Lorang <geert |AT| lorang.be> - 2014-09-29                  */
/*                                                                            */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <lirc/lirc_client.h>

#define DEV_LED "/dev/ad1000/led2"
#define PACKET_SIZE 256

/* function definitions */
void init_exit(int signum);

/* exit on signal */
volatile sig_atomic_t stop;


int main(int argc, char * argv[]) {

        /* file descriptor of lirc socket */
        int lirc_sock;
        /* socket flags */
        int lirc_flags;
        
        /* buffer for received IR code */
        ssize_t len;
        static char buffer[PACKET_SIZE+1]="";
        static int end_len=0;
        char *end;
        
        /* vars to split up received IR code */
        int keycode, rep;
        char button[PACKET_SIZE+1];
        char remote[PACKET_SIZE+1];
        
        /* file pointer for LED */
        FILE *fp_led; 
        
        /* pid & sid */
        pid_t pid, sid;
        /* error message */
        char errormsg[100];
        
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
                fprintf(stderr, "Could not fork to background\n");
                exit(EXIT_FAILURE);
        }
        
        /* If we got a good PID, then we can exit the parent process. */
        if (pid > 0) {
                exit(EXIT_SUCCESS);
        }
        
        /* Change the file mode mask */
        umask(0);
        
        /* Open syslog */
        openlog("lirc_led", LOG_PID|LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "daemon starting up");
        
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
                fprintf(stderr, "setsid() failed\n");
                exit(EXIT_FAILURE);
        }
        
        /* Change the current working directory */
        if ((chdir("/")) < 0) {
                fprintf(stderr, "chrdir() failed\n");
                exit(EXIT_FAILURE);
        }

        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        /* Open LED device file, but only if it exists */
        if(access(DEV_LED, F_OK) != -1) {
                fp_led = fopen(DEV_LED, "w");
                if(!fp_led) { 
                        sprintf(errormsg, "Could not open LED device file %s\n", DEV_LED); 
                        syslog(LOG_ERR, errormsg);
                        exit(EXIT_FAILURE);
                }
        } else {
                sprintf(errormsg, "Could not open LED device file %s\n", DEV_LED); 
                syslog(LOG_ERR, errormsg);
                exit(EXIT_FAILURE);
        }
        
        /* Initiate LIRC */
        if( (lirc_sock = lirc_init("lirc",1)) == -1) {
                syslog(LOG_ERR, "lirc_init() failed");
                exit(EXIT_FAILURE);
        }
        
        /* Set lirc socket to NONBLOCK */ 
        fcntl(lirc_sock,F_SETOWN,getpid());
        lirc_flags=fcntl(lirc_sock,F_GETFL,0);
        if(lirc_flags != -1) {
                fcntl(lirc_sock,F_SETFL,lirc_flags|O_NONBLOCK);
        }
        
        /* catch shutdown signals */
        signal(SIGABRT, init_exit);
        signal(SIGTERM, init_exit);
        
        /* Each time we receive an IR code, blink LED */
        while(stop == 0) {
                len=0;
                do
                {
                        if(strchr(buffer,'\n')==NULL) {
                                len=read(lirc_sock,buffer+end_len,PACKET_SIZE-end_len);
                        }
                        if(len==-1) {
                                if(errno==EAGAIN) break;
                                exit(EXIT_FAILURE);
                        }
                        buffer[len+end_len]=0;
                        if(sscanf(buffer,"%x %x %s %s\n",&keycode, &rep,button,remote) == 4) {

                                end=strchr(buffer,'\n')+1;
                                end_len=strlen(end);
                                memmove(buffer,end,end_len+1);
                                
                                /* FIXME: KEY_UP will not flash */
                                if(strstr(button, "_UP")) {
                                        fprintf(fp_led, "%d\n", 0);
                                } else {
                                        fprintf(fp_led, "%d\n", 1);
                                }
                                fflush(fp_led);
                         }
                }
                while(len!=0);
                /* Sleep 0.1s */
                usleep(100000);
        } 
        
        syslog(LOG_INFO, "caught exit signal - shutting down");
         
        /* Close LED device file */
        fclose(fp_led);
        
        /* Close LIRC connection */
        lirc_deinit();

        /* Close syslog */
        closelog();
        
        exit(EXIT_SUCCESS);
}


void init_exit(int signum) {
        stop = 1;
}
