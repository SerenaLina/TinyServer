#include <gtest/gtest.h>
#include "../http_conn.h"

TEST(HttpProcessTest,DoRequestTest) {
    http_conn conn;
    conn.init();
    const char* test_packet = 
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost:9006\r\n"
        "Content-length:13\r\n"
        "Connection: close\r\n"
        "\r\n";
    strcpy(conn.m_buffer_read,test_packet);
    conn.m_read_idx = strlen(test_packet);
    testing::internal::CaptureStdout();
    auto result = conn.process_read();
    std::string output = testing::internal::GetCapturedStdout();
    //EXPECT_EQ(output,"Get header request");
    EXPECT_EQ(result,http_conn::NO_REQUEST);
}

TEST(HttpProcessTest,FileRequestTest) {
    http_conn conn;
    conn.init();
    const char* test_packet =
    "POST /2CGISQL.cgi HTTP/1.1\r\n"
    "Host: 127.0.0.1:9006\r\n"
    "Content-Type: application/x-www-form-urlencoded\r\n"
    "Content-Length: 33\r\n"
    "\r\n"
    "user=testadmin&password=secret123";
    strcpy(conn.m_buffer_read,test_packet);
    conn.m_read_idx = strlen(test_packet);
    auto result = conn.process_read();
    EXPECT_STREQ(conn.m_url,"/logError.html");
}
