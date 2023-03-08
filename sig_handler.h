#if !defined(SIG_HANDLER_H_)
#define SIG_HANDLER_H_

class signal_handler {
public:
    static void sig_callback(int sig);
    static void register_sig(int sig, void (*handler)(int), bool restart);
};


#endif // SIG_HANDLER_H_
