//
// Created by 邹迪凯 on 2021/10/28.
//
#include <iostream>
#include <thread>
#include "client.h"
#include "message.h"


int main(int argc, char **argv) {
    cout << "start client!" << endl;
    string strServerPort(argv[1]);
    string localPort(argv[2]);
    int wormhole = -1;
    if (argc > 3) {
        wormhole = stoi(string(argv[3]));
    }
    boost::asio::io_context io_context;
    tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), stoi(strServerPort));
    auto client = new ad_hoc_client(endpoint, io_context, stoi(localPort), wormhole);
    //启动一个线程来运行io_context.run，这样接收数据的流程就不会被用户线程的操作干扰。
    std::thread t(boost::bind(&boost::asio::io_service::run, &io_context));

    int remotePort;
    string line;

    while (true) {
        cin >> remotePort >> line;
        client->send_user_message(remotePort, line.c_str(), line.size());
    }
    t.join();
    return 0;
}
