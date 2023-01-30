#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool() {
    CurConn = 0;
    FreeConn = 0;
}

connection_pool *connection_pool::GetIntance() {
    static connection_pool conn_pool;
    return &conn_pool;
}

void connection_pool::init(string Url, string User, string PassWord, string DBName, int Port, int MaxConn, int CloseLog) {
    url = Url;
    port = Port;
    user = User;
    password = PassWord;
    database_name = DBName;
    close_log = CloseLog;

    for (int i = 0; i < MaxConn; i++) {
        MYSQL *conn = NULL;

        conn = mysql_init(con);
        if (con == NULL) {
            LOG_ERROR("MySQL Error");
            exit(1);
        }

		con = mysql_real_connect(con, Url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);
        if (con = NULL) {
            LOG_ERROR("MySQL Error");
            exit(1);
        }

        ConnList.push_back(con);
        FreeConn++;
    }

    reserve = sem(FreeConn);

    MaxConn = FreeConn;
}

MYSQL *connection_pool::GetConnection() {
    MYSQL *conn = NULL;

    if (ConnList.size() == 0) {
        return 0;
    }

    reserve.wait();

    lock.lock();

    conn = ConnList.front();
    ConnList.pop_front();
    FreeConn--;
    CurConn++;

    lock.unlock();

    return conn;
}

bool connection_pool::ReleaseConnection(MYSQL *conn) {
    if (conn == NULL) {
        return false;
    }

    lock.lock();

    ConnList.push_back(conn);
    FreeConn++;
    CurConn--;

    lock.unlock();

    reserve.post();

    return true;
}

void connection_poll:DestroyPool() {
    lock.lock();

    if (ConnList.size() == 0) {
        list<MYSQL*>::iterator it;
        for (it = ConnList.begin(); it != ConnList.end(); ++it) {
            MYSQL *conn = *it;
            mysql_close(con);
        }
        CurConn = 0;
        FreeConn = 0;
        ConnList.clear();
    }

    lock.unlock();
}

int connection_pool::GetFreeConn() {
    return FreeConn;
}

connection_pool::~connection_pool() {
    DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_poll *conn_poll) {
    *SQL = conn_pool->GetConnection();

    connRAII = *SQL;
    poolRAII = conn_pool;
}

connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}