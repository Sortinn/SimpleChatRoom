
/*
 *  client.c
 *  function
 *  show ：查询用户在线
 *	to ：私法
 *	all：群发
 *	quit：退出
 *  
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <netinet/in.h>
//#include <error.h>
#include <errno.h>
#include <arpa/inet.h>
#include <termios.h>

#define MAX_SIZE 1024
#define PORT 7999

static int FLAGE = -1;
char name[20] = {0};
//查询用户在线，向服务器发送消息，获取查询信息
void fun_show(int sockfd) {
    char sendbuf[256] = {0};
    sprintf(sendbuf, "SHOW\r\n\r\n");
    if (send(sockfd, sendbuf, strlen(sendbuf), 0) <= 0) {
        printf("send err\n");
        close(sockfd);
        exit(1);
    }
}
//群聊模式
void fun_all(int sockfd) {
    char sendbuf[MAX_SIZE] = {0};
    sprintf(sendbuf, "ALL\r\n", 5);
    printf("输入发送的内容:\n");
    scanf("%s", sendbuf + 5);
    sprintf(sendbuf + strlen(sendbuf), "\r\n\r\n");
    if (send(sockfd, sendbuf, strlen(sendbuf), 0) <= 0) {
        printf("send err\n");
        close(sockfd);
        exit(1);
    }
}
//私聊模式，向服务器发送私聊对象，服务器转发
void fun_one(int sockfd) {
    char sendbuf[MAX_SIZE] = {0};
    char name3[20] = {0};
    printf("输入聊天对象:");
    scanf("%s", name3);
    sprintf(sendbuf, "TO\r\n%s\r\n", name3);
    printf("输入聊天内容:\n");
    scanf("%s", sendbuf + strlen(sendbuf));
    sprintf(sendbuf + strlen(sendbuf), "\r\n\r\n");
    if (send(sockfd, sendbuf, strlen(sendbuf), 0) <= 0) {
        printf("send err\n");
        close(sockfd);
        exit(1);
    }
}
//用户退出，向服务器发送退出请求
void fun_quit(int sockfd) {
    char sendbuf[256] = "QUIT\r\n\r\n";
    if (send(sockfd, sendbuf, strlen(sendbuf), 0) <= 0) {
        printf("send err\n");
        close(sockfd);
        exit(1);
    }
}

void *pthread_fun(int *sock) {
    int sockfd = *sock;
    char recvbuf[1024] = {0};
    int n = 0;
    char *p, *q;
    char name2[20] = {0};
    while (1) {
        memset(recvbuf, 0, 1024);
        n = recv(sockfd, recvbuf, sizeof(recvbuf), 0);
        if (n <= 0) {
            printf("recv failed\n");
            exit(1);
        }
        if (!strncmp(recvbuf, "NOTICE\r\n通讯通道开启\r\n\r\n", 30)) {//判断通讯通道开启
            printf("通讯通道开启\n");
            FLAGE = 1;
        }
        if (!strncmp(recvbuf, "100\r\n", 5)) {
            char *p = strtok(recvbuf + 5, "\r\n\r\n");//100\r\n：3个字符后取出\r\n\r\n前的名字
            strcpy(name2, p);
            printf("[NOTICE]%s进入聊天室\n", name2);
            FLAGE = 4;
        }
        if (!strncmp(recvbuf, "ERROR\r\n用户名重复\r\n\r\n", 26)) {//判断是否重名
            printf("用户名重复\n");
            FLAGE = 3;
        }
        if (!strncmp(recvbuf, "200\r\n", 5)) {
            p = strtok(recvbuf + 5, "\r\n\r\n");//取出\r\n\r\n前的消息
            while (p != NULL) {
                printf("%s\n", p);
                p = strtok(NULL, "\r\n\r\n");
            }
            printf("please input con:\n");
        }
        if (!strncmp(recvbuf, "AFROM\r\n", 7)) {
//			printf("recvbuf=%s\n",recvbuf);
            p = strtok(recvbuf + 6, "\r\n");//取出群聊信息
            q = strtok(NULL, "\r\n\r\n");
            printf("(%s)[群聊]:%s\n", p, q);
            printf("please input con:\n");
        }
        if (!strncmp(recvbuf, "FROM\r\n", 6)) {
            p = strtok(recvbuf + 6, "\r\n");//取出私聊名
            q = strtok(NULL, "\r\n\r\n");
            printf("(%s)[私聊](%s):%s\n", p, name, q);
            printf("please input con:\n");
        }
        if (!strncmp(recvbuf, "ERROR2\r\n", 8)) {//重名错误
            p = strtok(recvbuf + 8, "\r\n");
            printf("%s\n", p);
        }
        if (!strncmp(recvbuf, "NOTICE1\r\n", 9)) {
            p = strtok(recvbuf + 9, "\r\n");
            printf("用户%s\n", p);
        }
        if (!strncmp(recvbuf, "NOTICE\r\n服务器已满,请等候\r\n\r\n", 37)) {
            printf("服务器已满,请等候\n");
            FLAGE = 0;
        }
        if (!strncmp(recvbuf, "NOTICE\r\n您已被唤醒,请继续操作\r\n\r\n", 45)) {
            printf("你已经被唤醒,请继续操作\n");
            FLAGE = 2;
        }
    }
}

void main(int argc, char **argv) {
    if (argc != 2) {
        printf("input server ip:\n");
        exit(1);
    }
    pthread_t pid;//声明线程id
    int sockfd;
    struct sockaddr_in addr;
    char recvbuf[1024] = {0};
    char sendbuf[1024] = {0};
    int k = 0;
    int n;
    char str[6] = {0};
    char *p, *q;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {//创建套接字
        perror("socket");
        exit(1);
    }
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], (void *) &addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(1);
    }
//	inet_aton(argv[1], &addr.sin_addr);
    if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("connect");
        exit(1);
    }
    printf("connect success\n");
//	pid = fork();
    pthread_create(&pid, NULL, (void *) pthread_fun, (void *) &sockfd);//创建线程
    pthread_detach(pid);
//	if(pid > 0){
    usleep(100);//让子进程先运行
    while (FLAGE == 0) {
        printf("服务器已满,wait...\n");
        sleep(1);
    }
    tcflush(0, TCIFLUSH);
    while (FLAGE == 2) { //当FLAGE＝2时正好父进程运行，则等待,FLAGE=1
        usleep(100);
    }
    if (FLAGE == 1) {
        while (k < 3) {
            //fflush(stdin);
            memset(name, 0, 20);
            printf("login:");
            scanf("%s", name);
            //fflush(stdin);
            sprintf(sendbuf, "LOGIN\r\n%s\r\n\r\n", name);
            send(sockfd, sendbuf, strlen(sendbuf), 0);
            memset(sendbuf, 0, 1024);
            for (;;) {
                if (FLAGE == 3 || FLAGE == 4)
                    break;
            }
            if (FLAGE == 3) {
                k++;
                if (k == 3) {
                    printf("输入过多,退出\n");
                } else {
                    printf("还有%d次机会登录\n", 3 - k);
                }
                FLAGE = 1;
                continue;
            }
            if (FLAGE == 4) {
                while (1) {
                    memset(str, 0, 6);
                    scanf("%s", str);
                    if (!strcmp(str, "show")) {
                        printf("显示在线用户:\n");
                        fun_show(sockfd);
                        continue;
                    } else if (!strcmp(str, "all")) {
                        printf("群聊模式 ");
                        fun_all(sockfd);
                        continue;
                    } else if (!strcmp(str, "to")) {
                        printf("私聊模式");
                        fun_one(sockfd);
                        continue;
                    } else if (!strcmp(str, "quit")) {
                        printf("退出聊天室\n");
                        fun_quit(sockfd);
                        close(sockfd);
                        return;
                    } else if (!strcmp(str, "help")) {
                        printf("all[聊天内容]             群聊\n");
                        printf("to[name][聊天内容]        私聊\n");
                        printf("quit                    退出程序\n");
                        printf("help                    显示帮助信息\n");
                        printf("please input con:\n");
                        continue;
                    } else {
                        printf("请输入help查看指令:\n");
                        continue;
                    }
                }
            }
        }
    }
//	}
    wait(NULL);
}

