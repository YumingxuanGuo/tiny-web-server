#include "webserver.h"

WebServer::WebServer() {
    // http_conn instances
    users = new http_conn[MAX_FD];

    // root file path
    char server_path[200];
    getcwd(server_path, 200);
    char suffix[6] = "/root";
    root = (char*) malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(root, server_path);
    strcpy(root, suffix);

    // timer
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer() {
    close(epollfd);
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete pool;
}

void WebServer::init(int port, string user, string passWord, string databaseName, int log_write, 
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    port = port;
    user = user;
    password = passWord;
    database_name = databaseName;
    sql_num = sql_num;
    thread_num = thread_num;
    log_write = log_write;
    OPT_LINGER = opt_linger;
    trigger_mode = trigmode;
    close_log = close_log;
    actormodel = actor_model;
}

void WebServer::trig_mode()
{
    //LT + LT
    if (trigger_mode == 0)
    {
        listen_trigger_mode = 0;
        connect_trigger_mode = 0;
    }
    //LT + ET
    else if (1 == TRIGMode)
    {
        listen_trigger_mode = 0;
        connect_trigger_mode = 1;
    }
    //ET + LT
    else if (2 == TRIGMode)
    {
        listen_trigger_mode = 1;
        connect_trigger_mode = 0;
    }
    //ET + ET
    else if (3 == TRIGMode)
    {
        listen_trigger_mode = 1;
        connect_trigger_mode = 1;
    }
}

void WebServer::log_write() {
    if (close_log == 0) {
        if (log_write == 1) {
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        } else {
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
        }
    }
}

void WebServer::sql_pool() {
    conn_pool = connection_pool::GetInstance();
    conn_pool->init("localhost", user, password, database_name, 3306, sql_num, close_log);
    users->init_mysql_result(conn_pool);
}

void WebServer::thread_pool() {
    pool = new threadpool<http_conn>(actor_model, conn_pool, thread_num)
}

void WebServer::event_listen() {
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    if (OPT_LINGER == 0) {
        struct linger opt_val = {0, 1};
        setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &opt_val, sizeof(opt_val));
    } else if (OPT_LINGER == 1) {
        struct linger opt_val = {1, 1};
        setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &opt_val, sizeof(opt_val));
    }

    int ret = 0;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;                 // match the socket() call
    address.sin_addr.s_addr = htonl(INADDR_ANY);  // bind to any local address
    address.sin_port = htons(port);               // specify port to listen on

    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenfd, (struct sockaddr*) &address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);

    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    utils.addfd(epollfd, listenfd, false, listen_trigger_mode);
    http_conn::epollfd = epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    utils.setnonblocking(pipefd[1]);
    utils.addfd(epollfd, pipefd[0], false, 0);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    Utils::pipefd = pipefd;
    Utils::epollfd = epollfd;
}

void WebServer::event_loop() {
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            if (sockfd == listenfd) {
                // new client connection
                if (process_client_data() == false) {
                    continue;
                }
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // disconnect; remove corresponding timer
                util_timer *timer = users_timer[sockfd].timer;
                process_timer(timer, sockfd);
            } else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
                // process signal
                if (process_signal(timeout, stop_server) == false) {
                    LOG_ERROR("%s", "process_signal failure");
                }
            } else if (events[i].events & EPOLLIN) {
                // process data
                process_read(sockfd);
            } else if (events[i].events & EPOLLOUT) {
                process_write(sockfd);
            }
        }

        if (timeout) {
            utils.timer_handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}