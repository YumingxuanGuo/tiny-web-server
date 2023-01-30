#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool {
public:
    MYSQL *GetConnection();
    bool ReleaseConnection(MYSQL *conn);
    int GetFreeConn();
    void DestroyPool();
    
    static connection_pool *GetIntance();

    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log); 

private:
    connection_pool();
    ~connection_pool();

    int MaxConn;
    int CurConn;
    int FreeConn;
    locker lock;
    list<MYSQL*> ConnList;
    sem reserve;

public:
    string url;
    string port;
    string user;
    string password;
    string database_name;
    int close_log;
};

class connectionRAII {
public:
    connectionRAII(MYSQL **conn, connection_pool *conn_pool);
    ~connectionRAII();

private:
    MYSQL *connRAII;
    connection_pool *poolRAII;
};

#endif