#include "sig_handler.h"
#include "web_server.h"
#include <csignal>
#include <cstring>
#include <cassert>
#include <arpa/inet.h>
#include <cerrno>

int* signal_handler::pipeFD = nullptr;

// alarm信号的handler
void signal_handler::sig_callback(int sig) {
    auto save_errno = errno;
    char sig_num = sig;
    assert(nullptr != pipeFD);
    ::send(pipeFD[1], &sig_num, sizeof(sig_num), 0);
    errno = save_errno;
}

void signal_handler::register_sig(int tar_sig, void(*handler)(int), bool restart) {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    auto ret = sigaction(tar_sig, &sa, nullptr);
    assert(ret != -1);
}
