#include "http_conn.h"
#include "config.h"
#include <cerrno>
#include <iostream>

std::size_t http_conn::user_count = 0;

bool http_conn::recv_data() {
    if (rd_buff_.read_idx_ >= RD_BUFFER_SIZE) {
        return false;
    }

    std::size_t transfered_bytes = 0;
    if (ser_config::conn_trigger == 0) { // LT
        transfered_bytes = ::recv(fd_, (rd_buff_.buff_ + rd_buff_.read_idx_), (RD_BUFFER_SIZE - rd_buff_.read_idx_), 0);
        if (transfered_bytes == 0 || transfered_bytes == -1) { // 由于是ET模式，故只读一次，所以不会出现EAGAIN
            return false;
        } else {
            rd_buff_.read_idx_ += transfered_bytes;
            return true;
        }
    } else { // ET
        while (true) {
            transfered_bytes = ::recv(fd_, (rd_buff_.buff_ + rd_buff_.read_idx_), (RD_BUFFER_SIZE - rd_buff_.read_idx_), 0);
            if (transfered_bytes == 0) {
                return false;
            } else if (transfered_bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                return false;
            }
            rd_buff_.read_idx_ += transfered_bytes;
        }
        return true;
    }
}

void http_conn::init(const int fd, const sockaddr_in& addr) {
    ++user_count; // 更改当前用户数量

    fd_ = fd;
    addr_ = addr;

    init();
}

void http_conn::process() {
    std::cout << "I`s client request:" << std::endl;
    std::cout << rd_buff_.buff_ << std::endl;
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 11\r\n\r\nHello World";
    auto transfered_bytes = ::send(fd_, response.c_str(), response.size(), 0);
    if (transfered_bytes == -1) {
        std::cerr << "send failure" << std::endl;
        exit(0);
    }
}

void http_conn::init() {
    status_ = -1;
    this->rd_buff_ = {};
    this->wr_buff_ = {};
}
