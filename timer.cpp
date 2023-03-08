#include "timer.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>

void timer_list::add_timer(timer* new_timer) {
    if (!new_timer) {
        std::cerr << "null timer" << std::endl;
    }

    if (!head_) { // 没有head，做头结点
        head_ = tail_ = new_timer;
        return;
    }

    if (new_timer->expire_time_ < head_->expire_time_) { // 比头结点小，做头结点
        new_timer->next_ = head_;
        head_->prev_ = new_timer;
        head_ = new_timer;
        return;
    }

    add_timer(new_timer, head_); // 当前定时器不能做头结点
}

void timer_list::adjust_timer(timer* target) {
    if (!target) {
        std::cerr << "adjust null timer" << std::endl; 
        return;
    }

    timer* tmp = target->next_;
    // 如果已经是链表中的尾节点，或定时器更新后的时间依然小于其next，则无需移动，直接return
    if (!tmp || (target->expire_time_ < tmp->expire_time_)) {
        return;
    }

    // 如果更新的是链表头节点，且需要移动。则让出头结点位置
    if (target == head_) {
        head_ = target->next_;
        head_->prev_ = nullptr;
        target->next_ = nullptr;
        target->prev_ = nullptr;
        add_timer(target, head_);
    } else {
        target->prev_->next_ = target->next_;
        target->next_->prev_ = target->prev_;
        auto bound = target->next_; // 因为target->next的超时时间小于target的超时时间
        target->prev_ = nullptr;
        target->next_ = nullptr;
        add_timer(target, bound); // 故从bound开始向后判断其插入的位置即可
    }
}

void timer_list::remove_timer(timer* target) {
    if (!target) {
        std::cerr << "remove null timer" << std::endl; 
        return;
    }
    if ((target == head_) && (target == tail_)) {
        delete target;
        head_ = nullptr;
        tail_ = nullptr;
        return;
    }
    if (target == head_) {
        head_ = head_->next_;
        head_->prev_ = nullptr;
        delete target;
        return;
    } else if (target == tail_) {
        tail_ = tail_->prev_;
        tail_->next_ = nullptr;
        delete target;
        return;
    }

    target->prev_->next_ = target->next_;
    target->next_->prev_ = target->prev_;
    delete target;
}

void timer_list::check_timeout() {
    if (!head_) { return; } // 当前没有定时器(即没有用户连接)
    
    time_t cur = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); 
    timer* tmp = head_;
    while (tmp) {
        if (cur < tmp->expire_time_) {
            break;
        }
        head_ = tmp->next_;
        timer::timeout_cb(tmp->client_data_);
        if (head_) {
            head_->prev_ = nullptr;
        }
        delete tmp;
        tmp = head_;
    }
}

void timer_list::add_timer(timer* new_timer, timer* list_head) {
    timer* tmp = list_head->next_;
    while (tmp) {
        if (new_timer->expire_time_ < tmp->expire_time_) {
            list_head->next_ = new_timer;
            new_timer->next_ = tmp;
            new_timer->prev_ = list_head;
            tmp->prev_ = new_timer;
            return;
        }
        list_head = tmp;
        tmp = tmp->next_;
    }
    if (!tmp) {
        list_head->next_ = new_timer;
        new_timer->prev_ = list_head;
        tail_ = new_timer;
    }
}

const int timer::epollFD = -1;

void timer::timeout_cb(client_data* data) {
    if (data == nullptr) {
        std::cerr << "timeout callback func get a null parameter!" << std::endl;
    }
    epoll_ctl(timer::epollFD, EPOLL_CTL_DEL, data->fd, nullptr);
    close(data->fd);
    http_conn::user_count--;
}
