//
// Created by 邹迪凯 on 2021/10/28.
//

#ifndef ADHOC_SIMULATION_CLIENT_H
#define ADHOC_SIMULATION_CLIENT_H

#include <string>
#include <deque>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

#include "message.h"

using boost::asio::ip::tcp;
using namespace std;

typedef deque<ad_hoc_message> ad_hoc_message_queue;

class ad_hoc_client {
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
    ad_hoc_client(tcp::endpoint &endpoint, boost::asio::io_context &io_context)
            : socket(io_context),
              io_context(io_context) {
        socket.async_connect(endpoint,
                             boost::bind(
                                     &ad_hoc_client::handle_connect,
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
    void write(const ad_hoc_message &msg) {
        io_context.post(boost::bind(&ad_hoc_client::do_write, this, msg));
    }

    void close() {
        io_context.post(boost::bind(&ad_hoc_client::do_close, this));
    }

    ~ad_hoc_client() {

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
                                    boost::bind(&ad_hoc_client::handle_read_header, this,
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
                                    boost::bind(&ad_hoc_client::handle_read_body,
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
            cout.write(read_msg_.body(), read_msg_.body_length());
            cout << endl;
            boost::asio::async_read(socket,
                                    boost::asio::buffer(read_msg_.data(), ADHOCMESSAGE_HEADER_LENGTH),
                                    boost::bind(&ad_hoc_client::handle_read_header,
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
            cout << "sent " << write_msgs_.front().length() << " bytes to " << write_msgs_.front().id() << ":\n\t";
            cout.write(write_msgs_.front().body(), write_msgs_.front().body_length());
            cout << endl;
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
                boost::asio::async_write(socket,
                                         boost::asio::buffer(write_msgs_.front().data(),
                                                             write_msgs_.front().length()),
                                         boost::bind(&ad_hoc_client::handle_write,
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
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            boost::asio::async_write(socket,
                                     boost::asio::buffer(write_msgs_.front().data(),
                                                         write_msgs_.front().length()),
                                     boost::bind(&ad_hoc_client::handle_write,
                                                 this,
                                                 boost::asio::placeholders::error));
        }
    }

    void do_close() {
        socket.close();
    }

    //数据成员，同ad_hoc_session中的对应成员。
    boost::asio::io_context &io_context;
    tcp::socket socket;
    ad_hoc_message read_msg_;
    ad_hoc_message_queue write_msgs_;
};

#endif //ADHOC_SIMULATION_CLIENT_H
