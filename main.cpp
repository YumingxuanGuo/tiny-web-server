#include "config.h"

int main(int argc, char *argv[]) {
    string user = "root";
    string password = "root";
    string database_name = "mydb";

    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    server.init(config.PORT, user, password, database_name, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    

    server.log_write();

    server.sql_pool();

    server.thread_pool();

    server.trig_mode();

    server.eventListen();

    server.eventLoop();

    return 0;
}