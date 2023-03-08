#if !defined(WEB_SERVER_H_)
#define WEB_SERVER_H_

#include <iostream>
template <typename T>
class thread_pool; // forward declaration
class http_conn;   // forward declaration
class utility;     // forward declaration
class web_server {
public:
    web_server();
private:
    const static int TIME_SLOT = 5;
    const static int MAX_CONN_FD = 4000; 
    void start_listen();
    void create_epoll_instance();
    void init_signal();
private:
    std::string resource_root_;
    int pipeFD_[2]; // 两个双向的socket(两个socket既可读又可写),但该程序仅向[1]中写，[0]中读。(不写[0]中写，[1]中读)
    int epollFD_;
    int listenFD_;    

    http_conn* users_;
    // utility* util_;
};

#endif // WEB_SERVER_H_
