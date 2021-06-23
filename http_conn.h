#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include "locker.h"

#include <unistd.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>
#include "sql.h"

class http_conn
{
    public:
    // 文件名最大长度
    static const int FILENAME_LEN = 200;
    // 读缓冲区大小
    static const int READ_BUFFER_SIZE = 2048;
    // 写缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;
    // HTTP请求方法，仅支持GET
    enum METHOD{
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };

    // 解析客户请求是，主状态机所出的状态
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE = 0,        // 当前正在分析请求行
        CHECK_STATE_HEADER,                 // 当前正在分析头部字段
        CHECK_STATE_CONTENT                 // 当前正在分析内容
    };

    // 服务器处理HTTP请求的可能结果
    enum HTTP_CODE{
        NO_REQUEST,                 // 表示请求不完整，需要继续读取客户数据
        GET_REQUEST,                // 表示获得一个完成的客户请求
        BAD_REQUEST,                // 表示客户请求有语法错误
        NO_RESOURCE,                // 
        FORBIDDEN_REQUEST,          // 表示客户对资源没有足够的访问权限
        FILE_REQUEST,               // GET文件请求
        INTERNAL_ERROR,             // 表示服务器内部错误
        CLOSED_CONNECTION,          // 表示客户端已经关闭连接
        POST_REQUEST                // POST请求
    };

    // 行的读取状态
    enum LINE_STATUS{
        LINE_OK = 0,    // 读取到一个完整的行
        LINE_BAD,       // 行出错
        LINE_OPEN       // 行数据尚且不完整
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    // 初始化新接收的连接
    void init(int sockfd, const sockaddr_in& addr, SSL* ssl);
    // 关闭连接
    void close_conn(bool real_close = true);
    // 处理客户请求
    void process();
    // 非阻塞读操作
    bool read();
    // 非阻塞写操作
    bool write();

private:
    // 初始化连接
    void init();
    // 解析HTTP请求
    HTTP_CODE process_read();
    // 填充HTTP应答
    bool process_write(HTTP_CODE ret);

    // process_read调用以分析HTTP请求
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    char* get_line() {return m_read_buf + m_start_line; };
    LINE_STATUS parse_line();

    // process_write调用以填充HTTP应答
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

    bool add_content_range(int content_len);
    bool add_content_type();
    bool add_cookie_content();

    int get_upload_line(const char* text, int cur_pos);

public:
    // 所有socket上的时间都被注册到同一个epoll内核事件表中，所以将其设置为静态的
    static int m_epollfd;
    // 统计用户数量
    static int m_user_count;

private:
    // 该HTTP连接的socket和对方的socket地址
    int m_sockfd;
    sockaddr_in m_address;

    // 读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];
    // 表示读缓冲中已经读取的客户数据的最后一个字节的下一个位置
    int m_read_idx;
    // 当前正在分析的字符在读缓冲区中的位置
    int m_checked_idx;
    // 当前正在解析的行的起始位置
    int m_start_line;
    // 写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];
    // 写缓冲区中带发送的字节数
    int m_write_idx;

    // 存储请求体
    char* m_content;

    //
    SSL* m_ssl;

    // 主状态机当前所处的状态
    CHECK_STATE m_check_state;
    // 请求方法
    METHOD m_method;

    // 客户请求的目标文件的完整路径，其内容等于doc_root+m_url，doc_root是网站根目录
    char m_read_file[FILENAME_LEN];
    // 客户请求的目标文件的文件名
    char* m_url;
    // HTTP协议版本号，仅支持HTTP/1.1
    char* m_version;
    // 主机名
    char* m_host;
    // HTTP请求的消息体的长度
    int m_content_length;
    // HTTP请求是否要求保持连接
    bool m_linger;

    // range: 
    // char* m_range;
    bool m_range;
    int m_range_start;
    int m_range_end;
    int m_chunk_size;

    // 客户请求的目标文件被mmap到内存中的起始位置
    char* m_file_address;
    // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读、并获取文件大小等信息
    struct stat m_file_stat;

    // 我们将采用writev来执行写操作
    // m_iv_count表示被写内存块的数量
    struct iovec m_iv[2];
    int m_iv_count;

    int bytes_to_send;
    int bytes_have_send;

    bool m_is_blob;

    // cookie
    bool m_cookie;
    char* m_cookie_content;
    // 存放登录账户名
    // char* m_user;
    char m_user[100];

};


#endif