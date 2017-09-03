#ifndef _NON_BLOCK_SERVER_H_
#define _NON_BLOCK_SERVER_H_
#define _GNU_SOURCE 
#define _XOPEN_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>

/* пусть будет запросов, обрабатываемвых в очереди будет столько */
#define MAX_CON (10)
#define BUFSIZE 256
#define BACK_LOG 1024
/* Поток, обрабатывающий запросы.
 * Аргументы: имя программы, сокет TCP и сокет UDP
 */

enum states{RUNNING, INTHREAD, NEWTHREAD};

extern pthread_mutex_t mutex;
int state;
struct quebuf{
    long type;
    int sockfd;
};
struct tothread{
    char *progname;
    int tcpsock;
    int udpsock;
};


/*
 * TODO: сделать возможность запуска демоном
 * void daemonize();
 */
int create_tcp_sock(char *port);
int create_udp_socket(char *port);

void *pmanage(void *args);
void strret(char *str);
int todequeue(int msquid, int id);
void toqueue(int msqid, int sockfd, int id);
int gettcpmsg(int sockfd, char *str);
void sendtcpmsg(int sockfd, char *str);
void getudpmsg(int sockfd, char *buf,
               struct sockaddr *their_addr,
               socklen_t *addr_len);

void sendudpmsg(int sockfd, char *fp, struct  sockaddr *their_addr, socklen_t *addrlen);

int create_msqid(char *progname, int pid);
#endif
