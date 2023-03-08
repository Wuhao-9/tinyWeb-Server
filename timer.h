#if !defined(TIMER_H_)
#define TIMER_H_

#include <chrono>

class timer_list;
class timer;

struct client_data {
    int fd;
    timer* user_timer;    
};

class timer {
    friend timer_list;
public:
    timer(std::time_t expire)
        : expire_time_(expire)
        , prev_(nullptr)
        , next_(nullptr) {}
    static void timeout_cb(client_data*);
    static const int epollFD;
private:
    client_data* client_data_;
    std::time_t expire_time_;
    timer* prev_;
    timer* next_;
};

// ordered-list
class timer_list {
public:
    void add_timer(timer* new_timer);
    void adjust_timer(timer* target);
    void remove_timer(timer* target);
    void check_timeout();
private:
    void add_timer(timer* new_timer, timer* list_head);
private:
    timer* head_ = nullptr;
    timer* tail_ = nullptr;
};

#endif // TIMER_H_
