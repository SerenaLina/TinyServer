#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <string.h>
#include <cerrno>
#include <cstdio>
#include <cassert>
#include <sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include<unistd.h>
#include <fcntl.h>
#include <gtest/gtest_prod.h>
#include "sql_connection_pool.h"
#include "locker.h"
#include <sys/mman.h>
#include <sys/uio.h>

#define BUFFER_SIZE 1000
#define READ_BUFFER_SIZE 40960
#define WRITE_BUFFER_SIZE 40960

class http_conn {
    private:
        char m_buffer_read[READ_BUFFER_SIZE];
        char m_buffer_write[WRITE_BUFFER_SIZE];
        int  m_connfd;
    public:
        static const int FILENAME_LEN = 200;
        enum METHOD{GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATH};
        enum CHECK_STATE{CHECK_STATE_REQUESTLINE=0,CHECK_STATE_HEADER,CHECK_STATE_CONTENT};
        enum HTTP_CODE{NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION};
        enum LINE_STATUS{LINE_OK=0,LINE_BAD,LINE_OPEN};
    public:
        void connect_socket(int connfd);
        bool read_once();
        void close_conn(bool real_close = true);
        void process();
        bool write();
        void init(int sockfd,const sockaddr_in &addr);
    // test interface
    public:
        static int m_epoll_fd;
        static int m_user_count;

    private:
        void init();
        LINE_STATUS parse_line();
        HTTP_CODE parse_request_line(char *text);
        HTTP_CODE parse_header(char *text);
        HTTP_CODE parse_content(char *text);
        HTTP_CODE process_read();
        bool process_write(HTTP_CODE ret);
        char *get_line() { return m_buffer_read+m_start_line; };
        HTTP_CODE do_request();
        void initmysql_result();

        bool add_response(const char* format,...);
        bool add_status_line(int status,const char* title);
        bool add_headers(int content_len);
        bool add_content_length(int content_len);
        bool add_blank_line();
        bool add_linger();
        bool add_content(const char* content);
        void unmap();
    private:
        char *m_url;
        char *m_version;
        char m_real_file[FILENAME_LEN];
        sockaddr_in m_address;
        METHOD m_method;
        CHECK_STATE m_check_state;
        bool m_linger;
        int m_content_length;
        char *m_host;
        int m_read_idx;
        int m_write_idx;
        int m_checked_length;
        struct stat m_file_stat; //文件信息
        char *m_file_address; //文件在内存空间的地址
        char *m_string;
        int m_start_line;
        struct iovec m_iv[2];
        int m_iv_count;
        int cgi;

    FRIEND_TEST(HttpHeaderParserTest, ParseConnectionKeepAlive);
    FRIEND_TEST(HttpHeaderParserTest, ParseContentLength);
    FRIEND_TEST(HttpHeaderParserTest, ParseHost);
    FRIEND_TEST(HttpContentParserTest,ParseContent);
    FRIEND_TEST(HttpProcessTest,DoRequestTest);
    FRIEND_TEST(HttpHeaderParserTest,ParseRequestLine);
    FRIEND_TEST(HttpProcessTest,FileRequestTest);
    FRIEND_TEST(HttpProcessTest,WriteResponseTest);
};

#endif