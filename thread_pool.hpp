#if !defined(THREAD_POOL_HPP_)
#define THREAD_POOL_HPP_

#include "http_conn.h"
#include "config.h"
#include <pthread.h>
#include <iostream>
#include <list>
#include <mutex>
#include <atomic>
#include <cassert>
#include <condition_variable>
template <typename T>
class thread_pool {
public:
    thread_pool();
    ~thread_pool();
    void enqueue(http_conn* client, char which);
private:
    static void* work_thread(void*);
    void stop_worker(); 
    void run();
private:
    const static int MAX_REQUESTS = 3000;
    pthread_t* threads_array_;
    std::list<T*> request_queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
    // sql
};

template<typename T>
thread_pool<T>::thread_pool()
    : threads_array_(nullptr)
    , mutex_()
    , cv_()
    , stop_(false)
{
    try {
        threads_array_ = new ::pthread_t[ser_config::thread_num]; 
    } catch (std::bad_alloc& err) {
        std::cerr << "threads memory allocate failed!" << std::endl;
        throw;
    }

    for (int i = 0; i < ser_config::thread_num; i++) {
        if (::pthread_create(threads_array_+i, nullptr, work_thread, this) == -1) {
            this->stop_worker();
            throw std::runtime_error("create thread failure");
        } else {
            auto ret = ::pthread_detach(threads_array_[i]);
            if (ret != 0) {
                this->stop_worker();
                throw std::runtime_error("detach thread failure");
            }
        }
    }
}

template<typename T>
thread_pool<T>::~thread_pool() {
    this->stop_worker();
}

template<typename T>
void thread_pool<T>::enqueue(http_conn* request, char which) {
    {
        std::lock_guard<std::mutex> locker(mutex_);
        if (request_queue_.size() >= MAX_REQUESTS) {
            std::clog << "request queue is full" << std::endl;
            return;
        }
        request->setStatus(which);
        request_queue_.emplace_back(request);
    }
    cv_.notify_one();
}

template<typename T>
void* thread_pool<T>::work_thread(void* arg) {
    thread_pool* worker = static_cast<thread_pool*>(arg);
    worker->run();
    return nullptr;
}

template<typename T>
inline void thread_pool<T>::stop_worker() {
    stop_ = true;
    cv_.notify_all();
    delete[] threads_array_;
}

template<typename T>
void thread_pool<T>::run() {
    while (true) {
        std::unique_lock<std::mutex> locker(mutex_);
        while (request_queue_.empty() && !stop_) {
            cv_.wait(locker);
        }
        if (stop_) {
            break;
        }
        T* request = request_queue_.front();
        request_queue_.pop_front();
        locker.unlock();
        if (!request) continue;
        if (ser_config::net_model == 1) { // reactor
            if (request->getStatus() == 0) { // read
                auto ret = request->recv_data();
                if (ret == true) {
                    // 读成功，开展业务逻辑
                    request->process();
                } else {
                    // 对方断开连接\recv失败，等待定时器超时清理对应客户端即可
                }
            } else if (request->getStatus() == 1) { // write
                request->send();
            } else {
                assert(false); // 即不是读也不是写，error！
            }
        } else { // proactor 

        }
    }
 
}

#endif // THREAD_POOL_HPP_
