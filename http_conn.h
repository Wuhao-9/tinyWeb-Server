#if !defined(HTTP_CONN_H_)
#define HTTP_CONN_H_

#include "epoll_info.hpp"
#include "sql_conn_pool.h"
#include <arpa/inet.h>
#include <sys/uio.h> // for writev, struct iovec
#include <sys/stat.h>
#include <cstdlib>
#include <sstream>
#include <cassert>
#include <string>
#include <atomic>

class web_server; // forward declaration

class http_conn : public epoll_info<http_conn>{
public:
    const static std::size_t RD_BUFFER_SIZE = 1024 * 3;
    static std::atomic<std::size_t> user_count; 

    enum METHOD {
        GET = 1,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };

    enum CHECK_STATE { // 有限状态机当前状态
        CHECK_REQUESTLINE,
        CHECK_HEADER,
        CHECK_CONTENT
    };

    enum HTTP_CODE { // 当前http-request的状态
        NO_REQUEST, // 请求不完整
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    enum LINE_STATUS {
        LINE_OK = 0,
        LINE_BAD,   // 当前行信息格式错误
        LINE_OPEN   // 当前行不完整
    };

    enum CUR_STATUS {
        RECV,
        WRITE
    };
    
public:
    http_conn(web_server& context);
    bool recv_data();
    void init();
    void init(const int fd, const sockaddr_in& addr, const std::string& root);
    void setStatus(const char s) { if (s == 0 || s == 1) status_ = s; else assert(false); }
    char getStatus() { return status_; }
    void process();
    void send();
    const int getFD() { return fd_; }
    static void init_users_SQL_info(sql_conn_pool* pool);
    const MYSQL** get_SQLConn_handle() const { return const_cast<const MYSQL**>(&sql_conn_); }
    MYSQL** get_SQLConn_handle() { return &sql_conn_; }

private:
    http_conn::HTTP_CODE prase_recvData();
    inline char* get_line();
    http_conn::HTTP_CODE handle_request();
    http_conn::LINE_STATUS parse_one_line();
    HTTP_CODE parse_request_line(const std::string& text);
    HTTP_CODE parse_request_header(const std::string& text);
    HTTP_CODE parse_request_content(const std::string& text);
    void build_send_data(const HTTP_CODE requ_status);
    void build_response(int code, const char* tittle, const std::size_t content_len, const char* content);
    void build_response(int code, const char* tittle);
    inline void assemble_state_line(int code, const char* tittle);
    inline void assemble_reps_header(const std::size_t content_len);
    inline void assemble_content(const char* content);
    inline void munmap();
    bool try_send();
private:
    web_server& context_;
    int fd_;
    char status_; // read: 0, write: 1
    sockaddr_in addr_;

    struct {
        char buff_[RD_BUFFER_SIZE];
        int read_idx_;
        int check_idx_;
    } rd_buff_;

    // 解析http-request相关
    std::string root_; // 资源文件根目录
    CHECK_STATE cur_check_; // 主状态机解析进度
    int new_line_idx_; // 新一行的开始下标
    // request-info相关
    struct {
        std::string file_path_;
        std::string url_;
        std::string version_;
        std::string host_;
        std::string content_;
        std::size_t content_len_;
        bool linger_;
        METHOD method_;
    } requ_info_;
    // response-info相关
    struct stat file_stat_;
    void* file_addr_; // 目标文件的mmap地址
    std::ostringstream resp_;
    struct iovec iov_[2];
    unsigned iov_count_;
    bool html_file;
    std::size_t send_total_bytes_;
    std::string message_;
    // 当前http连接持有的数据库连接
    MYSQL* sql_conn_;
};

#endif // HTTP_CONN_H_
