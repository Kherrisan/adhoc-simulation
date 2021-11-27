#include <iostream>
#include <thread>
#include "message.h"
#include "BlackHole.h"

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
    auto client = new bh_client(endpoint, io_context, stoi(localPort), wormhole);
    //启动一个线程来运行io_context.run，这样接收数据的流程就不会被用户线程的操作干扰。
    std::thread t(boost::bind(&boost::asio::io_service::run, &io_context));

    ad_hoc_message msg;
    while (true) {
        int remotePort;
        string line;
        int localPort = client->get_sent_id();
        msg.sendid(localPort);
        cin >> remotePort >> line;
        msg.destid(remotePort);
        msg.sourceid(localPort);
        msg.receiveid(remotePort);
        msg.body_length(line.size());
        msg.msg_type(ORDINARY_MESSAGE);
        memcpy(msg.body(), line.c_str(), msg.body_length());
        msg.encode_header();
        client->write(msg);
    }
    t.join();
    return 0;
}
