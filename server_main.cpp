//
// Created by 邹迪凯 on 2021/10/28.
//
#include <iostream>
#include "server.h"


int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: chat_server <port> [args ...]\n";
        return 1;
    }
    string strPort(argv[1]);
    bool wc = false;
    if (argc > 2 && !strcmp(argv[2], "wc")) {
        wc = true;
    }
    int port = stoi(strPort);
    boost::asio::io_context io_context;
    tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port);
    auto *server = new ad_hoc_server(endpoint, io_context, wc);
    std::thread t(boost::bind(&boost::asio::io_service::run, &io_context));

    string cmd;
    while (true) {
        cin >> cmd;
        if (!strcmp(cmd.c_str(), "re")) {
            server->regenerate_matrix();
        }
    }
    t.join();
    return 0;
}



