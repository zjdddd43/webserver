#include "http_conn.h"

const char* ok_200_title = "OK";
const char* ok_206_title = "Partial Content";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
// const char* error_400_form = "Bad Request.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

const char* doc_root = "/home/zjd/webserver/resource";
const char* tools_root = "/home/zjd/webserver/tools/";
const char* videojs_root = "/home/zjd/webserver/resource/videojs/";
// const char* mp4_root = "/home/zjd/zjd/zjd/TinyWebServer/root";
// const char* user_root = "/home/zjd/users/";

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn(bool real_close)
{
    if(real_close && (m_sockfd != -1))
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::init(int sockfd, const sockaddr_in& addr, SSL* ssl)
{
    m_ssl = ssl;
    m_sockfd = sockfd;
    m_address = addr;
    //
    // int reuse = 1;
    // setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    init();
}

void http_conn::init()
{
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;

    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    m_range = false;
    // 
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_chunk_size = 1024*1024;
    m_is_blob = false;
    m_cookie = false;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_read_file, '\0', FILENAME_LEN);
}

http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for( ; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r')
        {
            if((m_checked_idx+1) == m_read_idx)
            {
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n')
        {
            if((m_checked_idx > 1) && (m_read_buf[m_checked_idx-1] == '\r'))
            {
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

bool http_conn::read()
{
    if(m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }

    int bytes_read = 0;
    while(true)
    {
        // bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        bytes_read = SSL_read(m_ssl, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx);
        if(bytes_read == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if(bytes_read == 0)
        {
            return false;
        }

        m_read_idx += bytes_read;
    }
    return true;

}

http_conn::HTTP_CODE http_conn::parse_request_line(char* text)
{
    m_url = strpbrk(text, " ");
    if(!m_url)
    {
        return BAD_REQUEST;
    }

    *m_url++ = '\0';

    char* method = text;
    if(strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if(strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
    }
    else
    {
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, " ");
    m_version = strpbrk(m_url, " ");
    if(!m_version)
    {
        return BAD_REQUEST;
    }

    *m_version++ = '\0';
    m_version += strspn(m_version, " ");
    if(strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    // printf("url: %s \n", m_url);
    if(strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;

}

http_conn::HTTP_CODE http_conn::parse_headers(char* text)
{
    if(text[0] == '\0')
    {
        if(m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " ");
        if(strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if(strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " ");
        m_content_length = atol(text);
    }
    else if(strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " ");
        m_host = text;
    }
    else if(strncasecmp(text, "Cookie:", 7) == 0)
    {
        text += 7;
        text += strspn(text, " ");
        m_cookie_content = text;
        m_cookie = true;

    }
    else if(strncasecmp(text, "Range:", 6) == 0)
    {
        // m_range = true;
        text += 6;
        text += strspn(text, " ");
        // bytes=0-   --->    0-
        text += 6;
        
        char* end_str = strstr(text, "-");
        if(strlen(end_str) == 1)
        {
            m_range_end = -1;
            end_str[0] = '\0';
        } 
        else
        {
            end_str[0] = '\0';
            end_str += 1;
            m_range_end = atoi(end_str);
        }
        m_range_start = atoi(text) == 0 ? 0 : atoi(text)+1;
        
        // printf("start:%d end:%d\n", m_range_start, m_range_end);
    }
    else 
    {
        // printf("oop! unknow header %s\n", text);
    }
    return NO_REQUEST;

}


http_conn::HTTP_CODE http_conn::parse_content(char* text)
{
    if(m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        m_content = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;
    m_range = false;

    while(((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        printf("got 1 http line: %s\n", text);

        switch(m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_headers(text);
                if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST)
                {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content(text);
                if(ret == GET_REQUEST)
                {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

int http_conn::get_upload_line(const char* text, int cur_pos)
{
    if(cur_pos >= strlen(text))
    {
        return -1;
    }
    int len = strlen(text);
    int i;
    for(i = cur_pos; i < len; ++i)
    {
        if(text[i] == '\r' && text[i+1] == '\n')
        {
            break;
        }
    }
    return i;
}

http_conn::HTTP_CODE http_conn::do_request()
{   
    strcpy(m_read_file, doc_root);
    int len = strlen(doc_root);
    // strncpy(m_read_file + len, m_url, FILENAME_LEN - len - 1);
    printf("m_url:%s\n", m_url);

    // post
    if(m_method == POST)
    {

        // 上传文件
        // 现只能上传txt文件
        if(strcmp(m_url+strlen(m_url)-6, "upload") == 0)
        {
            // printf("upload----m_content:%s\n", m_content);
            // 处理txt文件
            char upload_file_name[200];
            int prev_pos = 0;
            int cur_pos = 0;
            int is_content = false;
            char filename[200];


            while((cur_pos = get_upload_line(m_content, prev_pos)) != -1)
            {
                char str[cur_pos-prev_pos+1];
                int len = cur_pos - prev_pos;
                for(int i = 0; i < len; ++i)
                {
                    str[i] = m_content[prev_pos+i];
                }
                str[len] = '\0';
                // printf("s:%s\n", str);
                prev_pos = cur_pos+2;

                if(is_content)
                {
                    // 创建文件并写入文件
                    
                    char path[200];
                    sprintf(path, "%s%s%s/%s", doc_root, "/login/", m_user, filename);
                    // printf("path:%s\n", path);
                    // 创建文件并写入内容
                    FILE* f;
                    f = fopen(path, "w");
                    fprintf(f, "%s", str);

                    fclose(f);

                    is_content = false;
                }

                if(strcmp(str, "") == 0)
                {
                    // printf("the next is txt content...\n");
                    is_content = true;

                }

                if(strncasecmp(str, "Content-Disposition:", 20) == 0)
                {
                    // 获取上传文件名
                    str[strlen(str)-1] = '\0';
                    char* name = strstr(str, "filename=");
                    // printf("filename:%s\n", name+10);
                    for(int i = 0; i < strlen(name+10); i++)
                    {
                        filename[i] = *(name+10+i);
                    }
                    // printf("filename:%s\n", filename);
                }

            }

            // 上传成功
            strncpy(m_read_file + len, "/upload_success", FILENAME_LEN - len - 1);
        }

        // 登入
        else if(strncasecmp(m_url, "/login", 6) == 0)
        {
            
            // 获取登录账号和密码
            // char user[100];
            char password[100];
            
            int i;
            for(i = 5; m_content[i] != '&'; ++i)
            {
                m_user[i-5] = m_content[i];
            }
            m_user[i-5] = '\0';

            int j = 0;
            for(i = i + 10; m_content[i] != '\0'; ++i, ++j)
            {
                password[j] = m_content[i];
            }
            password[j] = '\0';

            // 从数据库中搜索user和password是否正确
            // 
            char query[100];
            sprintf(query, "select password from users where username = '%s'", m_user);
            // printf("query:%s\n", query);
            // query
            MYSQL_RES* result = SQL::get_instance()->sql_query(query);
            MYSQL_ROW row;
            if((row = mysql_fetch_row(result)) != NULL)
            {
                if(strcmp(row[0], password) == 0)
                {
                    // 返回用户目录页面
                    char dir_generate[200];
                    char user_path[200];
                    char dir_path[200];
                    sprintf(user_path, "%s/login/%s", doc_root, m_user);
                    // printf("user_path:%s\n", user_path);
                    sprintf(dir_generate, "%s%s %s", tools_root, "dir_generate", user_path);
                    // printf("dir_generate:%s\n", dir_generate);

                    // 实时生成用户目录
                    system(dir_generate);

                    sprintf(dir_path, "%s%s", m_url,"/dir.html");
                    // printf("dir_path:%s\n", dir_path);

                    strncpy(m_read_file + len, dir_path, FILENAME_LEN - len - 1);
                    // printf("m_read_file:%s\n", m_read_file);
                    m_cookie = true;
                }
                else
                {
                    // 密码错误
                    strncpy(m_read_file + len, "/login_error.html", FILENAME_LEN - len - 1);
                    memset(m_user, '\0', 100);
                }
            }
            else
            {
                // 账号不存在
                strncpy(m_read_file + len, "/login_error.html", FILENAME_LEN - len - 1);
                memset(m_user, '\0', 100);
            }
            mysql_free_result(result);
        }
        // 注册
        else if(strcmp(m_url, "/register") == 0)
        {
            // 向数据库添加user和password
            // 成功后返回login.html
            
            char password[100];
            char user[100];
            
            int i;
            for(i = 5; m_content[i] != '&'; ++i)
            {
                user[i-5] = m_content[i];
            }
            user[i-5] = '\0';

            int j = 0;
            for(i = i + 10; m_content[i] != '\0'; ++i, ++j)
            {
                password[j] = m_content[i];
            }
            password[j] = '\0';

            char query[100];
            sprintf(query, "select password from users where username = '%s'", user);
            MYSQL_RES* result = SQL::get_instance()->sql_query(query);
            MYSQL_ROW row;
            // 账号已存在
            if((row = mysql_fetch_row(result)) != NULL)
            {
                
                strncpy(m_read_file + len, "/register_error.html", FILENAME_LEN - len - 1);
            }
            else
            {
                char insert_query[100];
                sprintf(insert_query, "insert into users (username, password) values (\"%s\", \"%s\")", user, password);

                // 注册成功
                if(SQL::get_instance()->sql_insert(insert_query))
                {
                    printf("insert success...\n");
                    strncpy(m_read_file + len, "/login.html", FILENAME_LEN - len - 1);
                    
                    // 创建用户空间
                    // 在root_user中创建对应用户的文件夹
                    char path[200];
                    sprintf(path, "mkdir %s/login/%s", doc_root, user);
                    printf("path:%s\n", path);
                    system(path);
                }
                // 注册失败
                else
                {
                    strncpy(m_read_file + len, "/register_error.html", FILENAME_LEN - len - 1);
                }
            }

        }
        else
        {
            // 其他post请求

        }
    }
    else
    {
        // get请求
        char user[100];
        sprintf(user, "/login/%s", m_user);
        printf("user:%s\n", user);
        // 没有登录时，user为/login/，所以用户无法跳过登录直接获取资源
        if(strcmp(m_url, user) == 0)
        {
            // 返回用户目录
            //
            char dir_generate[200];
            char user_path[200];
            char dir_path[200];
            sprintf(user_path, "%s/login/%s", doc_root, m_user);
            // printf("user_path:%s\n", user_path);
            sprintf(dir_generate, "%s%s %s", tools_root, "dir_generate", user_path);
            // printf("dir_generate:%s\n", dir_generate);

            // 实时生成用户目录
            system(dir_generate);

            sprintf(dir_path, "%s%s", m_url,"/dir.html");
            // printf("dir_path:%s\n", dir_path);

            strncpy(m_read_file + len, dir_path, FILENAME_LEN - len - 1);
        }
        // 处理m3u8文件请求
        else if(strcmp(m_url+strlen(m_url)-5, "-m3u8") == 0)
        {
            char video_generate[200];
            char video_path[200];
            sprintf(video_generate, "%s%s %s", tools_root, "play_m3u8", m_url);
            // printf("video_generate:%s\n", video_generate);

            system(video_generate);
            sprintf(video_path, "%s%s", "/videojs/","play-m3u8.html");
            // printf("video_path:%s\n", video_path);
            // strncpy(m_read_file, video_path, FILENAME_LEN);
            strncpy(m_read_file + len, video_path, FILENAME_LEN - len - 1);
            
        }
        else
        {
            
            strncpy(m_read_file + len, m_url, FILENAME_LEN - len - 1);
            // printf("m_read_file:%s\n", m_read_file);
        }
        // strncpy(m_read_file + len, m_url, FILENAME_LEN - len - 1);
    }


    if(stat(m_read_file, &m_file_stat) < 0)
    {
        return NO_REQUEST;
    }

    if(!(m_file_stat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }
    if(S_ISDIR(m_file_stat.st_mode))
    {
        return BAD_REQUEST;
    }

    // printf("m_read_file:%s\n", m_read_file);
    
    int fd = open(m_read_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    m_is_blob = false;
    return FILE_REQUEST;
}

void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::write()
{
    int temp = 0;

    if(bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }
    // printf("start:%d, end:%d\n", m_range_start, m_range_end);
    while(1)
    {
        if(bytes_have_send < m_iv[0].iov_len)
        {
            temp = SSL_write(m_ssl, m_iv[0].iov_base, m_iv[0].iov_len);
        }
        else
        {
            temp = SSL_write(m_ssl, m_iv[1].iov_base, m_iv[1].iov_len);
        }
        // temp = writev(m_sockfd, m_iv, m_iv_count);
        if(temp <= -1)
        {
            if(errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;

        // 响应头发送完
        if(bytes_have_send >= m_iv[0].iov_len)
        {
            // m_iv[0]存放的是响应头，此时发送完，将其置为0
            m_iv[0].iov_len = 0;
            // m_iv[1]存放的是文件内容，将其base位置设置为m_file_address + (bytes_have_send - m_write_idx)
            // bytes_have_send - m_write_idx为除了响应头以外发送的内容
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            // 此时还需要发送文件长度为bytes_to_send
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            // 响应头没发送完
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        // 发送完
        if(bytes_to_send <= 0)
        {
            unmap();
            if(m_linger)
            {
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            }
            else
            {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}


bool http_conn::add_response(const char* format, ...)
{
    if(m_write_idx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    
    if(len >= (WRITE_BUFFER_SIZE-1-m_write_idx))
    {
        return false;
    }

    m_write_idx += len;
    va_end(arg_list);
    return true;

}


bool http_conn::add_status_line(int status, const char* title)
{
    return add_response("%s %d %s \r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len)
{
//     if(m_range)
//     {
//         if(m_range_end == -1)
//         {
//             m_range_end = m_range_start + m_chunk_size > content_len ? content_len-1 : m_range_start + m_chunk_size-1;
//             printf("m_range_end : %d\n", m_range_end);
//         }
//         add_content_length(m_range_end-m_range_start);
//         add_content_range(content_len);
//         add_content_type();
//     }
//     else
//     {
//         add_content_length(content_len);
//     }

    add_content_length(content_len);
    if(m_cookie)
    {
        add_cookie_content();
        m_cookie = false;
    }
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_range(int content_len)
{
    return add_response("Content-Range:bytes %d-%d/%d\r\n", m_range_start, m_range_end, content_len);
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_content_type()
{
    return add_response("content-type:%s", "application/x-mpegURL\r\n");
}

bool http_conn::add_cookie_content()
{
    char cookie[200];
    // sprintf(cookie, "name=%s;value=%s;Domain=%s;path=%s;expires=%s;max-age=%s");
    if(strlen(m_user) != 0)
    {
        sprintf(cookie, "name=%s;", m_user);
        // printf("cookie:%s\n", cookie);
        return add_response("Set-Cookie:%s\r\n", cookie);
    }
    else return false;
}

bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char* content)
{
    return add_response("%s", content);
}

bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if(!add_content(error_500_form))
        {
            return false;
        }
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(400, error_400_title);
        add_headers(strlen(error_400_form));
        if(!add_content(error_400_form))
        {
            return false;
        }
        break;
    }
    case NO_RESOURCE:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if(!add_content(error_404_form))
        {
            return false;
        }
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if(!add_content(error_403_form))
        {
            return false;
        }
        break;
    }
    case FILE_REQUEST:
    {
        if(!m_range)
        {
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size != 0)
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else
            {
                const char* ok_string = "<html><body></body><html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string))
                {
                    return false;
                }
            }
        }
        else
        {
            add_status_line(206, ok_206_title);
            if(m_file_stat.st_size != 0)
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address + m_range_start;
                m_iv[1].iov_len = m_range_end - m_range_start;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_range_end - m_range_start;
                return true;
            }
        }
    }
    
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    
    bool write_ret = process_write(read_ret);

    if(!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}