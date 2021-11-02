//
// Created by 邹迪凯 on 2021/10/28.
//

#ifndef ADHOC_SIMULATION_SERVER_H
#define ADHOC_SIMULATION_SERVER_H

#include <string>
#include <boost/array.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <unordered_map>
#include <deque>

#include "message.h"

using boost::asio::ip::tcp;
using namespace std;

typedef deque<ad_hoc_message> message_queue;

class ad_hoc_participant {
public:
    virtual ~ad_hoc_participant() {}

    virtual void deliver(const ad_hoc_message &msg) = 0;
};

typedef boost::shared_ptr<ad_hoc_participant> ad_hoc_participant_ptr;

class ad_hoc_scope {
public:
    void join(int id, ad_hoc_participant_ptr participant) {
        session_map[id] = participant;
    }

    void leave(int id) {
        session_map.erase(id);
    }

    bool deliver(const ad_hoc_message &msg) {
        if (session_map.find(msg.id()) == session_map.end()) {
            return false;
        }
        session_map[msg.id()]->deliver(msg);
        return true;
    }

private:
    unordered_map<int, ad_hoc_participant_ptr> session_map;
};

class ad_hoc_session : public boost::enable_shared_from_this<ad_hoc_session>,
                       public ad_hoc_participant {
public:
    ad_hoc_session(boost::asio::io_context &ioContext, ad_hoc_scope &scope) : socket_(ioContext), scope(scope) {
    }

    tcp::socket &socket() {
        return socket_;
    }

    void start() {
        scope.join(id(), shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.data(), ADHOCMESSAGE_HEADER_LENGTH),
                                boost::bind(
                                        &ad_hoc_session::handle_read_header,
                                        shared_from_this(),
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
    }

    void handle_read_header(const boost::system::error_code &error, size_t bytes_transferred) {
        if (!error && read_msg_.decode_header()) {
            boost::asio::async_read(socket_,
                                    boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
                                    boost::bind(
                                            &ad_hoc_session::handle_read_body,
                                            shared_from_this(),
                                            boost::asio::placeholders::error));
        }
    }

    void handle_read_body(const boost::system::error_code &error) {
        if (!error) {
            string body(read_msg_.body(), read_msg_.body_length());
            cout << "[" << id() << "->" << read_msg_.id() << "] " << body << endl;
            scope.deliver(read_msg_);
            read_msg_ = ad_hoc_message();
            boost::asio::async_read(socket_,
                                    boost::asio::buffer(read_msg_.data(), ADHOCMESSAGE_HEADER_LENGTH),
                                    boost::bind(
                                            &ad_hoc_session::handle_read_header,
                                            shared_from_this(),
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
        }
    }

    void handle_write(const boost::system::error_code &error) {
        if (!error) {
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
                boost::asio::async_write(socket_,
                                         boost::asio::buffer(write_msgs_.front().data(),
                                                             write_msgs_.front().length()),
                                         boost::bind(&ad_hoc_session::handle_write, shared_from_this(),
                                                     boost::asio::placeholders::error));
            }
        } else {
            scope.leave(id());
        }
    }

    void deliver(const ad_hoc_message &msg) override {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            boost::asio::async_write(socket_,
                                     boost::asio::buffer(write_msgs_.front().data(),
                                                         write_msgs_.front().length()),
                                     boost::bind(&ad_hoc_session::handle_write,
                                                 shared_from_this(),
                                                 boost::asio::placeholders::error));
        }
    }

    int id() {
        return socket_.remote_endpoint().port();
    }

private:
    ad_hoc_scope &scope;
    tcp::socket socket_;
    ad_hoc_message read_msg_;
    message_queue write_msgs_;
};

typedef boost::shared_ptr<ad_hoc_session> ad_hoc_session_ptr;

class ad_hoc_server {
public:
    ad_hoc_server(const tcp::endpoint &endpoint, boost::asio::io_context &io_context) : acceptor(io_context, endpoint),
                                                                                        io_context(io_context) {
        cout << "start listening at port " << endpoint.port() << endl;
        ad_hoc_session_ptr new_session(new ad_hoc_session(io_context, scope));
        acceptor.async_accept(new_session->socket(),
                              boost::bind(
                                      &ad_hoc_server::handle_accept,
                                      this,
                                      new_session,
                                      boost::asio::placeholders::error
                              ));
    }

private:
    void handle_accept(ad_hoc_session_ptr session, const boost::system::error_code &error) {
        if (!error) {
            cout << "accept incoming connection: " << session->socket().remote_endpoint().address() << ":"
                 << session->id() << endl;
            session->start();
            ad_hoc_session_ptr new_session(new ad_hoc_session(io_context, scope));
            acceptor.async_accept(new_session->socket(),
                                  boost::bind(
                                          &ad_hoc_server::handle_accept,
                                          this,
                                          new_session,
                                          boost::asio::placeholders::error
                                  ));
        } else {
            cerr << error << endl;
        }
    }

    tcp::acceptor acceptor;
    boost::asio::io_context &io_context;
    ad_hoc_scope scope;
};

#endif //ADHOC_SIMULATION_SERVER_H
