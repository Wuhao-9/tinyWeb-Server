#if !defined(HTTP_CONN_H_)
#define HTTP_CONN_H_

#include <arpa/inet.h>
#include <cstdlib>

class http_conn {
public:
    const static std::size_t RD_BUFFER_SIZE = 1024 * 3;
    const static std::size_t WR_BUFFER_SIZE = 1024 * 3;
    static std::size_t user_count; 

public:
    http_conn() = default;
    bool recv_data();
    void init();
    void init(const int fd, const sockaddr_in& addr);
    void setStatus(const char s) { if (s == 0 || s == 1) status_ = s; }
    void process();

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

};

#endif // HTTP_CONN_H_
