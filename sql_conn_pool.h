#if !defined(SQL_CONN_POOL_H_)
#define SQL_CONN_POOL_H_

#include <mysql/mysql.h>
#include <condition_variable>
#include <string>
#include <mutex>
#include <list>

class sql_conn_pool {
public:
	static sql_conn_pool* get_SQLpool_Instance(); // singleton pattern

public:
    void init_connPool(std::string host, std::string user, std::string pwd, std::string dbName, uint32_t port, std::size_t maxConn, bool close_log);
    MYSQL* getDBConn();
    bool releaseConn(MYSQL* conn);
    int get_freeConn_num() const { return free_conn_num_; }
    void destroyPool();
    
public:
	std::string host;	        // 主机地址
	std::string userName;		// 登陆数据库用户名
	std::string passWord;	    // 登陆数据库密码
	std::string DBName;         // 使用数据库名
	uint32_t Port;		    // 数据库端口号
	bool close_log;	            // 日志开关

private:
    sql_conn_pool() {
        busy_conn_num_ = 0;
        free_conn_num_ = 0;
    }
    
    ~sql_conn_pool() {
        destroyPool();
    } 

private:
    std::size_t max_conn_num_;
    std::size_t busy_conn_num_;
    std::size_t free_conn_num_;
    std::list<MYSQL*> conn_list_;
    std::condition_variable cv_;
    std::mutex mutex_;

};

class sql_conn_guard {
public:
    sql_conn_guard(MYSQL** conn, sql_conn_pool* pool);
    ~sql_conn_guard();
private:
    MYSQL* cur_SQLconn_;
    sql_conn_pool* context_;
};

#endif // SQL_CONN_POOL_H_
