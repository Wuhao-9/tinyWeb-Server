#if !defined(UTILITY_H_)
#define UTILITY_H_

class utility {
public:
    static void register_event(int epoll_instance, int fd, bool oneshot, int is_ET);
    static void modify_event(int epoll_instance, int fd, int event, int is_ET);
    static void remove_event(int epoll_instance, int fd);
    static void set_NoBlock(const int fd);
};

#endif // UTILITY_H_
