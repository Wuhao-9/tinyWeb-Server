#include "config.h"
#include "web_server.h"
#include "http_conn.h"
#include <signal.h>
int main(int argc, char* argv[]) {
    ser_config::parse_arg(argc, argv);
    signal(SIGSEGV, [](int sig) {std::cout << "当前user-count: " << http_conn::user_count << std::endl; signal(SIGSEGV, SIG_DFL); });
    web_server server("root", "123", "WebUsersInfo", ser_config::sql_num);
    server.start();
}