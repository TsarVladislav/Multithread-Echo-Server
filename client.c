/*
 *  client.c -- a stream socket client demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>


#define MAXDATASIZE 256 // max number of bytes we can get at once 

int main(int argc, char *argv[])
{
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char *bufptr;
    int b_sent = 0;

    if (argc != 3) {
        fprintf(stderr,"usage: client hostname portname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family,p->ai_addr, s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure
    /*
    printf("enter something: ");
    fgets (buf, MAXDATASIZE, stdin);
    */
    sprintf(buf, "asdasd");

    if((strlen(buf) > 0) && buf[strlen(buf) - 1] == '\n')
        buf[strlen(buf) - 1] = '\0';

    bufptr = buf;
    sleep(1);
    if ((numbytes = send(sockfd, buf, strlen(buf) + 1, 0)) == -1) {
        perror("send");
        exit(1);
    }

    sleep(1);
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    buf[numbytes] = '\0';

    printf("client: received '%s'\n",buf);
    close(sockfd);

    return 0;
}

