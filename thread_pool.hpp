#if !defined(THREAD_POOL_HPP_)
#define THREAD_POOL_HPP_

#include "config.h"
#include <pthread.h>
#include <iostream>
#include <list>
#include <mutex>
#include <atomic>
#include <condition_variable>
template <typename T>
class thread_pool {
    thread_pool();
    ~thread_pool();
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
    using ser_config;
    try {
        threads_array_ = new ::pthread_t[thread_num]; 
    } catch (std::bad_alloc& err) {
        std::cerr << "threads memory allocate failed!" << std::endl;
        throw;
    }

    for (int i = 0; i < thread_num; i++) {
        if (::pthread_create(threads_array_+i, nullptr, work_thread, this) == -1) {
            this->stop_worker();
            throw std::runtime_error();
        } else {
            auto ret = ::pthread_detach(threads_array_[i]);
            if (ret != 0) {
                this->stop_worker();
                throw std::runtime_error();
            }
        }
    }
}

template<typename T>
thread_pool<T>::~thread_pool() {
    this->stop_worker();
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
    using model = ser_config::net_model;
    while (true) {
        std::unique_lock<std::mutex> locker(mutex_);
        while (request_queue_.empty() && !stop_) {
            cv_.wait();
        }
        if (stop_) {
            break;
        }
        T* task = request_queue_.front();
        request_queue_.pop_front();
        locker.unlock();
        if (!task) continue;
        if (model == 0) { // proactor

        } else { // reactor

        }
    }
 
}

#endif // THREAD_POOL_HPP_
