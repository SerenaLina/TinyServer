#include <gtest/gtest.h>
#include "../http_conn.h"

TEST(HttpHeaderParserTest,ParseRequestLine) {
    http_conn conn;
    char input[] = "GET /index.html HTTP/1.1\r\n";
    conn.parse_request_line(input);
    auto method = conn.m_method;
    EXPECT_EQ(method,http_conn::GET);
    EXPECT_STREQ(conn.m_url,"/index.html");
}


TEST(HttpHeaderParserTest, ParseConnectionKeepAlive) {
    http_conn conn;
    conn.m_linger = false; 

    char input[] = "Connection:\tkeep-alive";
    
    // 调用函数
    auto result = conn.parse_header(input);
    
    // 断言结果
    EXPECT_EQ(result, http_conn::NO_REQUEST);
    EXPECT_TRUE(conn.m_linger);
}

TEST(HttpHeaderParserTest, ParseContentLength) {
    http_conn conn;
    conn.m_content_length = 0;

    char input[] = "Content-length:\t42";
    
    auto result = conn.parse_header(input);
    
    EXPECT_EQ(result, http_conn::NO_REQUEST);
    EXPECT_EQ(conn.m_content_length, 42);
}

TEST(HttpHeaderParserTest, ParseHost) {
    http_conn conn;

    char input[] = "Host:\tlocalhost";
    
    auto result = conn.parse_header(input);
    
    EXPECT_EQ(result, http_conn::NO_REQUEST);
    EXPECT_STREQ(conn.m_host, "localhost"); 
}


TEST(HttpContentParserTest,ParseContent){
    http_conn conn;
    conn.m_checked_length = 50;
    conn.m_content_length = 10;

    conn.m_read_idx      =  60;
    char input[] = "AAAAAAAAAABBBBBBB";
    auto result = conn.parse_content(input);
    EXPECT_EQ(result,http_conn::GET_REQUEST);
    EXPECT_STREQ(input,"AAAAAAAAAA");
    EXPECT_STREQ(conn.m_string,"AAAAAAAAAA");
}