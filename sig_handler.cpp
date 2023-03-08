#include "sig_handler.h"
#include <csignal>
#include <cstring>
#include <cassert>
void signal_handler::sig_callback(int sig) {

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
