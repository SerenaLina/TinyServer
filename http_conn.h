#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <string.h>
#include <cerrno>
#include <cstdio>
#include <cassert>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/stat.h>
#include<unistd.h>
#include <fcntl.h>
#include <gtest/gtest_prod.h>

#define BUFFER_SIZE 1000
#define READ_BUFFER_SIZE 1000

class http_conn {
    private:
        char m_buffer_read[READ_BUFFER_SIZE];
        int  m_connfd;
    public:
        static const int FILENAME_LEN = 200;
        enum METHOD{GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATH};
        enum CHECK_STATE{CHECK_STATE_REQUESTLINE=0,CHECK_STATE_HEADER,CHECK_STATE_CONTENT};
        enum HTTP_CODE{NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION};
        enum LINE_STATUS{LINE_OK=0,LINE_BAD,LINE_OPEN};
    public:
        bool read_once();
        void init();

    // test interface
    public:
        void run_parse_test();
    private:
        LINE_STATUS parse_line();
        HTTP_CODE parse_request_line(char *text);
        HTTP_CODE parse_header(char *text);
        HTTP_CODE parse_content(char *text);
        HTTP_CODE process_read();
        char *get_line() { return m_buffer_read+m_start_line; };
        HTTP_CODE do_request();
    private:
        char *m_url;
        char *m_version;
        char m_real_file[FILENAME_LEN];
        METHOD m_method;
        CHECK_STATE m_check_state;
        bool m_linger;
        int m_content_length;
        char *m_host;
        int m_read_idx;
        int m_checked_length;
        char *m_string;
        int m_start_line;
        int cgi;

    FRIEND_TEST(HttpHeaderParserTest, ParseConnectionKeepAlive);
    FRIEND_TEST(HttpHeaderParserTest, ParseContentLength);
    FRIEND_TEST(HttpHeaderParserTest, ParseHost);
    FRIEND_TEST(HttpContentParserTest,ParseContent);
    FRIEND_TEST(HttpProcessTest,DoRequestTest);
    FRIEND_TEST(HttpHeaderParserTest,ParseRequestLine);
};

#endif