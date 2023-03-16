#include "http_conn.h"
#include "config.h"
#include <cerrno>
#include <iostream>
#include <cstring>

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
    std::cout << rd_buff_.buff_;
    auto cur_http_code = prase_recvData();
    if (cur_http_code == NO_REQUEST) { // 若当前http状态码为NO_REQUEST，则请求不完整
        // 解析
    }
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 11\r\n\r\nHello World";
    auto transfered_bytes = ::send(fd_, response.c_str(), response.size(), 0);
    if (transfered_bytes == -1) {
        std::cerr << "send failure" << std::endl;
        exit(0);
    }
}

http_conn::HTTP_CODE http_conn::prase_recvData() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE result = NO_REQUEST;
    char* text = nullptr;

    while ((cur_check_ == CHECK_CONTENT && line_status == LINE_OK) || (line_status = parse_one_line()) == LINE_OK) {
        text = get_line();
        new_line_idx_ = rd_buff_.check_idx_; // 更新至新一行的起始位置
        std::clog << "[new line]: " << text << std::endl;
        switch (cur_check_) {
        case CHECK_REQUESTLINE:
        {
            result = parse_request_line(text);
            if (result == BAD_REQUEST) 
                return BAD_REQUEST;
            break;
        }
        case CHECK_HEADER:
        {
            result = parse_request_header(text);
            if (result == GET_REQUEST)
                return handle_request();
            break;
        }
        case CHECK_CONTENT:
        {
            result = parse_request_content(text); // 若没有获取到完整content，则返回NO_REQUEST
            if (result == GET_REQUEST) 
                return handle_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::handle_request() {
    return HTTP_CODE::GET_REQUEST;
}

// 获取当前行首指针
char* http_conn::get_line() {
    return rd_buff_.buff_ + new_line_idx_;
}

// 从状态机，用于分析出一行内容
// 返回值为行的读取状态，有LINE_OK, LINE_BAD, LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_one_line() {
    char tmp = 0;
    for (; rd_buff_.check_idx_ < rd_buff_.read_idx_; rd_buff_.check_idx_++) {
        tmp = rd_buff_.buff_[rd_buff_.check_idx_];

        if (tmp == '\r') {
            if ((rd_buff_.check_idx_ + 1) == rd_buff_.read_idx_)
                return LINE_OPEN;
            else if (rd_buff_.buff_[rd_buff_.check_idx_+1] == '\n') {
                rd_buff_.buff_[rd_buff_.check_idx_++] = '\0';
                rd_buff_.buff_[rd_buff_.check_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (tmp == '\n') { // 第一次请求不完整(正好缺'\n'), 第二次分析请求行时则会进入当前if分支
            if (rd_buff_.check_idx_ > 1 && rd_buff_.buff_[rd_buff_.check_idx_-1] == '\r') {
                rd_buff_.buff_[rd_buff_.check_idx_-1] = '\0';
                rd_buff_.buff_[rd_buff_.check_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN; // 如果没找到\r\n则表示当前行不完整
}

// 解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(const std::string& text) {
    auto pos = text.find_first_of(' ');
    if (pos == std::string::npos) {
        return BAD_REQUEST;
    } else {
        auto method = text.substr(0, pos);
        if (method == "GET") {
            requ_info_.method_ = GET;
        } else if (method == "POST") {
            requ_info_.method_ = POST;
        } else { return BAD_REQUEST; }
    }

    std::string url = text.substr(pos+1);
    pos = url.find_first_of(' ');
    if (pos == std::string::npos) {
        return BAD_REQUEST;
    }
    requ_info_.url_ = url.substr(0, pos);
    if (requ_info_.url_.compare(0, 7, "http://") == 0) {
        requ_info_.url_ = requ_info_.url_.substr(requ_info_.url_.substr(8).find_first_of('/'));
    } else if (requ_info_.url_.compare(0, 8, "https://") == 0) {
        requ_info_.url_ = requ_info_.url_.substr(requ_info_.url_.substr(8).find_first_of('/'));
    }

    if (requ_info_.url_.size() == 0 || requ_info_.url_[0] != '/')
        return BAD_REQUEST;

    std::string version = url.substr(pos + 1);
    if (version != "HTTP/1.1") {
        std::cerr << "[Unknow version]: " << version << std::endl;
        return BAD_REQUEST;
    } else {
        requ_info_.version_ = std::move(version);
    }

    if (requ_info_.url_.size() == 1) { // 当url为'/'时，显示judge页面
        requ_info_.url_ += "judge.html";
    }
    cur_check_ = CHECK_HEADER; // 改变主状态机状态
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_request_header(const std::string& text) {
    if (text[0] == '\0') { // 当前行为request-header后紧跟的\r\n
        if (requ_info_.content_len_ != 0) { // 如果有请求体，则继续读取
            cur_check_ = CHECK_CONTENT; // 将主状态机状态更新为检查请求体
            return NO_REQUEST;
        }
        return GET_REQUEST; // 若没有请求体，则直接解析请求、组装响应报文
    } else if (text.compare(0, std::strlen("Connection:"), "Connection:") == 0) {
        text.compare(12, std::string::npos, "keep-alive") == 0 ? requ_info_.linger_ = true : requ_info_.linger_ = false; 
    } else if (text.compare(0, 15, "Content-length:") == 0) {
        auto size = std::stoull(text.substr(16));
        requ_info_.content_len_ = size;
    } else if (text.compare(0, 5, "Host:") == 0) {
        requ_info_.host_ = text.substr(6);
    } else {
        std::clog << "<Unknow header info>: " << text << std::endl;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_request_content(const std::string& text) {
    if (rd_buff_.read_idx_ >= rd_buff_.check_idx_ + requ_info_.content_len_) { // 检查buffer中的请求体内容是否完整
        requ_info_.content_ = text.substr(0, requ_info_.content_len_);
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 新客户连接，重置"实现域"相关成员
void http_conn::init() {
    status_ = -1;
    this->rd_buff_ = {};
    this->wr_buff_ = {};
    this->requ_info_ = {};
    cur_check_ = CHECK_REQUESTLINE;
    new_line_idx_ = 0;
}
