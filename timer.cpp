#include "timer.h"
#include "http_conn.h"
#include "utility.h"
#include <chrono>
#include <functional>
#include <cassert>
#include <iostream>
#include <thread>
#include <unistd.h>

void SortTimerList::timer::handle_timeout_conn(timer* t) {
    utility::remove_event(t->conn_->get_epollFD(), t->conn_->getFD());
    auto ret = close(t->conn_->getFD());
    if (ret != 0) {
        return;
    }
    http_conn::user_count--;
}

SortTimerList::timer::timer(http_conn* conn, const std::time_t expire, std::function<void(timer*)> cb, timer* next)
    : conn_(conn)
    , expire_time_(expire)
    , cb_(cb)
    , next_(next) {}

SortTimerList::timer::timer() 
    : conn_(nullptr)
    , expire_time_(0)
    , cb_(nullptr)
    , next_(nullptr) {}

SortTimerList::SortTimerList() : dummyHead_(new timer(nullptr, -1, nullptr, nullptr)) {}

void SortTimerList::insert_timer(timer* const t) {
    std::lock_guard<std::mutex> lock(mutex_);
    timer* cur = dummyHead_; 
    while (cur->next_ && cur->next_->expire_time_ < t->expire_time_) {
        cur = cur->next_;
    }
    timer* tmp = cur->next_;
    cur->next_ = t;
    t->next_ = tmp;
}

bool SortTimerList::delete_timer(timer* const t) {
    std::lock_guard<std::mutex> lock(mutex_);
    timer* cur = dummyHead_;
    while (cur->next_ != nullptr) {
        if (cur->next_ == t) {
            timer* tmp = cur->next_;
            cur->next_ = tmp->next_;
            tmp->cb_(tmp);
            // delete tmp;
            return true;
        } else {
            cur = cur->next_;
        }
    }
    return false;
}

void SortTimerList::tick() {
    using namespace std::chrono;

    std::lock_guard<std::mutex> lock(mutex_);
    timer* cur = dummyHead_;
    auto now = system_clock::to_time_t(system_clock::now()); // 获取当前时间
    while (cur->next_ != nullptr && cur->next_->expire_time_ <= now) {
        timer* tmp = cur->next_;
        cur->next_ = tmp->next_;
        tmp->cb_(tmp);
        // delete tmp;
    }
}

void SortTimerList::update_timer(timer* const t, const time_t new_expire) {
    std::unique_lock<std::mutex> lock(mutex_);
    timer* cur = dummyHead_;
    while (cur->next_ != nullptr && cur->next_ != t) {
        cur = cur->next_;
    }
    if (cur) {
        // 从原位置取出定时器
        cur->next_ = t->next_;
        t->expire_time_ = new_expire;
        t->next_ = nullptr; // 防止出现环路
        // lock.unlock();
        // insert_timer(t);

        // 搜索合适位置插入定时器
        timer* f = dummyHead_;
        while (f->next_ != nullptr && f->next_->expire_time_ < t->expire_time_) {
            f = f->next_;
        }
        timer* tmp  = f->next_;
        f->next_ = t;
        t->next_ = tmp;
    } else {
        // std::cerr << "no target timer in sortList" << std::endl;
        assert(false);
    }
}