#include "web_server.h"
#include "http_conn.h"
#include "utility.h"
#include "config.h"
#include <sys/epoll.h>
#include <sys/mman.h> // for mmap
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <cassert>
#include <cerrno>
#include <iostream>
#include <cstring>

std::atomic<std::size_t> http_conn::user_count(0);

// 资源文件路径
static const char* register_page = "/register.html";
static const char* log_page = "/log.html";
static const char* picture_page = "/picture.html";
static const char* video_page = "/video.html";
static const char* follow_page = "/fans.html";

//定义http响应的一些状态信息
static const char *ok_200_title = "OK";
static const char *error_400_title = "Bad Request";
static const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
static const char *error_403_title = "Forbidden";
static const char *error_403_form = "You do not have permission to get file form this server.\n";
static const char *error_404_title = "Not Found";
static const char *error_404_form = "The requested file was not found on this server.\n";
static const char *error_500_title = "Internal Error";
static const char *error_500_form = "There was an unusual problem serving the request file.\n";

http_conn::http_conn(web_server& context)
    : context_(context) {}

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

void http_conn::init(const int fd, const sockaddr_in& addr, const std::string& root) {
    ++user_count; // 更改当前用户数量
    root_ = root;
    fd_ = fd;
    addr_ = addr;

    init();
}

void http_conn::process() {
    auto cur_http_code = prase_recvData();
    if (cur_http_code == NO_REQUEST) { // 若当前http状态码为NO_REQUEST，则请求不完整
        // 继续等待recv事件
        utility::modify_event(http_conn::get_epollFD(), fd_, EPOLLIN, ser_config::conn_trigger);
    }
    build_send_data(cur_http_code);
    // 尝试直接发送：若缓冲区满，则注册写事件，等待可写事件通知再发送
    if (!try_send()) {
        // 在epoll中注册写事件
        utility::modify_event(http_conn::get_epollFD(), fd_, EPOLLOUT, ser_config::conn_trigger);
    } else {
        // assert(::munmap(file_addr_, file_stat_.st_size) == 0);
        if (!requ_info_.linger_) { // 若为短连接，则关闭连接
        // TODO：此处只是简单的关闭了文件描述符等相关操作
        //       并没有移除定时器，释放定时器相关资源
        //        应该重构统计客户与定时器相关代码，尽量做到低耦合
        context_.handle_client_exit(fd_);
        } else { // 负责继续等待读事件就绪
            utility::modify_event(http_conn::get_epollFD(), fd_, EPOLLIN, ser_config::conn_trigger);    
            init();
        }
    }
}

bool http_conn::try_send() {
    auto ret = writev(fd_, iov_, iov_count_);
    if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { // 发送缓冲区已满，注册epoll写事件
            return false;
        } else {
            std::cerr << "Unknow net error occurred: " << std::strerror(errno) << std::endl;
            return false;
        }
    } else if (ret == send_total_bytes_) {
        return true;
    } else {
        assert(false); // 一次性没有写完全部数据，暂时触发assert（后续做相应优化）
    }
}

void http_conn::assemble_state_line(int code, const char* tittle) {
    resp_ << "HTTP/1.1" << " " << code << " " << tittle << "\r\n";
}

void http_conn::assemble_reps_header(const std::size_t content_len) {
    // Content-Type
    // resp_ << "Content-Type: " << "text/html" << "\r\n";
    // Content-Length
    resp_ << "Content-Length: " << content_len << "\r\n";
    // Connection
    resp_ << "Connection: " << (requ_info_.linger_ == true ? "keep-alive" : "close") << "\r\n";
    // blank-line
    resp_ << "\r\n";
}

void http_conn::assemble_content(const char* content) {
    resp_ << content;
}

void http_conn::build_response(int code, const char* tittle, const std::size_t content_len, const char* content) {
    assemble_state_line(code, tittle);
    assemble_reps_header(content_len);
    assemble_content(content);
}

void http_conn::build_response(int code, const char* tittle) { // 构建承载着template文件的响应报文
    assemble_state_line(200, ok_200_title);
    if (file_stat_.st_size != 0) {
        assemble_reps_header(file_stat_.st_size);
        message_ = resp_.str(); // 获取响应状态行和响应头部
        resp_.str(""); // clear buffer
        iov_[0].iov_base = const_cast<char*>(message_.c_str()); // 获取ostringstream中的数据
        iov_[0].iov_len = message_.size();
        iov_[1].iov_base = file_addr_;
        iov_[1].iov_len = file_stat_.st_size;
        iov_count_ = 2;
        send_total_bytes_ = iov_[0].iov_len + iov_[1].iov_len;
    } else {
        const std::string empty_page = "<html><body></body></html>"; // 请求的目标文件没有数据，返回空页面
        assemble_reps_header(empty_page.size());
        assemble_content(empty_page.c_str());
    }
    return;
}

void http_conn::build_send_data(const HTTP_CODE requ_status) {
    switch (requ_status) {
    case INTERNAL_ERROR: {
        build_response(500, error_500_title, std::strlen(error_500_form), error_500_form);
        break;
    }
    case BAD_REQUEST: {
        build_response(400, error_400_title, std::strlen(error_400_form), error_400_form);
        break;
    }
    case FORBIDDEN_REQUEST: {
        build_response(403, error_403_title, std::strlen(error_403_form), error_403_form);
        break;
    }
    case NO_REQUEST: {
        build_response(404, error_404_title, std::strlen(error_404_form), error_404_form);
        break;
    }
    case FILE_REQUEST: {
        build_response(200, ok_200_title);
        return;
    }
    default: {
        std::cerr << "[Unknow HTTP-CODE: " << requ_status << "]" << std::endl;
        return;
    }
    };
    message_ = resp_.str();
    iov_[0].iov_base = const_cast<char*>(message_.c_str());
    iov_[0].iov_len = message_.size();
    iov_count_ = 1;
    send_total_bytes_ = iov_[0].iov_len;
    return;
}

http_conn::HTTP_CODE http_conn::prase_recvData() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE result = NO_REQUEST;
    char* text = nullptr;

    while ((cur_check_ == CHECK_CONTENT && line_status == LINE_OK) || (line_status = parse_one_line()) == LINE_OK) {
        text = get_line();
        new_line_idx_ = rd_buff_.check_idx_; // 更新至新一行的起始位置
        // std::clog << "[new line]: " << text << std::endl;
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
    requ_info_.file_path_ = root_;
    const char which_page = requ_info_.url_[1];
    if (requ_info_.method_ == POST && (which_page == '2' || which_page == '3')) { // 登录或注册操作
        // 提取账号密码
        // 若为登录op，判断信息是否匹配
        // 若为注册op，则注册用户并判断是否注册成功
    }
    
    // 获取目标文件的路径
    if (which_page == '0') { // 请求注册用户页面
        requ_info_.file_path_ += register_page;
    } else if (which_page == '1') { // 请求登录页面
        requ_info_.file_path_ += log_page;
    } else if (which_page == '5') { // 请求照片页面
        requ_info_.file_path_ += picture_page;
    } else if (which_page == '6') { // 请求视频页面
        requ_info_.file_path_ += video_page;
    } else if (which_page == '7') { // 关注页面
        requ_info_.file_path_ += follow_page;
    } else { // 欢迎界面
        requ_info_.file_path_ += requ_info_.url_;
    }

    auto ret = ::stat(requ_info_.file_path_.c_str(), &file_stat_);
    if (ret != 0) {
        return NO_RESOURCE;
    } else {
        if (!(file_stat_.st_mode & S_IROTH))
            return FORBIDDEN_REQUEST;
        if (S_ISDIR(file_stat_.st_mode))
            return BAD_REQUEST;
    }
    
    int target_file_fd = ::open(requ_info_.file_path_.c_str(), O_RDONLY);
    if (target_file_fd == -1) {
        return INTERNAL_ERROR;
    }

    file_addr_ = ::mmap(0, file_stat_.st_size, PROT_READ, MAP_PRIVATE, target_file_fd, 0);
    if (!file_addr_) {
        return INTERNAL_ERROR;
    }

    close(target_file_fd);
    return FILE_REQUEST;
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
        // std::clog << "<Unknow header info>: " << text << std::endl;
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
    this->requ_info_ = {};
    cur_check_ = CHECK_REQUESTLINE;
    new_line_idx_ = 0;
    file_stat_ = {};
    file_addr_ = nullptr;
    iov_count_ = 0;
    send_total_bytes_ = 0;
    
    std::memset(iov_, 0, sizeof(iov_));
}
