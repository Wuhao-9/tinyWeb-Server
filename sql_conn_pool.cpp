#include "sql_conn_pool.h"
#include <iostream>

sql_conn_pool* sql_conn_pool::get_SQLpool_Instance() {
    static sql_conn_pool connPool;
    return &connPool;
}

void sql_conn_pool::init_connPool(std::string host, std::string user, std::string pwd, 
            std::string dbName, uint32_t port, std::size_t maxConn, bool close_log) {
    if (maxConn == 0) {
        std::cerr << "[MYSQL] Error: init zero SQL connection!" << std::endl;
    }

    this->host = host;
    this->userName = user;
    this->passWord = pwd;
    this->DBName = dbName;
    this->max_conn_num_ = maxConn;
    this->close_log = close_log; 
    this->Port = port;

    for (int i = 0; i < max_conn_num_; i++) {
        MYSQL* conn = ::mysql_init(nullptr); // init链接环境
        if (conn == nullptr) {
            std::cerr << "[MYSQL] Error: MYSQL init error" << std::endl;
            exit(EXIT_FAILURE);
        }
        
        conn = ::mysql_real_connect(conn, host.c_str(), userName.c_str(), passWord.c_str(), DBName.c_str(), Port, nullptr, 0);
        if (conn == nullptr) {
            std::cerr << "[MYSQL] Error: connect to target DB error" << std::endl;
            exit(EXIT_FAILURE);
        }
        conn_list_.push_back(conn);
        free_conn_num_++;
    }
}


MYSQL* sql_conn_pool::getDBConn() {
    MYSQL* conn = nullptr;
    std::lock_guard<std::mutex> lock(mutex_);

    if (conn_list_.size() == 0) {
        return nullptr;
    }

    conn = conn_list_.front();
    conn_list_.pop_front();
    free_conn_num_--;
    busy_conn_num_++;
    return conn;
}

bool sql_conn_pool::releaseConn(MYSQL* conn) {
    if (conn == nullptr)
        return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    conn_list_.push_back(conn);
    free_conn_num_++;
    busy_conn_num_--;
    return true;
}

void sql_conn_pool::destroyPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn_list_.size() > 0) {
        for (MYSQL* conn : conn_list_) {
            ::mysql_close(conn);
        }
        busy_conn_num_ = 0;
        free_conn_num_ = 0;
        conn_list_.clear();
    }
}

sql_conn_guard::sql_conn_guard(MYSQL** conn, sql_conn_pool* pool) {
    *conn = pool->getDBConn();
    this->cur_SQLconn_ = *conn;
    context_ = pool;
}

sql_conn_guard::~sql_conn_guard() {
    context_->releaseConn(cur_SQLconn_);
}
