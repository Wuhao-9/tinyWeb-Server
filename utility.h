#if !defined(UTILITY_H_)
#define UTILITY_H_

class utility {
public:
    static void register_event(int epoll_instance, int fd, bool oneshot, int);
    static void set_NoBlock(int fd);
};

#endif // UTILITY_H_
