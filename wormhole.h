//
// Created by 邹迪凯 on 2021/11/14.
//

#ifndef ADHOC_SIMULATION_WORMHOLE_H
#define ADHOC_SIMULATION_WORMHOLE_H

#include <string>
#include <deque>
#include <vector>
#include <ctime>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

#include "message_handler.h"
#include "utils.h"

const int AODV_WORMHOLE_DIFF_THRESHOLD = 2;

using boost::asio::ip::tcp;
using namespace std;


typedef deque<ad_hoc_message> ad_hoc_message_queue;

struct ad_hoc_wormhole_watchdog_item {
    //input-output(rx-tx)
    int diff;
};

class ad_hoc_wormhole_watchdog {
public:
    void handle_back(ad_hoc_aodv_back &back, ad_hoc_client_routing_table &routing_table) {
        if (back.receiver != back.msg_dest) {
            increment_rx(back.receiver);
        }
        if (back.sender != back.msg_src) {
            increment_tx(back.sender);
        }
#if DEBUG
        print();
#endif
        check(back.receiver, routing_table);
        check(back.sender, routing_table);
    }

    void check(int id, ad_hoc_client_routing_table &routing_table) {
        if (neighbor_diff_map[id].diff < -AODV_WORMHOLE_DIFF_THRESHOLD) {
            //rx-tx<0，收的少发的多
            cout << "found a wormhole node: " << id << endl;
            routing_table.remove_by_next(id);
        } else if (neighbor_diff_map[id].diff > AODV_WORMHOLE_DIFF_THRESHOLD) {
            //rx-tx>0，收的多发的少
            cout << "found a blackhole node: " << id << endl;
            routing_table.remove_by_next(id);
        }
    }

    void increment_rx(int neighbor) {
        if (neighbor_diff_map.find(neighbor) == neighbor_diff_map.end()) {
            neighbor_diff_map[neighbor] = ad_hoc_wormhole_watchdog_item{0};
        }
        neighbor_diff_map[neighbor].diff++;
#if DEBUG
        print();
#endif
    }

    void increment_tx(int neighbor) {
        if (neighbor_diff_map.find(neighbor) == neighbor_diff_map.end()) {
            neighbor_diff_map[neighbor] = ad_hoc_wormhole_watchdog_item{0};
        }
        neighbor_diff_map[neighbor].diff--;
#if DEBUG
        print();
#endif
    }

    bool is_malicious(int neighbor) {
        return neighbor_diff_map.find(neighbor) != neighbor_diff_map.end() &&
               (neighbor_diff_map[neighbor].diff > AODV_WORMHOLE_DIFF_THRESHOLD ||
                neighbor_diff_map[neighbor].diff < -AODV_WORMHOLE_DIFF_THRESHOLD);
    }

    void print() {
        cout << "watchdog" << endl;
        for (int i = 0; i < 20; i++) {
            cout << "-";
        }
        cout << endl;
        cout << right
             << setw(10) << "neighbor" << "|"
             << setw(10) << "rx-tx" << "|"
             << endl;
        for (int i = 0; i < 20; i++) {
            cout << "-";
        }
        cout << endl;
        for (auto itr = neighbor_diff_map.begin(); itr != neighbor_diff_map.end(); itr++) {
            cout << setw(10) << itr->first << "|" << setw(10) << itr->second.diff << "|" << endl;
        }
        cout << endl;
    }

private:
    unordered_map<int, ad_hoc_wormhole_watchdog_item> neighbor_diff_map;
};


class ad_hoc_wormhole_client {
public:
    /**
     * ad_hoc_client 构造函数
     *
     * client有两个方向的数据流动：发送和接收。
     * 其中client的数据接收事件是在IO线程上触发和运行的，由io_context对象负责管理
     * 而client的数据发送事件则是在用户线程上触发，但也在IO线程上运行。
     *
     * @param endpoint 将要连接的server的IP和端口
     * @param io_context 接收数据的IO事件循环
     */
    ad_hoc_wormhole_client(tcp::endpoint &endpoint, boost::asio::io_context &io_context, ad_hoc_message_handler *client)
            : socket(io_context),
              io_context(io_context),
              client(client) {
        socket.async_connect(endpoint,
                             boost::bind(
                                     &ad_hoc_wormhole_client::handle_connect,
                                     this,
                                     boost::asio::placeholders::error));
    }

    /**
     * 主动发送消息
     *
     * 此函数由用户/上层服务在用户线程主动调用，但并不是立即发送，而是由io_context在事件循环中轮训到发数据事件时调用do_write函数
     *
     * @param msg 待发消息
     */
    void write(const ad_hoc_message msg) {
        io_context.post(boost::bind(&ad_hoc_wormhole_client::do_write, this, msg));
    }

    void close() {
        io_context.post(boost::bind(&ad_hoc_wormhole_client::do_close, this));
    }

    ~ad_hoc_wormhole_client() {

    }

private:
    /**
     * 与server发起连接成功后的回调函数
     *
     * 在建立连接成功后，发起异步读操作，等待数据到来。
     *
     * @param error
     */
    void handle_connect(const boost::system::error_code &error) {
        if (!error) {
            cout << "connected to " << socket.remote_endpoint().address() << ":" << socket.remote_endpoint().port()
                 << endl;
            cout << "local port is " << socket.local_endpoint().port() << endl;
            boost::asio::async_read(socket,
                                    boost::asio::buffer(read_msg_.data(), ADHOCMESSAGE_HEADER_LENGTH),
                                    boost::bind(&ad_hoc_wormhole_client::handle_read_header, this,
                                                boost::asio::placeholders::error));
        } else {
            cerr << error << endl;
        }
    }

    /**
     * 读取消息首部的回调函数
     * 同ad_hoc_session中的同名函数
     *
     * @param error
     */
    void handle_read_header(const boost::system::error_code &error) {
        if (!error && read_msg_.decode_header()) {
            boost::asio::async_read(socket,
                                    boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
                                    boost::bind(&ad_hoc_wormhole_client::handle_read_body,
                                                this,
                                                boost::asio::placeholders::error));
        } else {
            do_close();
        }
    }

    /**
     * 读取消息数据载荷的回调函数
     * 同ad_hoc_session中的同名函数
     *
     * @param error
     */
    void handle_read_body(const boost::system::error_code &error) {
        if (!error) {
#if DEBUG
            LOG_RECEIVED(read_msg_);
#endif
            ad_hoc_message body_msg;
            memcpy(body_msg.data(), read_msg_.body(), read_msg_.body_length());
            body_msg.decode_header();
            client->handle_message(body_msg, true);
            boost::asio::async_read(socket,
                                    boost::asio::buffer(read_msg_.data(), ADHOCMESSAGE_HEADER_LENGTH),
                                    boost::bind(&ad_hoc_wormhole_client::handle_read_header,
                                                this,
                                                boost::asio::placeholders::error));
        } else {
            do_close();
        }
    }

    /**
     * 发送消息的回调函数
     * 同ad_hoc_session中的同名函数
     *
     * @param error
     */
    void handle_write(const boost::system::error_code &error) {
        if (!error) {
            auto msg = write_msgs_.front();
#if DEBUG
            //            cout << "sent" << endl;
            //            print(msg);
#endif
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
                boost::asio::async_write(socket,
                                         boost::asio::buffer(write_msgs_.front().data(),
                                                             write_msgs_.front().length()),
                                         boost::bind(&ad_hoc_wormhole_client::handle_write,
                                                     this,
                                                     boost::asio::placeholders::error));
            }
        } else {
            do_close();
        }
    }

    /**
     * 发送消息函数
     *
     * 发送过程同session的deliver函数
     *
     * @param msg
     */
    void do_write(ad_hoc_message msg) {
#if DEBUG
        LOG_SENDING(msg);
#endif
        msg.sendid(socket.local_endpoint().port());
        msg.encode_header();
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            boost::asio::async_write(socket,
                                     boost::asio::buffer(write_msgs_.front().data(),
                                                         write_msgs_.front().length()),
                                     boost::bind(&ad_hoc_wormhole_client::handle_write,
                                                 this,
                                                 boost::asio::placeholders::error));
        }
    }

    void do_close() {
        socket.close();
    }

    boost::asio::io_context &io_context;
    tcp::socket socket;
    ad_hoc_message read_msg_;
    ad_hoc_message_queue write_msgs_;
    ad_hoc_message_handler *client;
};

#endif //ADHOC_SIMULATION_WORMHOLE_H
