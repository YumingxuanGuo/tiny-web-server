#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

// http responses
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker lock;
map<string, string> users;

// set the file descriptor to non-blocking
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// register in epoll
void addfd(int epollfd, int fd, bool one_shot, int trigger_mode) {
    epoll_event event;
    event.data.fd = fd;

    if (trigger_mode == 1) {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    } else {
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// remove file descriptor in epoll
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// reset event to EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int trigger_mode) {
    epoll_event event;
    event.data.fd = fd;

    if (trigger_mode == 1) {
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    } else {
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::user_count = 0;
int http_conn::epollfd = -1;

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int trigger_mode,
                     int close_log, string user, string password, string sqlname) {
    sockfd_ = sockfd;
    address_ = addr;

    addfd(epollfd_, sockfd_, true, trigger_mode);
    user_count_++;

    doc_root_ = root;
    trigger_mode_ = trigger_mode;
    close_log_ = close_log;

    strcpy(sql_user_, user.c_str());
    strcpy(sql_password_, password.c_str());
    strcpy(sql_name_, sqlname.c_str());

    init();
}

void http_conn::init()
{
    mysql_ = NULL;
    bytes_to_send_ = 0;
    bytes_have_send_ = 0;
    check_state_ = CHECK_STATE_REQUESTLINE;
    linger_ = false;
    method_ = GET;
    url_ = 0;
    version_ = 0;
    content_length_ = 0;
    host_ = 0;
    start_line_ = 0;
    checked_idx_ = 0;
    read_idx_ = 0;
    write_idx_ = 0;
    cgi_ = 0;
    state_ = 0;
    timer_flag_ = 0;
    improv_ = 0;

    memset(read_buf_, '\0', READ_BUFFER_SIZE);
    memset(write_buf_, '\0', WRITE_BUFFER_SIZE);
    memset(real_file_, '\0', FILENAME_LEN);
}