#if !defined(CONFIG_H_)
#define CONFIG_H_

class ser_config {
public:
    ser_config() = default;
    static void parse_arg(int argc, char* argv[]);

public:
    static int port;
    static int log_write;
    static int trigger_mode;
    static int listen_trigger;
    static int conn_trigger;
    static int opt_linger;
    static int sql_num;
    static int thread_num;
    static int close_log;
    static int net_model;
};

#endif // CONFIG_H_
