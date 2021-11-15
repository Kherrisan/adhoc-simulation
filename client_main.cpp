//
// Created by 邹迪凯 on 2021/10/28.
//
#include <iostream>
#include <thread>
#include "client.h"
#include "pipeline.h"


int main(int argc, char **argv) {
    cout << "start client!" << endl;
    string strServerPort(argv[1]);
    boost::asio::io_context io_context;
    tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), stoi(strServerPort));
    auto client = new ad_hoc_client(endpoint, io_context);
    client->pipeline.add(new ad_hoc_client_message_decoder())
    .add()
    //启动一个线程来运行io_context.run，这样接收数据的流程就不会被用户线程的操作干扰。
    std::thread t(boost::bind(&boost::asio::io_service::run, &io_context));
    ad_hoc_message msg;
    while (true) {
        int remotePort;
        string line;
        int localPort = client->get_sent_id();
        msg.sendid(localPort);
        cin >> remotePort >> line;
        msg.receiveid(remotePort);
        msg.body_length(line.size());
        memcpy(msg.body(), line.c_str(), msg.body_length());
        msg.encode_header();
        client->write(msg);
    }
    t.join();
    return 0;
}
