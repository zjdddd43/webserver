#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <sstream>
#include "sql.h"
#include "log.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern int addfd(int epollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd);

void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int connfd, const char* info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    addsig(SIGPIPE, SIG_IGN);

    // 数据库连接初始化
    SQL::get_instance()->init("localhost", "root", "1", "webserver", 0);

    // 初始化异步日志
    Log::get_instance()->init("test.log");

    // ssl库初始化
    SSL_library_init();
    // 载入所有ssl算法
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    SSL_CTX* ctx;

    ctx = SSL_CTX_new(SSLv23_server_method());

    if(ctx == NULL)
    {
        printf("ctx is null...\n");
        return 1;
    }

    // 载入用户数字证书，此证书用来发给客户端，其中包含公钥
    if(SSL_CTX_use_certificate_file(ctx, argv[3], SSL_FILETYPE_PEM) <= 0)
    {
        printf("use certuficate file failed...\n");
        return 1;
    }

    // 载入用户私钥
    if(SSL_CTX_use_PrivateKey_file(ctx, argv[4], SSL_FILETYPE_PEM) <= 0)
    {
        printf("use private key file failed...\n");
        return 1;
    }

    if(!SSL_CTX_check_private_key(ctx))
    {
        printf("private key correct...\n");
        return 1;
    }

    // 初始化线程池
    threadpool<http_conn>* pool = NULL;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch(...)
    {
        return 1;
    }

    http_conn* users = new http_conn[MAX_FD];
    assert(users);

    int user_count = 0;

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    struct linger tmp = {0, 1};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    int ret = 0;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    ret = listen(listenfd, 5);
    assert(ret >= 0);

    epoll_event events[MAX_EVENT_NUMBER];
    
    int epollfd = epoll_create(5);

    assert(epollfd != -1);

    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    std::stringstream ss;

    while(true)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if((number < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
        }

        for(int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_addrlength);

                // et模式下要不停循环接受直到返回-1且errno==EAGAIN
                // 多个连接同时到达，服务器的 TCP 就绪队列瞬间积累多个就绪连接，
                // 由于是边缘触发模式，epoll 只会通知一次，accept 只处理一个连接，导致 TCP 就绪队列中剩下的连接都得不到处理。
                
                while(1)
                {
                    int connfd = accept(listenfd, (struct sockaddr*) &client_address, &client_addrlength);

                    if(connfd < 0)
                    {
                        // printf("errno is : %d\n", errno);
                        break;
                    }
                    if(http_conn::m_user_count >= MAX_FD)
                    {
                        show_error(connfd, "Internal server busy");
                        // continue;
                        break;
                    }

                    // 
                    SSL* ssl = SSL_new(ctx);
                    SSL_set_fd(ssl, connfd);
                    if(SSL_accept(ssl) <= 0)
                    {
                        printf("can't complete SSL connection...\n");
                        close(connfd);
                        break;
                    }
                    // 初始化客户连接
                    users[connfd].init(connfd, client_address, ssl);

                    ss << "line 192: accept " << inet_ntoa(client_address.sin_addr) << ":" << htons(client_address.sin_port) << " connfd : " << connfd;
                    LOG_DEBUG(ss.str());
                    ss.str("");
                    ss.clear();
                }
                
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 出现异常，关闭客户连接
                users[sockfd].close_conn();
                ss << "line 204: " << "events is " << events[i].events << ", " << "close sockfd : " << sockfd;
                LOG_ERROR(ss.str());
                ss.str("");
                ss.clear();
            }
            else if(events[i].events & EPOLLIN)
            {
                // 根据读的结果，决定是将任务添加到线程池还是关闭连接
                if(users[sockfd].read())
                {
                    pool->append(users + sockfd);
                    ss << "line 215: " << "sockfd :" << sockfd << " read success and append to work queue.";
                    LOG_DEBUG(ss.str());
                    ss.str("");
                    ss.clear();
                }
                else
                {
                    users[sockfd].close_conn();
                    ss << "line 223: " << "sockfd :" << sockfd << " read failed and close connect.";
                    LOG_DEBUG(ss.str());
                    ss.str("");
                    ss.clear();
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                // 根据写的结果，决定是否关闭连接
                if(!users[sockfd].write())
                {
                    users[sockfd].close_conn();
                    ss << "line 235: " << "sockfd :" << sockfd << " write and return false. close connect.";
                    LOG_DEBUG(ss.str());
                    ss.str("");
                    ss.clear();
                }
                else
                {
                    ss << "line 242: " << "sockfd :" << sockfd << " write and return true. keep connect.";
                    LOG_DEBUG(ss.str());
                    ss.str("");
                    ss.clear();
                }
            }
            else
            {}
        }
    }
    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;
    return 0;
    
}