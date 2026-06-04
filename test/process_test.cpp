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
    EXPECT_EQ(result,http_conn::FILE_REQUEST);
}