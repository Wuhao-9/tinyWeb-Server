#if !defined(EPOLL_INFO_HPP_)
#define EPOLL_INFO_HPP_

#include <exception>
#include <iostream>
template <typename T>
class epoll_info {
public:
    static void init_epollFD(const int fd) {
        static bool is_init = false;
        if (!is_init) {
            epollFD = fd;
            is_init = true;
        } else {
            throw std::logic_error("multiple assign val to epollFD");
        }
    }

    static const int get_epollFD() { return epollFD; }
private:
    static int epollFD;
};

template<typename T>
int epoll_info<T>::epollFD;

#endif // EPOLL_INFO_HPP_

