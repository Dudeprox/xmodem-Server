
#include "helper.h"
#include "xmodemserver.h"  
#include "crc16.h"  


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/signal.h>


int port = 53514;
static int listenfd;

struct client *top = NULL;


static void addclient(int fd);
static void removeclient(int fd);


int main() {

    extern void bindandlisten(), newconnection();

    bindandlisten();

    // Muffinman Inspired Structure with Modifications of States 
    // to behave like xmodemserver type.
    
    char temp;
    while(1){
        fd_set fds;
        int maxfd = listenfd;

        FD_ZERO(&fds);
        
        FD_SET(listenfd, &fds);
        struct client *p = top;
        while (p != NULL){
            FD_SET(p->fd, &fds);
            if (p->fd > maxfd){
                maxfd = p->fd;
            }
            p = p->next;
        }

        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0) {
	        perror("select");
	    } else {
            printf("Client Detected!\n");
            p = top; 
            while (p != NULL){
                if (FD_ISSET(p->fd, &fds)){

                    if (p->state == initial){
                        
                        memset(p->buf, 0, 2048);
                        p->inbuf = 0;
                        memset(p->filename, '\0', 20);
               
                        
                        // Read File Name from the client
                        p->inbuf += read(p->fd, p->buf + p->inbuf, 2048-p->inbuf);
                        p->buf[p->inbuf] = '\0';

                        // Read any remaining data, which was unread by the socket (ASK TA)
                        while(strstr(p->buf, "\r\n") == NULL) {
                            p->inbuf += read(p->fd, p->buf + p->inbuf, 2048-p->inbuf);
                            p->buf[p->inbuf]='\0';
                        }

                        if (p->inbuf > 21){
                            p->state = finished;
                        }
                        else {

                            strncpy(p->filename, p->buf, p->inbuf-2);

                            p->filename[p->inbuf-2] = '\0';
                            p->inbuf -= 2;

                            memset(p->buf, 0, 2048);
                            p->inbuf = 0;

                            // Open the file with the given filename for writing. (Maybe Open it in some directory)
                            FILE *fp = open_file_in_dir(p->filename, "filestore");
                            if (!fp) {
                                perror("fopen");
                                exit(2);
                            }
                            // send a C to the client
                            write(p->fd, "C", 1);
                            // transition to pre_block with first block that the server expects is block number 1.
                            p->fp = fp;
                            p->state = pre_block;
                           
                        }

                    } 
                    if (p->state == pre_block) {
                        
                        int len;

                        while ((len = read(p->fd, &temp, 1)) > 0) {
                            if (temp == EOT) {
                                temp = ACK;
                                write(p->fd, &temp, 1);
                                printf("Client found EOT; entering finished\n");
                                p->state = finished;
                                break;
                            } else if (temp == SOH) {
                                printf("Client found SOH; entering get_block\n");
                                p->state = get_block;
                                break;
                            } else if (temp == STX) {
                                printf("Client found STX; entering get_block\n");
                                p->state = get_block;
                                break;
                            }
                        }

                    } 
                    if (p->state == get_block) {

                    p->inbuf += read(p->fd, p->buf + p->inbuf, 2048-p->inbuf);

                    if (p->inbuf == 132){
                        printf("Server Read 132 bytes\n");
                        p->blocksize = p->inbuf;
                        p->state = check_block;
                    }
                    else if (p->inbuf == 1028) {
                        printf("Server Read 1028 bytes\n");
                        p->blocksize = p->inbuf;
                        p->state = check_block;
                    }
                    }
                    if (p->state == check_block) {

                    unsigned char high_byte;
                    unsigned char low_byte;

                    unsigned short crc = crc_message(XMODEM_KEY, (unsigned char *)(p->buf + 2), p->blocksize - 4);
                    high_byte = crc >> 8;
                    low_byte = crc; 
                    
                    if (((unsigned char) 255 - (unsigned char)p->buf[0]) != (unsigned char) p->buf[1]){

                        printf("Block number and inverse do not correspond\n");
                        p->state = finished;
                    }
                    else if((unsigned char) p->buf[0] == (unsigned char) p->current_block){
                        printf("Block number is the same as the block number of the previous block\n");
                        temp = ACK;
                        write(p->fd, &temp, 1);
                        p->state = pre_block;
                    }
                    else if((unsigned char) p->buf[0] != (unsigned char) (p->current_block + 1)){
                        printf("Block number is not the one that is expected to be next\n");
                        p->state = finished;
                    }
                    else if ((high_byte != ((unsigned char)p->buf[p->blocksize - 2])) || (low_byte != ((unsigned char)p->buf[p->blocksize - 1]))){
                        printf("CRC16 is incorrect\n");
                        temp = NAK;
                        write(p->fd, &temp, 1);
                        p->state = pre_block;
                        
                    }
                    else {
                        printf("Check Complete: Status OK. Writing Payload...\n");


                        int error = 0;
                        error = fwrite(p->buf + 2, sizeof(char), p->blocksize - 4, p->fp);
                        if (error != p->blocksize - 4){
                            fprintf(stderr, "Error: Could Not Write Data Succesfully\n");
                            exit(2);
                        }

                        printf("Writing Payload Complete: Status OK.\n");


                        p->current_block++;
                        if (p->current_block > 255) {
                            p->current_block = 0;
                        }

                        printf("Sending ACK...\n");
                        temp = ACK;
                        write(p->fd, &temp, 1);
                        p->state = pre_block;

                        memset(p->buf, 0, 2048);
                        p->inbuf = 0;
                    }
                    } 
                    
                    if (p->state == finished) {   
                        removeclient(p->fd);
                        printf("Client removed.");
                        fflush(stdout);
                    }    
                }
                if(p != NULL){
                    p = p->next;
                }
                
            }
            
            if (FD_ISSET(listenfd, &fds)){
                newconnection();
            }
            
	    }
    }
    return(0);
    
}

void bindandlisten()  // Taken Directly From Muffinman with a few modications to reuse port.
{
    printf("Server Binding\n");
    struct sockaddr_in r;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }
    int on = 1;
    int status = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
        (const char *) &on, sizeof(on));
    if (status < 0) {
        perror("setsockopt");
        exit(1);
    }


    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
	perror("bind");
	exit(1);
    }

    if (listen(listenfd, 5)) {
	perror("listen");
	exit(1);
    }
    printf("Server is running on port %d...\n", port);
}

static void removeclient(int fd)
{   
    struct client *p = top;

    if (p->fd == fd){ // If the top of the linked list is the removing client
        close(p->fd);
        if (p->fp != NULL){
            fclose(p->fp);
        }
        p = p->next;
        free(top);
        top = p;
    }
    else{
        while (p->next->fd != fd){
            p = p->next;
        }
        close(p->next->fd);
        if (p->next->fp != NULL){
            fclose(p->next->fp);
        }
        struct client *temp = p->next;
        p->next = p->next->next;
        free(temp);
    }
   
}

static void addclient(int fd)
{
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        fprintf(stderr, "out of memory!\n");
        exit(1);
    }
    printf("Adding client ...\n");
    p->fd = fd;
    memset(p->buf, 0, 2048);
    p->inbuf = 0;
    p->fp = NULL;

    memset(p->filename, '\0', 20);
    p->state = initial;
    p->blocksize = 0;
    p->current_block = 0;
    p->next = top;
    top = p;
    printf("Client Added\n");

}

void newconnection()
{
    int fd;
    struct sockaddr_in r;
    socklen_t socklen = sizeof r;

    if ((fd = accept(listenfd, (struct sockaddr *)&r, &socklen)) < 0) {
	    perror("accept");
    } 
    else {
        printf("Connecting New Client...\n");
        addclient(fd);
        printf("Client Connected! Status: OK\n");
    }
}
