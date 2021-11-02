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
    ad_hoc_client(tcp::endpoint &endpoint, boost::asio::io_context &io_context)
            : socket(io_context),
              io_context(io_context) {
        socket.async_connect(endpoint,
                             boost::bind(
                                     &ad_hoc_client::handle_connect,
                                     this,
                                     boost::asio::placeholders::error));
    }

    void write(const ad_hoc_message &msg) {
        io_context.post(boost::bind(&ad_hoc_client::do_write, this, msg));
    }

    void close() {
        io_context.post(boost::bind(&ad_hoc_client::do_close, this));
    }

    ~ad_hoc_client() {

    }

private:
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

    void handle_read_body(const boost::system::error_code &error,) {
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

    boost::asio::io_context &io_context;
    tcp::socket socket;
    ad_hoc_message read_msg_;
    ad_hoc_message_queue write_msgs_;
};

#endif //ADHOC_SIMULATION_CLIENT_H
