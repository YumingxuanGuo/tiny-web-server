#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>


#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

using namespace std;

const int MAX_FD = 65536;           // max # file descriptors
const int MAX_EVENT_NUMBER = 10000; // max # events
const int TIMESLOT = 5;             // min units of timeout

class WebServer {
public:
    WebServer();
    ~WebServer();

    void init(int port , string user, string passWord, string databaseName,
              int log_write , int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void event_listen();
    void event_loop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void process_timer(util_timer *timer, int sockfd);
    bool process_client_data();
    bool process_signal(bool &timeout, bool &stop_server);
    void process_read(int sockfd);
    void process_write(int sockfd);

public:
    // basics
    char *root;
    int port;
    int log_write;
    int close_log;
    int actormodel;

    int pipefd[2];
    int epollfd;
    http_conn *users;

    // database
    connection_pool *conn_pool;
    string user;
    string password;
    string database_name;
    int sql_num;

    // threadpool
    threadpool<http_conn> *pool;
    int thread_num;

    // epoll_event
    epoll_event events[MAX_EVENT_NUMBER];

    int listenfd;
    int OPT_LINGER;
    int trigger_mode;
    int listen_trigger_mode;
    int connect_trigger_mode;

    // timer
    client_data *users_timer;
    Utils utils;
};

#endif
