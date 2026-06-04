#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_
#include <mysql/mysql.h>
#include<list>
#include <string>
#include <gtest/gtest_prod.h>
using namespace std;
class connection_pool {
    public:
        MYSQL* GET_CONNECTION();
        // 属于类本身，不依赖实例
        static connection_pool *GET_INSTANCE(int MaxConn,string user,string password,
        int port,string DataBaseName,string host);
    private:

        pthread_mutex_t lock;
        list<MYSQL *> connList;
        connection_pool(int MaxConn,string user,string password,
        int port,string DataBaseName,string host);

    private:
        int MaxConn;
        int freeConn;
        int currConn;
    private:
        string user;
        string password;
        string host;
        int port;
        string DataBaseName;
        static connection_pool *connPool;
        FRIEND_TEST(DataBaseConnectionPoolTest,InstanceInitTest);
};

#endif __CONNECTION_POOL_