#include "config.h"
#include "http_conn.h"
#include "web_server.h"
#include "sig_handler.h"
#include "thread_pool.hpp"
#include "timer.h"
#include "utility.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <csignal>
#include <sys/epoll.h>

web_server::web_server()
    : timer_list_(new timer_list)
    // : util_(new utility)
{
    try {
        users_ = new http_conn[MAX_CONN_FD];
        timers_ = new client_data[MAX_CONN_FD];
    } catch (std::bad_alloc& err) {
        std::cerr << "users-array memory allocate failed" << std::endl;
        throw;
    }
    char work_dir[100];
    auto ret = ::getcwd(work_dir, sizeof(work_dir));
    if (ret == nullptr) {
        std::cerr << "getcwd() failuer" << std::endl;
        ::exit(EXIT_FAILURE);
    }
    resource_root_ += std::string(work_dir, std::strlen(work_dir));

    create_epoll_instance();
    start_listen();
    create_thread_pool();
    init_signal();
}

void web_server::show_error(int fd, const std::string& info) {
    send(fd, info.c_str(), info.size(), 0);
    close(fd);
}

void web_server::start_listen() {
    listenFD_ = ::socket(PF_INET, SOCK_STREAM, 0);
    if (listenFD_ == -1) {
        std::cerr << "failed to create listen fd!" << std::endl;
        ::exit(EXIT_FAILURE);
    }

    sockaddr_in server_addr;
    ::bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(ser_config::port);
    server_addr.sin_family = AF_INET;

    int optval = 1;
    auto ret = ::setsockopt(listenFD_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
    if (ret == -1) {
        std::cerr << "failed to set reuse ADDR" << std::endl;
    }

    ret = bind(listenFD_, (sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        std::cerr << "failed to bind" << std::endl;
        perror("bind");
        ::exit(EXIT_FAILURE);
    }
    ret = listen(listenFD_, 128);
    if (ret == -1) {
        std::cerr << "failed to open listen" << std::endl;
        ::exit(EXIT_FAILURE);
    }
    utility::register_event(epollFD_, listenFD_, false, ser_config::listen_trigger);
}

void web_server::create_epoll_instance() {
    epollFD_ = epoll_create(1);
    if (epollFD_ == -1) {
        std::cerr << "failed to create epoll instance" << std::endl;
        exit(EXIT_FAILURE);
    }
    timer::epollFD = epollFD_;
}

void web_server::init_signal() {
    auto ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipeFD_);
    if (ret == -1) {
        perror("sockpair");
        abort();
    }
    assert(ret != -1);

    signal_handler::pipeFD = pipeFD_;
    utility::set_NoBlock(pipeFD_[1]);
    utility::register_event(epollFD_, pipeFD_[0], false, 0);
    signal_handler::register_sig(SIGPIPE, SIG_DFL, true); // 若管道读端关闭，再向管道写数据，则会产生SIGPIPE信号, 默认abort
    signal_handler::register_sig(SIGALRM, signal_handler::sig_callback, false);
    signal_handler::register_sig(SIGTERM, signal_handler::sig_callback, false);
    
    ::alarm(TIME_SLOT); // 启动定时器
}

void web_server::create_thread_pool() {
    try {
        pool_ = new thread_pool<http_conn>;
    } catch (std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        throw;
    }
}

void web_server::handle_newCoon() {
    ::sockaddr_in client_addr;
    ::socklen_t addr_len = sizeof(client_addr);
    if (ser_config::listen_trigger == 0) {
        int client_fd = ::accept(listenFD_, (sockaddr*)&client_addr, &addr_len);
        utility::set_NoBlock(client_fd); // 设置nonblocking；在linux下，也可以将listen-fd设为非阻塞，其accept的客户端fd默认为非阻塞状态
        if (client_fd == -1) {
            std::cerr << "accept new client failure!" << std::endl;
            return;
        } else if (http_conn::user_count >= MAX_CONN_FD) {
            show_error(client_fd, "Internal server busy");
            std::cerr << "Internal server busy!" << std::endl;
            return;
        }
        
        users_[client_fd].init(client_fd, client_addr); // 初始化该客户端对应的http_conn对象
        if (ser_config::conn_trigger == 0) { // 注册epoll事件
            utility::register_event(epollFD_, client_fd, true, 0);
        } else {
            utility::register_event(epollFD_, client_fd, true, 1);
        }
        create_timer(client_fd); // 初始化新连接的定时器
    }

}
void web_server::create_timer(int fd) {
    timers_[fd].fd = fd;
    timer* tmp = new (std::nothrow) timer(TIME_SLOT * 3 + std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    if (tmp == nullptr) {
        std::cerr << "create timer failure!" << std::endl;
        show_error(fd, "internal server occurred error");
        return;
    }
    timers_[fd].user_timer = tmp;
    tmp->set_clientData(timers_ + fd);
    timer_list_->add_timer(tmp);
}

void web_server::handle_client_exit(int client_fd) {
    timer::timeout_cb(&timers_[client_fd]);
    timer* cur_user_timer = timers_[client_fd].user_timer;
    timer_list_->remove_timer(cur_user_timer);
    std::clog << "In [web_server::handle_client_exit] close fd: " << client_fd << std::endl;
}

void web_server::handle_recv(int fd) {
    if (ser_config::net_model == 1) { // reactor
        timer* cur_timer = timers_[fd].user_timer;
        assert(cur_timer != nullptr);
        timer_list_->adjust_timer(cur_timer); // 更新计数器时间
        pool_->enqueue(users_ + fd, 0);
    } else { // proactor
        // .....
    }
}

bool web_server::handle_signal(bool* const timeout, bool* const terminated) {
    char sig_buffer[500] {};
    auto ret = recv(pipeFD_[0], sig_buffer, sizeof(sig_buffer), 0);
    if (ret == -1) {
        return false;
    } else if (ret == 0) {
        return false;
    } else {
        for (int i = 0; i < ret; i++) {
            switch (sig_buffer[i])
            {
            case SIGALRM:
                *timeout = true;
                break;
            case SIGTERM:
                *terminated = true;
                break;
            default:
                std::cerr <<  "unknow signal: " << sig_buffer[i] << std::endl;
                break;
            }
        }
    }
    return true;
}

void web_server::handle_write(int fd) {
    if (ser_config::net_model == 1) { // reactor
        timer* cur_timer = timers_[fd].user_timer;
        assert(cur_timer != nullptr);
        timer_list_->adjust_timer(cur_timer); // 更新计数器时间
        pool_->enqueue(users_ + fd, 1);

    } else { // proactor
        // .....
    }
}

void web_server::eventLoop() {
    bool timeout = false;
    bool terminated = false;

    ::epoll_event ready_event[MAX_EVENTS];
    while (!terminated) {
        int num = ::epoll_wait(epollFD_, ready_event, MAX_EVENTS, -1);
        if (num == -1 && (errno != EINTR)) {
            std::cerr << "epoll_wait failure!" << std::endl;
            break;
        }
        
        for (int i = 0; i < num; i++) {
            int sock_fd = ready_event[i].data.fd;
            if (sock_fd == listenFD_) {
                handle_newCoon();
            } else if (ready_event[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                handle_client_exit(sock_fd);
            } else if (sock_fd == pipeFD_[0] && ready_event[i].events & EPOLLIN) {
                // 每隔5秒会产生alarm信号，其handler会向管道写入SIGALRM
                // 主线监听管道读端，若读端有数据且为SIGALARM,则将timeout标志置为true
                // 当timeout为true时，定时器链表会遍历其elem，若有定时器超时，则将会清除该用户的相关资源
                bool flag = handle_signal(&timeout, &terminated);
                if (!flag) {
                    std::cerr << "read pipe failure" << std::endl;
                }
            } else if (ready_event[i].events & EPOLLIN) {
                handle_recv(sock_fd);
            } else if (ready_event[i].events & EPOLLOUT) {
                handle_write(sock_fd);
            }
        }

        if (timeout) { // 等到所有事件都被dispatch过后，再进行清理失效的定时器(如果存在的话)
            timer_list_->check_timeout();
            alarm(TIME_SLOT);
            timeout = false;
        }
    }
}