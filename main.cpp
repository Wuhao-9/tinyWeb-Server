#include "config.h"
#include "web_server.h"
int main(int argc, char* argv[]) {
    ser_config::parse_arg(argc, argv);
    web_server server;
    server.start();
}