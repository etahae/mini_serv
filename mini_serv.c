#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

typedef struct s_client {

    int fd;
    int id;
    struct s_client *next;

} t_client;

t_client *clients = NULL;

int sockfd, g_id = 0;

fd_set cpy_wr, cpy_rd, curr_sock;

char msg[42], str[42 * 4096], buf[42 * 4096 + 42], tmp[42 * 4096];

int fatal() {
    write(2, "Fatal error\n", 13);
    close(sockfd);
    exit(1);
}

int max_fd() {
    t_client *temp = clients;
    int max = sockfd;

    while (temp) {
        if (max < temp->fd)
            max = temp->fd;
        temp = temp->next;
    }

    return max;
}

int addCLientToList(int senderFD) {

    t_client *temp = clients;
    t_client *newCLient;

    if (!(newCLient = calloc(1, sizeof(t_client))))
        fatal();
    
    newCLient->fd = senderFD;
    newCLient->id = g_id++;
    newCLient->next = NULL;

    if (!clients)
        clients = newCLient;
    else {
        while (temp->next)
            temp = temp->next;
        temp->next = newCLient;
    }
    return newCLient->id;
}

void broadcast(int senderFd, char *msg_) {
    t_client *temp = clients;

    while (temp) {
        printf("%d\n", senderFd != temp->fd);
        if (senderFd != temp->fd && FD_ISSET(temp->fd, &cpy_wr)) {
            write(1, "lmao", 5);
            if (send(temp->fd, msg_, strlen(msg_), 0) < 0)
                fatal();
        }
        temp = temp->next;
    }
}

void addCLientToServer() {

    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    int newFD;

    if ((newFD = accept(sockfd, (struct sockaddr *) &clientaddr, &len)) < 0)
        fatal();
    sprintf(msg, "server: client %d just arrived\n", addCLientToList(newFD));
    broadcast(newFD, msg);
    FD_SET(newFD, &curr_sock);
}

int get_id (int fd) {
    t_client *temp = clients;

    while (temp){
        if (temp->fd = fd)
            return temp->id;
        temp = temp->next;
    }
    return -1;
}

int rm_client(int fd) {

    t_client *temp = clients;
    t_client *del;
    int id = get_id(fd);

    if (temp && temp->fd == fd) {
        clients = temp->next;
        free(temp);
    }
    else {
        while (temp && temp->next && temp->next->fd != fd)
            temp = temp->next;
        del = temp->next;
        temp->next = temp->next->next;
        free(del);
    }
    return id;
}

void send_msg(int fd) {
    int i = 0;
    int j = 0;

    while (str[i]) {
        tmp[j] = str[i];
        j++;
        if (str[i] == '\n') {
            sprintf(buf, "client %d: %s", get_id(fd), tmp);
            broadcast(fd, buf);
            bzero(&buf, sizeof(buf));
            bzero(&tmp, sizeof(tmp));
            j = 0;
        }
        i++;
    }
    bzero(&str, sizeof(str));
}

int main(int c, char **v) {

    if (c < 2) {
        write(2, "Wrong number of arguments\n", 27);
        exit(1);
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(v[1]));

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        fatal();
    if (bind(sockfd, (const struct sockaddr *)& servaddr, sizeof(servaddr)) < 0)
        fatal();
    if (listen(sockfd, 0) < 0)
        fatal();

    FD_ZERO(&curr_sock);
    FD_SET(sockfd, &curr_sock);

    bzero(&str, sizeof(str));
    bzero(&buf, sizeof(buf));
    bzero(&tmp, sizeof(tmp));

    while (1) {
        cpy_wr = cpy_rd = curr_sock;
        if (select(max_fd() + 1, &cpy_rd, &cpy_wr, NULL, NULL) < 0)
            continue;
        for (int fd = 0; fd <= max_fd(); fd++) {
            if (FD_ISSET(fd, &cpy_rd)) {
                if (fd == sockfd) {
                    bzero(&msg, sizeof(msg));
                    addCLientToServer();
                    break;
                }
                else {
                    int ret_recv = 1000;
                    while (ret_recv == 1000 || str[strlen(str) - 1] != '\n') {
                        ret_recv = recv(fd, str + strlen(str), 1000, 0);
                        if (ret_recv <= 0)
                            break;
                    }
                    if (ret_recv <= 0) {
                        bzero(&msg, sizeof(msg));
                        sprintf(msg, "server: client %d just left\n", rm_client(fd));
                        broadcast(fd, msg);
                        FD_CLR(fd, &curr_sock);
                        close(fd);
                        break;
                    }
                    else
                        send_msg(fd);
                }
            }
        }   
    }
}