#if !defined(WEB_SERVER_H_)
#define WEB_SERVER_H_

#include "timer.h"
#include "sql_conn_pool.h"
#include <iostream>

template <typename T>
class thread_pool;          // forward declaration
class http_conn;            // forward declaration

class web_server {
public:
    const static time_t TIME_SLOT = 5;
    web_server(const std::string& db_user, const std::string& db_PWD, const std::string& db_name, const std::size_t conn_amount);
    web_server(const web_server&) = delete;
    void start() { eventLoop(); }
    void handle_client_exit(int client_fd);
private:
    const static int MAX_CONN_FD = 4000; 
    const static int MAX_EVENTS = 500;
    static void show_error(int fd, const std::string& info);
    void start_listen();
    void create_epoll_instance();
    void init_signal();
    void create_thread_pool();
    void create_SQLConn_pool();
    void handle_newCoon();
    void create_timer(http_conn* conn);
    void handle_recv(int fd);
    void handle_write(int fd);
    bool handle_signal(bool* const timeout, bool* const terminated);
    void eventLoop();
private:
    std::string resource_root_;
    int pipeFD_[2]; // 两个双向的socket(两个socket既可读又可写),但该程序仅向[1]中写，[0]中读。(不写[0]中写，[1]中读)
    int epollFD_;
    int listenFD_;    
    
    http_conn* users_;
    thread_pool<http_conn>* pool_;
    SortTimerList::timer* timers_;
    SortTimerList* timerList_;

    // about DataBase
    sql_conn_pool* db_conn_pool_;
    std::string sql_user_;      // 登陆数据库用户名
    std::string sqlPWD_;        // 登陆数据库密码
    std::string DB_name_;       // 使用数据库名
    std::size_t conn_amount_;   // 连接池连接数量
};

#endif // WEB_SERVER_H_
