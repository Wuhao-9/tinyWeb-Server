#include "config.h"
#include <unistd.h>
#include <thread>
#include <string>

int ser_config::port = 1993;
//触发组合模式,默认listenfd LT + connfd LT
int ser_config::trigger_mode = 0;
int ser_config::listen_trigger = 0;
int ser_config::conn_trigger = 0;
//日志写入方式，默认同步
int ser_config::log_write = 0;
//优雅关闭链接，默认不使用
int ser_config::opt_linger = 0;
int ser_config::sql_num = 8;
int ser_config::thread_num = std::thread::hardware_concurrency();
int ser_config::close_log = 0;
//并发模型,默认是Reactor
int ser_config::net_model = 1; 

void ser_config::parse_arg(int argc, char* argv[]) {
    int opt;
    const char* opt_str = "p:l:m:o:s:t:c:a:";
    while ((opt = ::getopt(argc, argv, opt_str)) != -1) {
        switch (opt)
        {
        case 'p':
        {
            port = std::stoi(::optarg);
            break;
        }
        case 'l':
        {
            log_write = std::stoi(::optarg);
            break;
        }
        case 'm':
        {
            trigger_mode = std::stoi(::optarg);
            break;
        }
        case 'o':
        {
            opt_linger = std::stoi(::optarg);
        }
        case 's':
        {
            sql_num = std::stoi(::optarg);
        }
        case 't':
        {
            thread_num = std::stoi(::optarg);
        }
        case 'c':
        {
            close_log = std::stoi(::optarg);
        }
        case 'a':
        {
            net_model = std::stoi(::optarg);
        }
        }
    }
    if (trigger_mode == 1) {
        listen_trigger = 1;
        conn_trigger = 0;
    } else if (trigger_mode == 2) {
        listen_trigger = 0;
        conn_trigger = 1;
    } else if (trigger_mode == 3) {
        listen_trigger = conn_trigger = 1;
    } else { // Default
        listen_trigger = conn_trigger = 0;
    }
}