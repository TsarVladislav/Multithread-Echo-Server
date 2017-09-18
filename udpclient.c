#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFLEN 256
struct addr {
    int sockfd;
    struct addrinfo *p;
    struct addrinfo *servinfo;
};

struct addr create_socket(char *ipaddr, char *port)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct addr ad;
    int rv;


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(ipaddr, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "client: failed to create socket\n");
        exit(2);
    }

    ad.sockfd = sockfd;
    ad.p = p;
    ad.servinfo = servinfo;
    return ad;

}
int main(int argc, char *argv[])
{
 
    struct addr ad;
    int numbytes;
    char msg[100];
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t addr_len;

    if (argc != 3) {
        fprintf(stderr,"usage: client hostname port\n");
        exit(1);
    }

    addr_len = sizeof(struct sockaddr_in);
    ad = create_socket(argv[1], argv[2]);

    if ((numbytes = sendto(ad.sockfd, "ping", strlen("ping"), 0,
             ad.p->ai_addr, ad.p->ai_addrlen)) == -1) {
        perror("client: sendto");
        exit(1);
    }

    printf("sent\n");
    (numbytes = recvfrom(ad.sockfd, msg, BUFLEN, 0,
                         NULL, NULL));
    msg[numbytes] = 0;
    printf("%s\n",msg);

    freeaddrinfo(ad.servinfo);
    close(ad.sockfd);

    return 0;
}
