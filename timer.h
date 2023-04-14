#if !defined(TIMER_H_)
#define TIMER_H_

#include <chrono>
#include <functional>
#include <mutex>

class http_conn;
// 升序链表
class SortTimerList {
public:
    
    class timer {
    public:
        static void handle_timeout_conn(timer* t);

    public:
        explicit timer(http_conn* conn, const std::time_t expire, std::function<void(timer*)> cb, timer* next = nullptr);

        explicit timer();
            
    public:
        std::function<void(timer*)> cb_;
        http_conn* conn_;
        std::time_t expire_time_;
        timer* next_;
    };

    SortTimerList();
    SortTimerList(const SortTimerList&) = delete;
    void insert_timer(timer* const t);
    bool delete_timer(timer* const t);
    void update_timer(timer* const t, const time_t new_expire);
    void tick();
private:
    timer* const dummyHead_;
    std::mutex mutex_;
};

#endif // TIMER_H_
