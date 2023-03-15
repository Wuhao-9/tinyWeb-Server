#if !defined(WEB_SERVER_H_)
#define WEB_SERVER_H_

#include <iostream>
template <typename T>
class thread_pool; // forward declaration

class http_conn;   // forward declaration
class client_data; // forward declaration
class timer_list;  // forward declaration
class utility;     // forward declaration
class web_server {
public:
    const static time_t TIME_SLOT = 5;
    web_server();
    void start() { eventLoop(); }
private:
    const static int MAX_CONN_FD = 4000; 
    const static int MAX_EVENTS = 500;
    static void show_error(int fd, const std::string& info);
    void start_listen();
    void create_epoll_instance();
    void init_signal();
    void create_thread_pool();
    void handle_newCoon();
    void create_timer(int fd);
    void handle_client_exit(int client_fd);
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
    client_data* timers_;
    timer_list* timer_list_;
    thread_pool<http_conn>* pool_;
};

#endif // WEB_SERVER_H_
