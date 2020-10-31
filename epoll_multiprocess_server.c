#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sysinfo.h>
#include <sys/epoll.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "httpd.h"

#define DEFAULT_PORT 8080
#define MAX_EVENT_NUM 1024
#define INFTIM -1

void process(int);

void handle_subprocess_exit(int);

int main(int argc, char *argv[])  
{
    struct sockaddr_in server_addr;
    int listen_fd;
    int cpu_core_num;
    int on = 1;
    
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DEFAULT_PORT);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind error, message: ");
        exit(1);
    }

    if (listen(listen_fd, 5) == -1) {
        perror("listen error, message: ");
        exit(1);
    }

    printf("listening 8080\n");
     // SIGCHLD，在一个进程终止或者停止时，将SIGCHLD信号发送给其父进程，按系统默认将忽略此信号，
    // 设置某一信号的对应动作
    signal(SIGCHLD, handle_subprocess_exit);
    // 获取处理器核心数
    cpu_core_num = get_nprocs();
    printf("cpu core num: %d\n", cpu_core_num);
    for (int i = 0; i < cpu_core_num * 2; i++) {
        pid_t pid = fork();
        // 多个，子进程执行如下代码：
        if (pid == 0) {
            process(listen_fd);
            exit(0);
        }
    }

    while (1) {
        sleep(1);
    }

    return 0;
}

void process(int listen_fd) 
{
    int conn_fd;
    int ready_fd_num;
    struct sockaddr_in client_addr;
    int client_addr_size = sizeof(client_addr);
    char buf[128];
    // 
    struct epoll_event ev, events[MAX_EVENT_NUM];
    // epoll_create参数没有什么重大的意义
    int epoll_fd = epoll_create(MAX_EVENT_NUM);
    
    ev.data.fd = listen_fd;
    ev.events = EPOLLIN;

    // 对epoll_fd进中添加listen_fd, 关注的事件是ev => EPOLLIN
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl error, message: ");
        exit(1);
    }

    while(1) {
        // redis
        // nginx
        // java nio 都是基于epoll
        // 最重要的函数
        // 有返回值的，一共有多少个fd触发了时间;
        // 只要对前ready_fd_num进行遍历就可以了;
        // epoll_wait会对触发的时间进行重新排列;
        ready_fd_num = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, INFTIM);
        printf("[pid %d] 😱 震惊！我又被唤醒了...\n", getpid());
        if (ready_fd_num == -1) {
            perror("epoll_wait error, message: ");
            continue;
        }
        //epoll_wait会对进行排序，将就绪的排到前面;
        for(int i = 0; i < ready_fd_num; i++) {
            // 这个描述符是监听的端口;
            if (events[i].data.fd == listen_fd) {
                conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_size);
                if (conn_fd == -1) {
                    sprintf(buf, "[pid %d] ❌ accept 出错了: ", getpid());
                    perror(buf);
                    continue;
                }

                if (fcntl(conn_fd, F_SETFL, fcntl(conn_fd, F_GETFD, 0) | O_NONBLOCK) == -1) {
                    continue;
                }
                // 监听连接队列中，获取文件描述符，注册到epoll_fd
                ev.data.fd = conn_fd;
                ev.events = EPOLLIN;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) == -1) {
                    perror("epoll_ctl error, message: ");
                    close(conn_fd);
                }
                printf("[pid %d] 📡 收到来自 %s:%d 的请求\n", getpid(), inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
            } 
            // 当前文件描述符已经就绪：且不是监听端口，则是连接的socket;
            else if (events[i].events & EPOLLIN) {
                printf("[pid %d] ✅ 处理来自 %s:%d 的请求\n", getpid(), inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
                conn_fd = events[i].data.fd;
                // 获取就绪的文件描述符；
                // 处理连接请求;
                accept_request(conn_fd, &client_addr);
                close(conn_fd);
            } 
            // 错误码，则关闭当前文件描述符;
            else if (events[i].events & EPOLLERR) {
                fprintf(stderr, "epoll error\n");
                close(conn_fd);
            }
        }
    }
}

void handle_subprocess_exit(int signo)
{
    printf("clean subprocess.\n");
    int status;
    // 清楚子进程，就是子进程很早就结束了，进程表项中还没有清楚的问题;
    while(waitpid(-1, &status, WNOHANG) > 0);
}