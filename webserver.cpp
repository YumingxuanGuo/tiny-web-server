#include "webserver.h"

WebServer::WebServer() {
    // http_conn instances
    users = new http_conn[MAX_FD];

    // root file path
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char*) malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcpy(m_root, root);

    // timer
    users_timer = new client_data[MAX_FD];
}