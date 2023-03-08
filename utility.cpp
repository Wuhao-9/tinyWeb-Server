#include "utility.h"
#include "config.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <cassert>
void utility::register_event(int epoll_instance, int fd, bool oneshot, int is_ET) {

    epoll_event target_event = {};
    target_event.data.fd = fd;
    if (is_ET == 1) {
        target_event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    } else {
        target_event.events = EPOLLIN | EPOLLRDHUP;
    }

    if (oneshot) {
        target_event.events |= EPOLLONESHOT;
    }

    auto ret = epoll_ctl(epoll_instance, EPOLL_CTL_ADD, fd, &target_event);
    assert(ret != -1);
}

void utility::set_NoBlock(int fd) {
    int old_flag = ::fcntl(fd, F_GETFL);
    auto ret = ::fcntl(fd, F_SETFL, old_flag | O_NONBLOCK);
    assert(ret != -1);
}
