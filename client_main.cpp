//
// Created by 邹迪凯 on 2021/10/28.
//
#include <iostream>
#include <thread>
#include "client.h"

int main(int argc, char **argv) {
    cout << "start client!" << endl;
    string strServerPort(argv[1]);
    boost::asio::io_context io_context;
    tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), stoi(strServerPort));
    auto client = new ad_hoc_client(endpoint, io_context);
    std::thread t(boost::bind(&boost::asio::io_service::run, &io_context));
    ad_hoc_message msg;
//    boost::thread t(boost::bind(&boost::asio::io_service::run, &io_context));
    while (true) {
        int remotePort;
        string line;
        cin >> remotePort >> line;
        msg.id(remotePort);
        msg.body_length(line.size());
        memcpy(msg.body(), line.c_str(), msg.body_length());
        msg.encode_header();
        client->write(msg);
    }
    t.join();
    return 0;
}
