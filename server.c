/*
 * server.c -- многопоточный сервер с неблокирующими сокетами TCP и UDP
 * отвечает строкой на запрос клиента
 */
#include "server.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER ;
/* хочу завершить выполнение программы на ^C и освободить память */
static volatile int keepRunning = 1;
void intHandler(int dummy) {
    keepRunning = 0;
}

int main(int argc, char *argv[])
{

    /* десриптор сокета, который мы слушаем */
    int tcplisten;
    int udplisten;
    int threads;
    pthread_t *t;
    int epfd;
    struct tothread thrdinput;
    /* чтобы я мог по ^C выйти из цикла */
    struct sigaction sa;
    sa.sa_handler = &intHandler;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    /* не указан порт */
    if (argc != 3) {
        fprintf(stderr, "%s tcpport udport\n", argv[0]);
        return 1;
    }

    tcplisten = create_tcp_sock(argv[1]);
    udplisten = create_udp_socket(argv[2]);
    if ((epfd = epoll_create(MAX_CON)) == -1) {
            perror("epoll_create");
            exit(1);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = tcplisten;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, tcplisten, &ev) < 0) {
            perror("epoll_ctl");
            exit(1);
    }

    thrdinput.udpsock = udplisten;
    thrdinput.tcpsock = tcplisten;
    thrdinput.progname = argv[0];
    thrdinput.epfd = epfd;


    threads = 1;
    t = malloc(sizeof(pthread_t) * threads);
    pthread_create(&t[0], NULL, pmanage, &thrdinput);

    while (keepRunning) {}

    printf("---++ %d ++---\n", threads);
    printf("\nконечная\n");
    return 0;

}
/* создаем новый сокет */
int create_udp_socket(char *port)
{
    int sockfd;
    int rv;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family,
                             p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket()");
            continue;
        }
        if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
            perror("server, fcntl:");
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind()");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    return sockfd;
}

int create_tcp_sock(char *port)
{
    struct addrinfo hints, *p, *ai;
    int rv;
    int yes;
    int tcplisten;
    memset(&hints, 0, sizeof(hints));

    yes = 1;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    /* заполняем информацию */
    if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        fprintf(stderr, "server: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        tcplisten = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        /* проверяем, получили ли мы сокет */
        if (tcplisten < 0)
            continue;

        if (fcntl(tcplisten, F_SETFL,
                  fcntl(tcplisten, F_GETFL, 0) | O_NONBLOCK) == -1) {
            perror("server, fcntl:");
        }
        /* мы хотим использовать именно этот адресс и порт */
        setsockopt(tcplisten, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(tcplisten, p->ai_addr, p->ai_addrlen) < 0) {
            close(tcplisten);
            continue;
        }
        break;
    }
    /* если у нас p == NULL, то нифига не вышло */
    if (p == NULL) {
        fprintf(stderr, "server: can't bind\n");
        exit(2);
    }

    freeaddrinfo(ai);
    if (listen(tcplisten, BACK_LOG) == -1) {
        perror("server: listen ");
        exit(3);
    }
    return tcplisten;
}

