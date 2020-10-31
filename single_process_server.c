#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "httpd.h"

/*
    1.
*/



#define BUFFER_SIZE 2048
#define DEFAULT_PORT 8080

int main(int argc, char *argv[])
{
    // 存储ip+端口
    struct sockaddr_in server_addr, client_addr;
    int client_addr_size;
    int listen_fd, conn_fd;
    int on = 1;
    
    client_addr_size = sizeof(client_addr);
    // 创建一个socket: 指定协议族为ipv4, 面向可靠连接，默认的
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    // 设置socket可重用
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    //bzero函数：将字符串s的前n个字节置为0，一般来说n通常取sizeof(s),将整块空间清零。
    bzero(&server_addr, sizeof(server_addr));
    // 指定地址+端口号;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DEFAULT_PORT);
    // 将创建的socket 与 (地址+端口号)进行绑定;
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind error, message: ");
        exit(1);
    }
    // 监听来自客户端的tcp socket的连接请求。
    //backlog监听队列长度，表示已经建立的TCP连接,backlog 连接请求队列长度
    if (listen(listen_fd, 5) == -1) {
        perror("listen error, message: ");
        exit(1);
    }

    printf("listening 8080\n");
    // 死循环启动：
    while (1) {
        // 在等待的连接队列中，提取一个连接client_addr
        conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (conn_fd == -1) {
            perror("accept error, message: ");
            continue;
        }

        accept_request(conn_fd, &client_addr);//返回一个socket的句柄
        // 关闭连接
        close(conn_fd);
    }

    return 0;
}