#include "config.h"
#include "web_server.h"
#include "sig_handler.h"
#include "timer.h"
#include "utility.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <csignal>
#include <sys/epoll.h>

web_server::web_server()
    // : util_(new utility)
{
    try {
        users_ = new http_conn[MAX_CONN_FD];
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
    init_signal();
}

void web_server::start_listen() {
    listenFD_ = ::socket(PF_INET, SOCK_STREAM, 0);
    if (listenFD_ == -1) {
        std::cerr << "failed to create listen fd!" << std::endl;
        ::exit(EXIT_FAILURE);
    }

    sockaddr_in server_addr;
    ::bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_addr.s_addr = ::htonl(INADDR_ANY);
    server_addr.sin_port = htons(ser_config::port);

    int optval = 1;
    auto ret = ::setsockopt(listenFD_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
    if (ret == -1) {
        std::cerr << "failed to set reuse ADDR" << std::endl;
    }

    ret = bind(listenFD_, (sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        std::cerr << "failed to bind" << std::endl;
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
    auto ret = socketpair(AF_INET, SOCK_STREAM, 0, pipeFD_);
    assert(ret != -1);

    utility::set_NoBlock(pipeFD_[1]);
    utility::register_event(epollFD_, pipeFD_[0], false, 0);
    signal_handler::register_sig(SIGPIPE, SIG_DFL, true); // 若管道读端关闭，再向管道写数据，则会产生SIGPIPE信号, 默认abort
    signal_handler::register_sig(SIGALRM, signal_handler::sig_callback, false);
    signal_handler::register_sig(SIGTERM, signal_handler::sig_callback, false);
    
    ::alarm(TIME_SLOT); // 启动定时器
}
