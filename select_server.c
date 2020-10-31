#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "httpd.h"

#define BUFFER_SIZE 2048
#define DEFAULT_PORT 8080

int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr, client_addr;
    int client_addr_size;
    int listen_fd, conn_fd, max_fd;
    // 文件描述符数组;1024
    int client_fds[FD_SETSIZE];
    int index, max_index;
    // bitmap类型
    fd_set read_set, all_set; // read_set会拷贝到内核; select交给内核来判断; 自己去判断的话，则是
    int ready_fd_num;
    int on = 1;
    
    client_addr_size = sizeof(client_addr);
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
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

    for (int i = 0; i < FD_SETSIZE; i++) {
        client_fds[i] = -1;
    }
    max_index = 0;
    //将指定的文件描述符集清空
    FD_ZERO(&read_set);
    FD_ZERO(&all_set);
    //用于在文件描述符集合中增加一个新的文件描述符。
    // 监听当前文件描述符，一直都存在的。
    FD_SET(listen_fd, &all_set);
    max_fd = listen_fd;

    while (1) {
        read_set = all_set;
        // 最大文件描述符+1， 读文件描述符集合，写文件描述符r_set，异常描述符集合，超时时间 
        // select是一个阻塞函数，如果没有一个有数数据来的话会一直阻塞在这一行;
        // 会对r_set进行置位。
        printf("-----stop---------1\n");
        ready_fd_num = select(max_fd + 1, &read_set , NULL, NULL, NULL);
        printf("-----stop---------2\n");
        if (ready_fd_num < 0) {
            perror("select error, message: ");
            continue;
        }
        // 先判断 是否有连接;
        if (FD_ISSET(listen_fd, &read_set)) {
            conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_size);
            if (conn_fd == -1) {
                perror("accept error, message: ");
                continue;
            }
            // 在client_fds中，找到一个位置存放当前连接文件描述符
            for (index = 1; index < FD_SETSIZE; index++) {
                if (client_fds[index] < 0) {
                    client_fds[index] = conn_fd;
                    break;
                }
            }
            // 如果没有找到一个位置可以存放当前 的文件描述符，则说明 连接太多了，数组放不下了
            if (index == FD_SETSIZE) {
                fprintf(stderr, "too many connections\n");
                close(conn_fd);
                continue;
            }
            // 记录最大的文件描述符存放位置index;
            if (index > max_index) {
                max_index = index;
            }
            // 客户端连接的文件描述符，放置到all_set
            FD_SET(conn_fd, &all_set);
            if (conn_fd > max_fd) {
                max_fd = conn_fd;
            }

            if (--ready_fd_num <= 0) {
                continue;
            }
        }
        
        //  
        for (int i = 1; i <= max_index; i++) {
            conn_fd = client_fds[i];
            if (conn_fd == -1) {
                continue;
            }

            if (FD_ISSET(conn_fd, &read_set)) {
                accept_request(conn_fd, &client_addr);
                // all_set中清除 文件描述符
                FD_CLR(conn_fd, &all_set);
                close(conn_fd);
                client_fds[i] = -1;
            }
            // 处理个数达到了，则直接返回。
            if (--ready_fd_num <= 0) {
                break;
            }
        }
    }

    return 0;
}