/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "httpd.h"

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: coolblog-httpd/1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client, struct sockaddr_in *client_addr)
{
    char buf[1024];
    char time_buf[50];
    char *log_buf[5];
    size_t numchars;
    char method[255];   //方法（请求类型）
    char url[255];      //请求资源的url
    char path[512];     //请求的资源本地路径
    size_t i, j;
    struct stat st;
    int cgi = 0;      /* becomes true if server decides this is a CGI program */
                     /*为真时,表示服务器需要调用一个cgi程序*/
    char *query_string = NULL;

    log_buf[0] = inet_ntoa(client_addr->sin_addr);

    // 1.获取请求头的第一行
    // 从客户端请求数据中读取一行;(请求行)
    numchars = get_line(client, buf, sizeof(buf));

    log_buf[1] = buf;

    printf("%s - - [%s] %s", log_buf[0], get_time_str(time_buf), log_buf[1]);

    // 2. 从读取的数据中提取出请求类型
    i = 0; j = 0;
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j=i;
    method[i] = '\0';
    // 3. 判断请求类型
    // 判断请求方法是否所允许的方法 GET 和 POST
    // 当前只能处理post和get方法
    //strcasecmp用忽略大小写比较字符串,存在一个相等
    // 同时不和get和post相等
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        return;
    }

    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    
    // 4. 提取请求的资源URL
    // 消耗 http method 后面多余的空格
    while (ISspace(buf[j]) && (j < numchars))
        j++;
    
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    // 处理 http method 为 GET 的请求
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        // 定位 url 中 ? 的位置
        // 如果查询语句中含义'?',查询语句为'?'字符后面部分
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        // 将 ? 替换为 \0
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0'; // 从'?'处截断,前半截为url
            query_string++;
        }
    }
    // 
    // 根据 url 拼接资源路径，比如请求 /test.html，拼接后的路径为 htdocs/test.html
    sprintf(path, "htdocs%s", url);


    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");  // 	如果请求url是以'/'结尾,则指定为该目录下的index.html文件
    // 6.判断请求资源状态
    // 函数作用是将path指定路径文件状态保留在st中;
    if (stat(path, &st) == -1) {
        // 从client中读取，知道遇到两个换行
        // 读取并丢弃其他的请求头
        // 从client中读取,直到遇到两个换行(起始行startline和首部header之间间隔
        // 跳过
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        // 返回404响应
        not_found(client);
    }
    else
    {
        // 请求的是个目录,指定为该目录下的index.html文件
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        // // 请求的是一个可执行文件,作为CGI程序
        //S_IXUSR 文件所有者具可执行权限
        //S_IXGRP 用户组具可执行权限
        //S_IXOTH 其他用户具可执行权限
        if ((st.st_mode & S_IXUSR) ||
                (st.st_mode & S_IXGRP) ||
                (st.st_mode & S_IXOTH)    )
            cgi = 1;
        // 判断是执行一个CGI程序还是返回一个文件内容给客户端
        // 判断是执行一个CGI程序还是返回一个文件内容给客户端
        if (!cgi)
            // 静态资源;
            serve_file(client, path);
        else
            execute_cgi(client, path, method, query_string);
    }
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
/*
    客户端错误，报文语法有问题:400
*/
void bad_request(int client)
{
    char buf[1024];
    // 响应行
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    // 响应头
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    //空行
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    // 响应体
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
/*
    发送文件具体内容
    小细节：只有当文件位置指针(fp－>_ptr)到了文件末尾，然后再发生读/写操作时，标志位(fp->_flag)才会被置为含有_IOEOF。然后再调用feof()，才会得到文件结束的信息。
*/
void cat(int client, FILE *resource)
{
    char buf[1024];
    
    // 从resource中读取一行
    fgets(buf, sizeof(buf), resource);
    // 将文件中的内容发送到客户端
    // fgets是逐行读取文件内容
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{   
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
/** @brief	执行一个CGI脚本.可能需要设置适当的环境变量.
 * @prama	client	客户端socket文件描述符
 * @prama	path	CGI 脚本路径
 * @prama	method	请求类型
 * @prama	query_string  查询语句*/
void execute_cgi(int client, const char *path,
        const char *method, const char *query_string)
{   // 缓冲区
    char buf[1024];
    // 管道0 是读（出队），1是写（入队);
    int cgi_output[2]; // CGI程序输出管道
    int cgi_input[2]; // CGI程序输入管道
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0) /*GET*/
        // 跳过请求头
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else if (strcasecmp(method, "POST") == 0) /*POST*/
    {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }
        // 如果没有Content-Length:内容长度，post请求
        if (content_length == -1) {
            bad_request(client);
            return;
        }
    }
    else/*HEAD or other*/  /*其他请求方式占时不做处理*/
    {
    }
    /*
        pipe 
        返回值：成功，返回0，否则返回-1。
        参数数组包含pipe使用的两个文件的描述符。
        fd[0]:读管道，fd[1]:写管道。
        必须在fork()中调用pipe()，否则子进程不会继承文件描述符。两个进程不共享祖先进程，就不能使用pipe。但是可以使用命名管道。
    */
    // 创建两个······匿名管道·····    
    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }
    // 负值：创建子进程失败。
    if ( (pid = fork()) < 0 ) {
        cannot_execute(client);
        return;
    }
    // 发送响应头
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    // 零：返回到新创建的子进程。
    if (pid == 0)  /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];
        // 0 - r . 1 - w;
        ////复制cgi_output[1](读端)到子进程的标准输出
        // 输出管道 ，输入表述替换为stdout;
        dup2(cgi_output[1], STDOUT); // 拷贝文件描述符，将当前进程的标准出，定义为“输出管道的输入”；
        //复制cgi_input[0](写端)到子进程的标准输入
        // 输入管道，输初描述符替换为stdin
        dup2(cgi_input[0], STDIN);   // 拷贝文件描述符，将当前进程的标准输入，定义为“输入管道的输出”
        // 多余的文字描述符

        close(cgi_output[0]);   //子进程关闭 输出管道读端；
        close(cgi_input[1]); // 子进程关闭输入管道写端;
        // 设置环境变量(忘记了系统相关，还是进程相关的了)
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        // 设置环境变量
        // 在当前进程执行程序
        execl(path, NULL);
        // 退出当前进程
        exit(0);
    } else {    /* parent 正值：返回给父亲或调用者。该值包含新创建子进程的进程pid。 */
        close(cgi_output[1]); // 主进程关闭 输出管道的写端;
        close(cgi_input[0]);  //主进程关闭 输入管道的读端；
        // 读取post
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                // post后续还有参数，需要发送到cgi
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        // 读这个管道输出output[0], socket文件描述符发送到client
        // 读取输出管道的读端，将数据发送给客户端。
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        close(cgi_output[0]);
        close(cgi_input[1]);
        //waitpid会暂时停止目前进程的执行，直到有信号来到或子进程结束。
        waitpid(pid, &status, 0);
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor + socket描述符
 *             the buffer to save the data in 
 *             the size of the buffer 缓冲区
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/

/*
// flag 设置为0，则读取TCP缓冲区中的数据到buff中，
// 并从tcp缓冲区中移除已读取的数据 
//flag 设置为 MSG_PEEK, 仅仅把TCP缓冲区的数据读取到buff中，并没有移除tcp缓冲区中的数据
// 再次调用recv还能接收到数据;
*/
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        // 从sock中读取一个字符
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            // 将 /r/n 或者 /r 转变成/n
            if (c == '\r')
            {   
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
/*
    填充响应头:
*/
void headers(int client, const char *filename)
{
    char buf[1024];
    char suffix[8];
    char type[24];
    get_file_suffix(filename, suffix);
    suffix2type(suffix, type);
    // 响应行
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    // 响应头：服务器名
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    // 响应头：Content-Type
    sprintf(buf, "Content-Type: %s\r\n", type);
    send(client, buf, strlen(buf), 0);
    // 空行;
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

// 获取文件的.后缀
void get_file_suffix(const char *filename, char *suffix)
{
    int suffix_len = 0;
    for (int i = strlen(filename) - 1; i > 0; i--)
    {
        if (filename[i] != '.')
        {
            suffix_len++;
            continue;
        }
        // 将.之后文件类型返回哎
        strcpy(suffix, filename + i + 1);
        suffix[suffix_len] = '\0';
        break;
    }
}
// 具体类别=>类别补全;
/*
    html => text/html;
    htm => text/html;
    css => css/html;
    txt => text/plain;
    js => application/javascript;
*/
void suffix2type(const char *suffix, char *type)
{
    if (strcasecmp(suffix, "html") == 0)
    {
        strcpy(type, "text/html");
    }
    else if (strcasecmp(suffix, "htm") == 0)
    {
        strcpy(type, "text/html");
    }
    else if (strcasecmp(suffix, "txt") == 0)
    {
        strcpy(type, "text/plain");
    }
    else if (strcasecmp(suffix, "xml") == 0)
    {
        strcpy(type, "text/xml");
    }
    else if (strcasecmp(suffix, "js") == 0)
    {
        strcpy(type, "application/javascript");
    }
    else if (strcasecmp(suffix, "css") == 0)
    {
        strcpy(type, "text/css");
    }
    else if (strcasecmp(suffix, "pdf") == 0)
    {
        strcpy(type, "application/pdf");
    }
    else if (strcasecmp(suffix, "json") == 0)
    {
        strcpy(type, "application/json");
    }
    else if (strcasecmp(suffix, "jpg") == 0)
    {
        strcpy(type, "image/jpeg");
    }
    else if (strcasecmp(suffix, "png") == 0)
    {
        strcpy(type, "image/png");
    }
    else if (strcasecmp(suffix, "ico") == 0)
    {
        strcpy(type, "image/x-icon");
    }
    else if (strcasecmp(suffix, "gif") == 0)
    {
        strcpy(type, "image/gif");
    }
    else if (strcasecmp(suffix, "tif") == 0)
    {
        strcpy(type, "image/tiff");
    }
    else if (strcasecmp(suffix, "bmp") == 0)
    {
        strcpy(type, "application/x-bmp");
    }
    else 
    {
        strcpy(type, "application/octet-stream");
    }
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
/* 
    404 not found响应;
*/ 
void not_found(int client)
{
    char buf[1024];
    // 请求行
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    // 请求头
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    // 空行隔开
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    // 请求体
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
/*
    响应静态资源;
*/
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];
    int n;

    buf[0] = 'A'; buf[1] = '\0';
    // 读取并丢弃其他消息头
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));
    
    resource = fopen(filename, is_text_type(filename) ? "r" : "rb");
    // 请求资源不存在，404 not found;
    if (resource == NULL)
        not_found(client);
    else
    {   
        // 填充响应头
        headers(client, filename); // filename是字符串
        
        cat(client, resource); // resource是fopen返回的文件描述符
    }
    // 关闭File *
    fclose(resource); 
}

/*
    是否是txt类型： 
        html,htm,json,xml,js,css;
*/
int is_text_type(const char *filename)
{
    char suffix[8];
    get_file_suffix(filename, suffix);

    if (strcasecmp(suffix, "html") == 0)
    {
        return 1;
    }
    else if (strcasecmp(suffix, "htm") == 0)
    {
        return 1;
    }
    else if (strcasecmp(suffix, "txt") == 0)
    {
        return 1;
    }
    else if (strcasecmp(suffix, "xml") == 0)
    {
        return 1;
    }
    else if (strcasecmp(suffix, "js") == 0)
    {
        return 1;
    }
    else if (strcasecmp(suffix, "css") == 0)
    {
        return 1;
    }
    else if (strcasecmp(suffix, "json") == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
/*  此功能开始侦听Web连接的过程在指定的端口上。 
    如果端口为0，则动态分配一个port并修改原始的port变量以反映实际端口。
    参数：指向包含要连接的端口的变量的指针
    返回：套接字
*/
int startup(u_short *port)
{
    int httpd = 0;
    int on = 1;
    struct sockaddr_in name;
    //创建一个socket,域是PF_INET， 面向可靠连接的，默认协议
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");
    // name赋值为0
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET; //设置协议族AF_INET（TCP/IP – IPv4）
    name.sin_port = htons(*port); // 转换成字节序，//设置端口
    name.sin_addr.s_addr = htonl(INADDR_ANY); //将主机数转换成无符号长整型的网络字节顺序
                                            /*这边应该泛指所有网卡的，该端口*/
    //（一般不会立即关闭而经历TIME_WAIT的过程）后想继续重用该socket：
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)  
    {  
        error_die("setsockopt failed");
    }
    // 绑定socket到指定端口
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    if (*port == 0)  /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        /*
            getsockname-获取本地地址；
            比如，在绑定的时候设置端口号为0由系统自动选择端口绑定，
            或者使用了INADDR_ANY通配所有地址的情况下，后面需要用到具体的地址和端口，
            就可以用getsockname获取地址信息；
            getpeername-获取建立连接的对端的地址和端口；
        */ 
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0) 
        error_die("listen");
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
/* 没有实现的http响应体  */
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}
/*
    返回当前时间;
*/
char* get_time_str(char buf[])
{
    time_t now;
    struct tm *tm;  
    time(&now);
    tm = localtime(&now);
    sprintf(buf, "%s", asctime(tm));
    buf[strlen(buf) - 1] = '\0';
    return buf;
}