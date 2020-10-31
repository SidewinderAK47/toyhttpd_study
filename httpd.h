#ifndef HTTPD_H
#define HTTPD_H
// 接收请求并且处理
void accept_request(int, struct sockaddr_in *);
// 无法处理请求，回写400 到client;
void bad_request(int);
// 将文件内容发送至client
void cat(int, FILE *);
// 不可执行cgi程序，回写500到client;
void cannot_execute(int);
// 输出错误信息
void error_die(const char *);
// 执行cgi
void execute_cgi(int, const char *, const char *, const char *);
// 从fd中读取一行，并且将读取的'\r\n'和'\r' 转换成'\n'
int get_line(int, char *, int);
// 返回200 ok给客户端
void headers(int, const char *);

// 返回404状态给客户端。
void not_found(int);
// 处理客户端的文件请求，发送404或200文件内容给client
void serve_file(int, const char *);
// 开启监听
int startup(u_short *);
// 返回501状态给客户端
void unimplemented(int);

void get_file_suffix(const char *, char *);
void suffix2type(const char *, char *);
int is_text_type(const char *);
char* get_time_str(char[]);

#endif