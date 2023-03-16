#if !defined(HTTP_CONN_H_)
#define HTTP_CONN_H_

#include <arpa/inet.h>
#include <cstdlib>
#include <string>

class http_conn {
public:
    const static std::size_t RD_BUFFER_SIZE = 1024 * 3;
    const static std::size_t WR_BUFFER_SIZE = 1024 * 3;
    static std::size_t user_count; 

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

public:
    http_conn() = default;
    bool recv_data();
    void init();
    void init(const int fd, const sockaddr_in& addr);
    void setStatus(const char s) { if (s == 0 || s == 1) status_ = s; }
    void process();

private:
    http_conn::HTTP_CODE prase_recvData();
    inline char* get_line();
    http_conn::HTTP_CODE handle_request();
    http_conn::LINE_STATUS parse_one_line();
    HTTP_CODE parse_request_line(const std::string& text);
    HTTP_CODE parse_request_header(const std::string& text);
    HTTP_CODE parse_request_content(const std::string& text);
private:
    int fd_;
    char status_; // read: 0, write: 1
    sockaddr_in addr_;

    struct {
        char buff_[RD_BUFFER_SIZE];
        int read_idx_;
        int check_idx_;
    } rd_buff_;

    struct {
        char buff_[RD_BUFFER_SIZE];
    } wr_buff_;

    // 解析http-request相关
    CHECK_STATE cur_check_; // 主状态机解析进度
    int new_line_idx_; // 新一行的开始下标
    // request-info相关
    struct {
        std::string url_;
        METHOD method_;
        std::string version_;
        std::string host_;
        std::string content_;
        std::size_t content_len_;
        bool linger_;
    } requ_info_;
};

#endif // HTTP_CONN_H_
