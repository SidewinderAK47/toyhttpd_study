#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include "httpd.h"

#define BUFFER_SIZE 2048
#define DEFAULT_PORT 8080

#define INFTIM -1
#define OPEN_MAX 1024

int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr, client_addr;

    int client_addr_size;
    int listen_fd, conn_fd;
    // 定义客户端的fds
    // pollfd结构体
    struct pollfd client_fds[OPEN_MAX];
    int index, max_index;
    int ready_fd_num;
    
    int on = 1;

    client_addr_size = sizeof(client_addr);
    //创建scoket，面向可靠连接，ipv4;
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    // 设置复数用socket
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    // 设置 协议版本，ip + 端口号;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DEFAULT_PORT);
    
    // 绑定 (地址+端口号 到 socket文件描述符)
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind error, message: ");
        exit(1);
    }
    // 开启监听模式
    if (listen(listen_fd, 5) == -1) {
        perror("listen error, message: ");
        exit(1);
    }

    printf("listening 8080\n");
    // 初始化
    for (int i = 0; i < OPEN_MAX; i++) {
        client_fds[i].fd = -1;
    }
    max_index = 0;
    // fd文件描述符
    client_fds[0].fd = listen_fd;
    // 事件 如果两个事件都在意的话，或一下.
    client_fds[0].events = POLLRDNORM; //普通数据可读

    while (1) {
        // 最主要区别在这里：
        //poll 的参数：client_fds数组
        // 与select相似，都是阻塞函数;
        // 有一个或者多个文件描述符有数据的时候，会进行置位；与select不可复用，使用r_set被置位，导致不可复用。
        // 对poll_fd的revent置位
        ready_fd_num = poll(client_fds, max_index + 1, INFTIM);
        if (ready_fd_num < 0) {
            perror("poll error, message: ");
            continue;
        }
        //POLLRDNORM 数据可读
        if (client_fds[0].revents & POLLRDNORM) {
            conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_size);
            if (conn_fd == -1) {
                perror("accept error, message: ");
                continue;
            }
            // 找到一个可以防止poll_fd位置，并保存。
            for (index = 1; index < OPEN_MAX; index++) {
                if (client_fds[index].fd < 0) {
                    client_fds[index].fd = conn_fd;
                    client_fds[index].events = POLLRDNORM;
                    break;
                }
            }
            // 数组中没有可以防止poll_fd位置了：连接太多了，close当前连接
            if (index == OPEN_MAX) {
                fprintf(stderr, "too many connections\n");
                close(conn_fd);
                continue;
            }
            // max_index用于截断当前数组；poll函数中。
            if (index > max_index) {
                max_index = index;
            }

            if (--ready_fd_num <= 0) {
                continue;
            }
        }
        // 然后再对所有的客户端文件描述符进行遍历,判断是否在;
        // 并且标志位已经被重置了。
        for (int i = 1; i <= max_index; i++) {
            if ((conn_fd = client_fds[i].fd) < 0) {
                continue;
            }
            // 是我想要的事件
            if (client_fds[i].revents & POLLRDNORM) {
                // 接收连接;
                accept_request(client_fds[i].fd, &client_addr);
                // 关闭连接，并且在数组中置-1;
                close(client_fds[i].fd);
                client_fds[i].fd = -1;
            } else {
                // 这边为什么要释放到当前客户端文件描述符没有理解;
                close(client_fds[i].fd);
                client_fds[i].fd = -1;
            }

            if (--ready_fd_num <= 0) {
                break;
            }
        }
    }

    return 0;
}