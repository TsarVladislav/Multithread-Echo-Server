#include "server.h"
void *pmanage(void *args)
{
    struct tothread inargs;
    struct epoll_event *events;
    struct sockaddr_in clientaddr;

    struct msqid_ds bf; 
    int fdmax;
    int listener;
    int newfd;
    char buf[1024];
    int nbytes;
    socklen_t addrlen;
    int yes;
    int msquid;
    int epfd = -1;
    struct epoll_event ev;
    int index = 0;
    int client_fd = -1;

    int connections =  1;
    inargs = *((struct tothread *)args);
    fdmax = listener = inargs.tcpsock;
    
    /* нарежем память под те события, которые мы готовы обработать за раз */
    events = malloc(MAX_CON * sizeof(struct epoll_event));
    /* создадим очередь */
    if ((epfd = epoll_create(MAX_CON)) == -1) {
            perror("epoll_create");
            exit(1);
    }
    ev.events = EPOLLIN;
    ev.data.fd = fdmax;
    /* добавляем новый дескриптор */
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fdmax, &ev) < 0) {
            perror("epoll_ctl");
            exit(1);
    }

    /* создаем новую очередь сообщений.
     * В ней будем хранить все TCP сокеты. UDP сокеты обрабатываем
     * сразу по получении, поэтому хранить их тут не вижу смысла 
     * выкидывать сокеты будем по мере необходимости.
     * В случае экстренного звершения программы мы должны закрыть все сокеты.
     */
    msquid = create_msqid(inargs.progname, pthread_self());
    msgctl(msquid, IPC_STAT, &bf);
    if (bf.msg_qnum != 0) {
        msgctl(msquid, IPC_RMID, NULL);
        msquid = create_msqid(inargs.progname, pthread_self());
    }
    for(;;){

        /* ждем пока не придет событие */
        /* epfd - указатель на структуру epoll в ядре 
         * events - куда вернуть результат
         * MAX_CON - количество обрабатываемых запросов
         * -1 - заблокироваться навечно(до получения запроса)
         */

        msgctl(msquid, IPC_STAT, &bf);
        printf("connections: %d --- quesize: %d\n",connections, bf.msg_qnum);
        epoll_wait(epfd, events, MAX_CON, -1);
        for (index = 0; index < MAX_CON; index++) {
            client_fd = events[index].data.fd;
            /* если подключается новый клиент */
            /* TODO: проверить на UDP сокет*/
#if 0
            if(connections + 2 == MAX_CON){
                /*
                printf("Не справляюсь, создаю новый поток\n");
                */
                state = NEWTHREAD;
                break;
            }
#endif
            if (client_fd == listener) {
                printf("LISTENER\n");
                addrlen = sizeof(clientaddr);
                if ((newfd = accept(listener, 
                                    (struct sockaddr *)&clientaddr,
                                    &addrlen)) == -1) {
                    perror("server: accept() error ");
                } else {
                    printf("server: подключается %s...\n", inet_ntoa(clientaddr.sin_addr));
                    /* https://stackoverflow.com/a/22339017 */
                    if (fcntl(newfd, F_SETFL,
                               fcntl(newfd, F_GETFL, 0) | O_NONBLOCK) == -1){
                        perror("calling fcntl");
                    }
                    ev.events = EPOLLIN;
                    ev.data.fd = newfd;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, newfd, &ev) < 0) {
                        perror("epoll_ctl");
                        exit(1);
                    }
                    connections++;
                    /* добавляем новый дескриптор в очередь сообщений.
                     * Так как уникальность каждого дескриптора гарантирована, 
                     * мы можем использовать его так же в качестве значения
                     * для поля type.
                     */
                    toqueue(msquid, newfd, newfd);
                    /* TODO: прчоекай, не нужно ли создать новый поток */
                }
                break;
            } else {
                /* Note that when reading from a channel such as a pipe or a
                 * stream socket, event EPOLLHUP merely indicates that the peer
                 * closed its end of the channel.
                 */
                if (events[index].events & EPOLLHUP) {
                    printf("EPOLLHUP\n");
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, &ev) < 0) {
                            perror("epoll_ctl");
                    }
                    /* TODO: проверяй, выкинулось ли правильно */
                    todequeue(msquid, client_fd);
                    connections--;
                    close(client_fd);
                    break;
                }
                /* есть входящий запрос */
                if (events[index].events & EPOLLIN)  {
                    /* если ничего не пришло */
                    /* TODO: перепиши */
                    printf("EPOLLIN\n");
                    if((nbytes = gettcpmsg(client_fd, buf)) <= 0){
                        printf("EPOLLIN < 0\n");
                        if(nbytes == 0) {
                            printf("socket %d hung up, closing\n", client_fd);
                        }
                        else {
                            printf("recv() error ! %d", client_fd);
                            perror("");
                        }
                        /* выкинем его из списка */
                        if (epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, &ev) < 0) {
                            perror("epoll_ctl");
                        }
                        todequeue(msquid, client_fd);
                        connections--;
                        close(client_fd);
                        break;
                    } else {
                        printf("WANT TO REPLY\n");
                        /* Отвечаем сразу, не хочу пилить стопицот буферов */
                        strret(buf);
                        sendtcpmsg(client_fd, buf); 
                        todequeue(msquid, client_fd);
                        if (epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, &ev) < 0) {
                            perror("epoll_ctl");
                        }
                        connections--;
                        close(client_fd);
                        break;
                    }
                } 
            }
        }
    }
}

/* выбираем что послать */
void strret(char *str)
{

    time_t t;
    struct tm *tm;

    if(!strcmp(str, "ping")){
        strcpy(str, "pong");
    } else if(!strcmp(str, "time")){
        t = time(NULL);
        tm = localtime(&t);
        strftime(str, 64, "%c", tm);
    } else {
        strcpy(str, "агагагага");
    }
}
/* вытаскиваем сообщение из очереди */
int todequeue(int msquid, int id)
{
    struct quebuf tmp;
    int ret;
    /* TODO: чекнуть, чтобы оно на самом деле выкинулось */
    ret = msgrcv(msquid, &tmp, sizeof(tmp.sockfd), id, IPC_NOWAIT);

    return ret;
}
/* записываем в очередь новое сообзение */
void toqueue(int msqid, int sockfd, int id)
{
    struct quebuf tmp;
    int size;

    tmp.type = id;
    tmp.sockfd = sockfd;

    /* TODO: чекнуть, что добавилось в очередь */
    msgsnd(msqid, (void *) &tmp, sizeof(tmp.sockfd), IPC_NOWAIT);

}

/* принимаем сообщение от TCP-клиента */
int gettcpmsg(int sockfd, char *str)
{
    int len;
    int got;
    char *fp;
    fp = str;
    len = got = 0;
    while(1) {
        len = recv(sockfd, fp, BUFSIZE,0);
        if (len == -1 && errno == EAGAIN)
            break;
        if(len == 0)
            break;
        if(len >=0)
            got += len;
        fp+=len -1;
    }
    return got;

}
/* посылаем сообщение TCP-клиенту */
void sendtcpmsg(int sockfd, char *str)
{
    int len;
    char *fp;
    fp = str;
    while(1) {
        len = send(sockfd, fp, strlen(fp),0);
        if (len == -1 && errno == EAGAIN)
            break;
        if(len == 0)
            break;
        fp+=len;
    }
}
/* получаем сообщение от UDP сокета */
void getudpmsg(int sockfd, char *buf,
               struct sockaddr *their_addr,
               socklen_t *addr_len)
{

    char *fp;
    int numbytes;
    *addr_len = sizeof *their_addr;
    fp = buf;
    while(1) {
        numbytes = recvfrom(sockfd, fp, BUFSIZE , 0,
                          their_addr, addr_len);
        if (numbytes == -1 && errno == EAGAIN)
            break;
        if(numbytes == 0)
            break;
        fp+=numbytes;
    }

}

/* посылаем сообщение UDP сокету */
void sendudpmsg(int sockfd, char *fp, struct  sockaddr *their_addr, socklen_t *addrlen)
{
    int len;
    char *p;
    p = fp;
    while(1) {
        len = sendto(sockfd, p, strlen(p), 0, their_addr, *addrlen);
        if (len == -1 && errno == EAGAIN)
            break;
        if(len == 0)
            break;
        p+=len;
    }
}

/* создаем новую очередь сообщений */
int create_msqid(char *progname, int pid)
{
    int   msqid;
    key_t key;
    if((key = ftok(progname, pid)) == -1){
        fprintf(stderr, "server: Key generate error\n");
        exit(2);
    }

    if((msqid = msgget(key, (IPC_CREAT | 0644))) == -1 ){
        fprintf(stderr, "server: can't get msqid\n");
        exit(2);
    }
    return msqid;
}
