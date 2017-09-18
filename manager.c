#include "server.h"
#include <assert.h>

extern int keepRunning;
extern pthread_mutex_t mutex;
void *pmanage(void *args)
{
    struct tothread inargs;
    struct epoll_event events[MAX_CON];
    struct sockaddr_in clientaddr;

    struct epoll_event ev;
    struct sockaddr their_addr;
    struct msqid_ds bf; 
    char buf[1024];
    int tscpsock;
    int udpsock;
    int newfd;
    socklen_t addrlen;
    int msquid;
    int epfd;
    int events_cnt;
    int connections =  1;
    int index = 0;
    inargs = *((struct tothread *)args);
    tscpsock = inargs.tcpsock;
    udpsock = inargs.udpsock;
    epfd = inargs.epfd;

    /* создаем новую очередь сообщений.
     * В случае экстренного звершения программы мы должны закрыть все сокеты.
     */


    msquid = create_msqid(inargs.progname, pthread_self());
    msgctl(msquid, IPC_STAT, &bf);
    printf("msquid = %d, size = %d\n", msquid, (int)bf.msg_qnum);
/*
    while (bf.msg_qnum != 0) {
        msgctl(msquid, IPC_RMID, NULL);
        msquid = create_msqid(inargs.progname, pthread_self());
        msgctl(msquid, IPC_STAT, &bf);
    }
 */
        printf("EPOLL WAIT\n");
    while ((events_cnt = epoll_wait(epfd, events, MAX_CON, -1))){

        if(connections == 1 && keepRunning == 0){
            printf("ой вэй\n");
            return (void *) 0;
        }
        /* ждем пока не придет событие */
        /* epfd - указатель на структуру epoll в ядре 
         * events - куда вернуть результат
         * MAX_CON - количество обрабатываемых запросов
         * -1 - заблокироваться до получения запроса
         */
        msgctl(msquid, IPC_STAT, &bf);
        printf("i = %d, connections: %d --- ID %d -- quesize: %d\n",
                index,connections,(int)pthread_self(),(int)bf.msg_qnum);
        printf("EPOLL DONE %d\n", events_cnt);
        for (index = 0; index < events_cnt ; index++) {
            assert(events[index].data.fd);
            printf("ITERATION - %d\n", events[index].data.fd);
            if(keepRunning == 0) {

                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, events[index].data.fd, NULL) < 0) {
                            perror("epoll_ctl");
                    }
                    todequeue(msquid, events[index].data.fd);
                    connections--;
                    close(events[index].data.fd);
            }
            /* если подключается новый клиент */
            /* TODO: проверить на UDP сокет*/
            if (events[index].data.fd == tscpsock) {
                printf("LISTENER\n");
                while(1){
                    addrlen = sizeof(clientaddr);
                    if ((newfd = accept(tscpsock, (struct sockaddr *)&clientaddr,
                                        &addrlen)) < 0) {
                        if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                            break;
                        }
                        perror("server: accept() error ");
                    } else {
                        printf("server: подключается %s -- socket: %d...\n",
                                inet_ntoa(clientaddr.sin_addr), newfd);

                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.fd = newfd;
                        if (epoll_ctl(epfd, EPOLL_CTL_ADD, newfd, &ev) < 0) {
                            perror("epoll_ctl");
                            close(newfd);
                            continue;
                        }
                        connections++;
                        /* добавляем новый дескриптор в очередь сообщений.
                         * Так как уникальность каждого дескриптора гарантирована, 
                         * мы можем использовать его так же в качестве значения
                         * для поля type.
                         */
                        toqueue(msquid, newfd, newfd);
                        printf("++++===NEW QUEUED===++++\n");
                        /* TODO: прчоекай, не нужно ли создать новый поток */
                        }
                }
            } else if(events[index].data.fd == udpsock) {
                addrlen = sizeof(their_addr);
                if(getudpmsg(events[index].data.fd, buf, &their_addr, &addrlen) > 0){
                        printf("WANT TO REPLY\n");
                        strret(buf);
                        printf("REPLYING...\n");
                        sendudpmsg(events[index].data.fd, buf, &their_addr, &addrlen); 
                        printf("DONE REPLYING\n");
                    }


            
            }else {
                printf("ELSE\n");
                /* Note that when reading from a channel such as a pipe or a
                 * stream socket, event EPOLLHUP merely indicates that the peer
                 * closed its end of the channel.
                 */
                if ((events[index].events & EPOLLHUP) ||
                    (events[index].events & EPOLLRDHUP)) {
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, events[index].data.fd, NULL) < 0) {
                            perror("epoll_ctl");
                    }
                    todequeue(msquid, events[index].data.fd);
                    connections--;
                    close(events[index].data.fd);
                }
                /* есть входящий запрос */
                if (events[index].events & EPOLLIN)  { 
                    printf("EPOLLIN\n");
                    if(gettcpmsg(events[index].data.fd, buf) > 0){
                        printf("WANT TO REPLY\n");
                        strret(buf);
                        printf("REPLYING...\n");
                        sendtcpmsg(events[index].data.fd, buf); 
                        printf("DONE REPLYING\n");
                        if (epoll_ctl(epfd, EPOLL_CTL_DEL, events[index].data.fd, NULL) < 0) {
                            perror("epoll_ctl");
                        }
                        todequeue(msquid, events[index].data.fd);
                        connections--;
                        close(events[index].data.fd);
                        printf("DEQUEUED\n");
                    }

                 }

            }
        }
        msgctl(msquid, IPC_STAT, &bf);
        printf("i = %d, connections: %d --- ID %d -- quesize: %d\n",
                index,connections,(int)pthread_self(),(int)bf.msg_qnum);
        printf("EPOLL WAIT\n");
    }
    printf("выход\n");
    return (void *) 0;
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
    /* На самом деле я должен указывать размер массива char, но мало того, что
     * я использую просто переменную int, так еще и  sizeof(int) не работает */
    ret = msgrcv(msquid, &tmp, sizeof(tmp), id, IPC_NOWAIT);
    if (ret == -1) {
        perror("todequeue");
    }
    return ret;
}
/* записываем в очередь новое сообзение */
void toqueue(int msqid, int sockfd, int id)
{
    struct quebuf tmp;
    int retval;

    tmp.type = id;
    tmp.sockfd = sockfd;

    retval = msgsnd(msqid, (void *) &tmp, sizeof(tmp), 0);
    if (retval == -1) {
        perror("msgsnd");
    }
}

/* принимаем сообщение от TCP-клиента */
int gettcpmsg(int sockfd, char *str)
{
    int len;
    int got;
    char *fp;
    fp = str;
    len = got = 0;
    printf("GETTING\n");
    while(1) {
        len = recv(sockfd, fp, BUFSIZE,0);
        printf("GETTING...%d\n", len);
        if(len == 0){
            break;
        }
        if(len >=0){
            fp += len;
            got += len;
        }
        if (len == -1 || (errno == EAGAIN || errno == EWOULDBLOCK)){
            break;
        }
    }
    printf("GOT %d\n", got);
    return got;

}

/* посылаем сообщение TCP-клиенту */
void sendtcpmsg(int sockfd, char *str)
{
    int len;
    char *fp;
    fp = str;
    printf("SENDING\n");
    while(1) {
        printf("SENDING...\n");
        len = send(sockfd, fp, strlen(fp),0);
        if (len == -1 && errno == EAGAIN ){
            perror("send");
            break;
        }
        if(len == 0)
            break;
        fp+=len;
        printf("%s\n", fp);
    }
}
/* получаем сообщение от UDP сокета */
int getudpmsg(int sockfd, char *str,
               struct sockaddr *their_addr,
               socklen_t *addrlen)
{

    int len;
    int got;
    char *fp;
    fp = str;
    len = got = 0;
    printf("GETTING\n");
    while(1) {
        len = recvfrom(sockfd, fp, BUFSIZE , 0, their_addr, addrlen);
        printf("GETTING...%d\n", len);
        if(len == 0){
            break;
        }
        if(len >=0){
            fp += len;
            got += len;
        }
        if (len == -1 || (errno == EAGAIN || errno == EWOULDBLOCK)){
            break;
        }
    }
    printf("GOT %d\n", got);
    return got;
}

/* посылаем сообщение UDP сокету */
void sendudpmsg(int sockfd, char *str, struct  sockaddr *their_addr, socklen_t *addrlen)
{

    int len;
    char *fp;
    fp = str;
    printf("SENDING UDP\n");
    while(1) {
        printf("SENDING... UDP\n");
        len = sendto(sockfd, fp, strlen(fp), 0, their_addr, *addrlen);
        printf("%d\n", len);
        if (len == -1 && errno == EAGAIN ){
            perror("send");
            break;
        }
        if(len == 0)
            break;
        fp+=len;
        printf("%s\n", fp);
    }
}

/* создаем новую очередь сообщений */
int create_msqid(char *progname, int pid)
{
    int   msqid;
    key_t key;
    /* The  ftok() function uses the identity of the file named by the 
     * given pathname (which must refer to an existing, accessible file) 
     * and the least significant ** 8 bits ** of proj_id
     * (which must be nonzero) to generate a key_t
     */

    /* Короче, phtread_self() выдает что-то типа -1312262400, что в двоичном
     * представлении - -0b1001110001101111000100100000000. И так у меня 
     * получалось со всеми потоками. То есть, второй аргумент ftok всегда
     * был 0 и мне возвращалась одна и та же очередь
     */
    
    pid = pid >> 8;
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
