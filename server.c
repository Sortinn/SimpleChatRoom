/*
 *  server.c
 *  client.c
 *  function
 *  show ：查询用户在线
 *	to ：私法
 *	all：群发
 *	quit：退出
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
//#include <error.h>
#include <errno.h>
#include<netinet/in.h>

#define PORT 7999
#define MAX_NUM 3  //client连接最大个数
#define MAX_CLIENT 15
#define MAX_SIZE 1024
#define MSG_NOSIGNAL 0

pthread_rwlock_t idx_lock, wait_lock;  //定义读写锁

//client 信息
typedef struct _client {
    int sockfd;
    char name[20];
    pthread_t pid;
    int flg;
} c_client;
c_client client[MAX_CLIENT];//定义client;
//等待的client
struct _client_ {
    int sockfd;
    char name[20];
    pthread_t pid;
    struct _client_ *next;
};
typedef struct _client_ c_client_c;
c_client_c *head = NULL;
//链式存储在线用户
c_client_c *temp_c1 = NULL, *temp_c2 = NULL;//等待的

//初始化client信息
void init_client() {
    int i = 0;
    for (i = 0; i < MAX_CLIENT; i++) {
        client[i].sockfd = -1;
        memset(client[i].name, 0, 20);
        client[i].pid = -1;
        client[i].flg = -1;
    }
}

//查找结构体数组中sockfd为-1的下标值
int find_fd(c_client *client) {
    int i = 0;
    while (i < MAX_NUM) {
//		printf("====%d\n",client[i].sockfd);
        if (client[i].sockfd == -1)
            return i;
        i++;
    }
    return -1;
}

//判断登录格式
int logform(char *buf) {
    char *p = strstr(buf, "LOGIN\r\n");
    int n = strlen(buf);
    char *q = p + n - 4;
    if (p != NULL && p + 7 != q && strcmp(q, "\r\n\r\n") == 0)
        return 1;
    else
        return 0;
}

int cmpname(char *buf, c_client *p_client) {
    int i = 0;
    char *p = strtok(buf + 7, "\r\n\r\n");
    while (client[i].sockfd != -1 && client[i].sockfd != p_client->sockfd && i
                                                                             < MAX_NUM) {
        if (strcmp(client[i].name, p) == 0)
            return 0;
        i++;
    }
    return 1;
}

//显示用户的信息
void showuser(c_client *p_client) {
    int i = 0;
    char buf[1024] = {0};
    strcpy(buf, "200\r\n");
    for (i = 0; i < MAX_NUM; i++) {
        if (client[i].sockfd != -1) {
            sprintf(buf + strlen(buf), "%s\r\n", client[i].name);
        }
    }
    sprintf(buf + strlen(buf), "\r\n");
    send(p_client->sockfd, buf, strlen(buf), 0);
}

//群聊，给所有用户发送消息
void sendto_all(c_client *p_client, char *buf) {
    int i = 0;
    char sendbuf[1024] = {0};
    sprintf(sendbuf, "AFROM\r\n%s\r\n%s", p_client->name, buf + 5);
    for (i = 0; i < MAX_NUM; i++) {
        if (client[i].sockfd != -1 && client[i].flg != -1)//判断用户是否在线
            if (send(client[i].sockfd, sendbuf, strlen(sendbuf), 0) <= 0) {
                printf("send error\n");
                exit(1);
            }
    }

}
//查找该用户的套接字句柄
int findname(char *name) {
    int i = 0;
    for (i = 0; i < MAX_NUM; i++) {
        if (client[i].sockfd != -1 && strcmp(client[i].name, name) == 0)
            return client[i].sockfd;
    }
    return 0;
}

//私聊
void sendto_one(c_client *p_client, char *buf) {
    int i = 0;
    char sendbuf[1024] = {0};
    char name[20] = {0};
    char *p = strtok(buf + 4, "\r\n");//TO\r\n：4个字符后取出\r\n前的名字
    strcpy(name, p);
    int sock = findname(name);//
    if (!sock) {
        sprintf(sendbuf, "ERROR2\r\n%s用户不存在\r\n\r\n", name);//用户不在线
        send(p_client->sockfd, sendbuf, strlen(sendbuf), 0);
    } else {
        sprintf(sendbuf, "FROM\r\n%s\r\n%s", p_client->name, buf + 4 + strlen(
                name) + 2);//发送给客户端已连接的用户
        if (send(sock, sendbuf, strlen(sendbuf), 0) <= 0) {//发送失败
            printf("send error\n");
            exit(1);
        }
    }
}

void pthread_fun(void *cclient);

//退出聊天程序
void quit(c_client *p_client) {
    int i = 0;
    int idx;
    char buf[1024] = {0};
    c_client_c *temp;
    printf("--%s退出聊天室\n", p_client->name);//输出退出的信息
    close(p_client->sockfd);//设置已退出用户的信息
    p_client->sockfd = -1;
    p_client->pid = -1;
    p_client->flg = -1;
    sprintf(buf, "NOTICE1\r\n%s退出聊天室\r\n\r\n", p_client->name);
    memset(p_client->name, 0, 20);
    for (i = 0; i < MAX_NUM; i++) {//通知在线用户
        if (client[i].sockfd != -1 && client[i].flg != -1)
            send(client[i].sockfd, buf, strlen(buf), 0);
    }
    if (head != NULL && head->next != NULL) {//判断排队队列是否为空，若不为空唤醒下一个
        memset(buf, 0, 1024);
        pthread_rwlock_rdlock(&idx_lock);//获取非阻塞式读锁
        idx = find_fd(client);
        pthread_rwlock_unlock(&idx_lock);//释放读锁
        client[idx].sockfd = head->next->sockfd;
        pthread_rwlock_wrlock(&wait_lock);//获取非阻塞式写锁
        temp = head->next;
        head->next = head->next->next;//删除链式存储中已经退出的用户信息
        free(temp);
        pthread_rwlock_unlock(&wait_lock);//释放写锁
        sprintf(buf, "NOTICE\r\n您已被唤醒,请继续操作\r\n\r\n");
        send(client[idx].sockfd, buf, strlen(buf), 0);
        if (pthread_create(&client[idx].pid, NULL, (void *) pthread_fun, (void *) &client[idx]) != 0) {//创建新的线程
            perror("pthread_create");
            exit(1);
        }
        pthread_detach(client[idx].pid);//线程运行结束后会自动释放资源
    }
}
//线程
void pthread_fun(void *cclient) {
    c_client *p_client = (c_client *) cclient;
    char buf[MAX_SIZE] = {0};
    char sendbuf[1024] = {0};
    int i, n;
    char *p;
    sprintf(sendbuf, "%s", "NOTICE\r\n通讯通道开启\r\n\r\n");
    if (send(p_client->sockfd, sendbuf, strlen(sendbuf), 0) <= 0) {
        printf("send err\n");
    }
    memset(sendbuf, 0, 1024);
    while (1) {
        memset(buf, 0, MAX_SIZE);
        n = recv(p_client->sockfd, buf, sizeof(buf) - 1, MSG_NOSIGNAL);
        if (n <= 0) {
            close(p_client->sockfd);
            p_client->sockfd = -1;
            break;
        }
        if (logform(buf)) {//判断登录格式
            if (cmpname(buf, p_client) == 0) {
                send(p_client->sockfd, "ERROR\r\n用户名重复\r\n\r\n", 26, 0);
                continue;
            } else {
                p_client->flg = 1;//标志在线
                p = strtok(buf + 7, "\r\n\r\n");//获取字符串的用户名
                strcpy(p_client->name, p);
                sprintf(sendbuf, "100\r\n%s\r\n\r\n", p_client->name);
                send(p_client->sockfd, sendbuf, sizeof(sendbuf), 0);
                printf("%s进入聊天室\n", p_client->name);
                for (i = 0; i < MAX_NUM; i++) {//群发给所有在线用户，告诉有用户进入
                    if (client[i].sockfd != -1 && client[i].sockfd
                                                  != p_client->sockfd && client[i].flg != -1)
                        send(client[i].sockfd, sendbuf, sizeof(sendbuf), 0);
                }
                memset(sendbuf, 0, 1024);//清零
                while (1) {
					//接收消息
                    memset(buf, 0, MAX_SIZE);
                    if ((n = recv(p_client->sockfd, buf, MAX_SIZE, 0)) <= 0) {
                        perror("recv err");
                        break;
                    }
//					printf("recv=%s\n",buf);
                    if ((p = strstr(buf, "\r\n\r\n")) != NULL && *(p + 4)//p指向结束符在字符串中的位置，p+4指向发送的消息。所发判断消息是否为字符串末尾
                                                                 == '\0') {
                        if (!strncmp(buf, "SHOW\r\n\r\n", 8)) {
                            showuser(p_client);//客户端执行show后，发送给客户端已连接的用户
                            continue;
                        }
                        if (!strncmp(buf, "ALL\r\n", 5)) {//群发
                            sendto_all(p_client, buf);
                            continue;
                        }
                        if (!strncmp(buf, "TO\r\n", 4)) {//私法
                            sendto_one(p_client, buf);
                            continue;
                        }
                        if (!strncmp(buf, "QUIT\r\n\r\n", 8))//退出聊天程序
                            quit(p_client);
                        //						break;
                        pthread_exit(NULL);
                    } else {
                        send(p_client->sockfd, "ERROR\r\n信息不符合协议要求\r\n\r\n",
                             38, 0);
                    }
                }
            }
        } else {
            send(p_client->sockfd, "ERROR\r\n未登录,请您登录再进行其他操作\r\n\r\n", 56, 0);
        }
    }
    pthread_exit(NULL);
}

int main() {
    int ser_sockfd, clt_sockfd;
    struct sockaddr_in addr;
    int idx;
    char buf[1024] = {0};
    //	pthread_rwlock_t idx_lock,wait_lock;
    pthread_rwlock_init(&idx_lock, NULL);//初始化读写锁
    pthread_rwlock_init(&wait_lock, NULL);
    init_client();
    //创建服务器sockfd
    if ((ser_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    //设置服务器网络地址
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //设置端口可重用
    int opt = 1;
    setsockopt(ser_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //将套接字绑定到服务器的网络地址上
    if (bind(ser_sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(1);
    }
    printf("bind success\n");
    //监听连接请求--监听队列长度为10
    if (listen(ser_sockfd, 10) == -1) {
        perror("listen");
        exit(1);
    }
    printf("listen success\n");
    while (1) {
        if ((clt_sockfd = accept(ser_sockfd, NULL, NULL)) == -1) {
            perror("accept");
            exit(1);
        }
        pthread_rwlock_rdlock(&idx_lock);//获取非阻塞式读锁
        idx = find_fd(client);
//		printf("idx=%d\n",idx);
        pthread_rwlock_unlock(&idx_lock);//释放锁
        if (idx != -1) { //连接末满
            client[idx].sockfd = clt_sockfd;
            if (pthread_create(&client[idx].pid, NULL, (void *) pthread_fun,
                               (void *) &client[idx]) != 0) {
                perror("pthread_create");
                exit(1);
            }
            pthread_detach(client[idx].pid);
        } else { //连接已满,加入等待队列
            temp_c1 = (c_client_c *) malloc(sizeof(c_client_c));
            temp_c1->sockfd = clt_sockfd;
            temp_c1->next = NULL;
            pthread_rwlock_wrlock(&wait_lock);//获取非阻塞式写锁
            if (head == NULL) {
                head = (c_client_c *) malloc(sizeof(c_client_c));
                head->next = temp_c1;
            } else {
                for (temp_c2 = head; temp_c2->next != NULL; temp_c2 = temp_c2->next);//尾插法
                temp_c2->next = temp_c1;
            }
            pthread_rwlock_unlock(&wait_lock);//释放写锁
            memset(buf, 0, 1024);
            sprintf(buf, "NOTICE\r\n服务器已满,请等候\r\n\r\n");//客户端接受 则等待
            if (send(temp_c1->sockfd, buf, strlen(buf), 0) <= 0) {
                printf("sendr err\n");
            }
        }
    }
}
