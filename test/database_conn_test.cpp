#include <gtest/gtest.h>
#include "../sql_connection_pool.h"

TEST(DataBaseConnectionPoolTest,InstanceInitTest) {
    connection_pool *conn = connection_pool::GET_INSTANCE(10,"tiny_http","root",3306,"http_database","localhost");
    ASSERT_NE(conn,nullptr);
    EXPECT_EQ(conn -> user,"tiny_http");
    EXPECT_EQ(conn -> password,"root");
    EXPECT_EQ(conn -> port,3306);
    EXPECT_EQ(conn -> DataBaseName,"http_database");
    EXPECT_EQ(conn -> connList.size(),10);
}