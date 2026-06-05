#include "sql_connection_pool.h"
#include<mysql/mysql.h>
#include <iostream>
#include<pthread.h>

connection_pool* connection_pool::connPool = NULL;

connection_pool::connection_pool(int MaxConn,string user,
    string password,int port,string DataBaseName,string host) {
        this -> user = user;
        this -> password = password;
        this -> port = port;
        this -> DataBaseName = DataBaseName;
        this -> MaxConn = MaxConn;
        this -> host = host;
        pthread_mutex_lock(&lock);
        // 临界区，分配连接池时其它线程不许使用连接池。
        for(int i = 0 ; i< MaxConn ; i ++) {
            MYSQL *conn = NULL;
            conn = mysql_init(conn);
            if(conn == NULL) {
                cout << "Error:" << mysql_error(conn);
                exit(1);
            }
            conn = mysql_real_connect(conn,host.c_str(),user.c_str(),password.c_str(),DataBaseName.c_str(),port,NULL,0);
            if(conn == NULL) {
                cout<<"Error: "<<mysql_error(conn);
			    exit(1);
            }
            connList.push_back(conn);
            freeConn++;
        }

    this -> currConn = 0;
    pthread_mutex_unlock(&lock);

}

MYSQL*  connection_pool::GET_CONNECTION() {
    MYSQL* con = NULL;
    pthread_mutex_lock(&lock);
    // 临界区
    if(connList.size() > 0) {
        con = connList.front();
        connList.pop_front();
        freeConn--;
        currConn++;
        pthread_mutex_unlock(&lock);
        return con;
    }
    return NULL;
}

connection_pool * connection_pool::GET_INSTANCE(int MaxConn,string user,string password,
        int port,string DataBaseName,string host) {
            if(connPool == NULL) {
                connPool = new connection_pool(MaxConn,user,password,port,DataBaseName,host);
            }
            return connPool;
}

bool connection_pool::RELEASE_CONNECTION(MYSQL* conn) {
    pthread_mutex_lock(&lock);
    if(conn != NULL) {
        connList.push_back(conn);
        ++freeConn;
        --currConn;
        pthread_mutex_unlock(&lock);
        return true;
    }
    return false;
}

void connection_pool::DESTROY_POOL()
{
	pthread_mutex_lock(&lock);
	if(connList.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for(it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL * con = *it;
			mysql_close(con);
		}
	    currConn = 0;
		freeConn = 0;
		connList.clear();
	}
}

int connection_pool::GET_FREE_CONN()
{
	return this->freeConn;
}


connection_pool::~connection_pool()
{
	DESTROY_POOL();
}

