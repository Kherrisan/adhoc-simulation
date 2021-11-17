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
#include "aodv.h"

using boost::asio::ip::tcp;
using namespace std;

typedef deque<ad_hoc_message> ad_hoc_message_queue;

const int AODV_HELLO_INTERVAL = 30;

void print(ad_hoc_message &msg) {
    cout << "[message] src: " << msg.sourceid() << ", dst: " << msg.destid() << ", sender: " << msg.sendid()
         << ", receiver: " << msg.receiveid() << ", type: " << msg.msg_type() << endl;
    if (msg.msg_type() == ORDINARY_MESSAGE) {
        cout.write(msg.body(), msg.body_length());
        cout << endl;
    } else {
        print_aodv(msg.body());
    }
    cout << endl;
}

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
              io_context(io_context),
              hello_timer(io_context, boost::posix_time::seconds(AODV_HELLO_INTERVAL)) {
        //第一个参数指向某个IP主机的IP端口，第二个是偏函数对象，实际代码地址指向成员函数handle_connect
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

    int get_sent_id() {
        return socket.local_endpoint().port();
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
            cout << "received" << endl;
            print(read_msg_);
            if (read_msg_.msg_type() == AODV_MESSAGE) {
                handle_adov_message();
            } else {
                handle_user_message(read_msg_);
            }
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
            auto msg = write_msgs_.front();
            cout << "sent" << endl;
            print_message(msg);
            if (msg.msg_type() == ORDINARY_MESSAGE) {
                cout.write(msg.body(), msg.body_length());
                cout << endl;
            } else {
                print_aodv(msg.body());
            }
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
        if (routing_table_.contains(msg.destid()) || msg.receiveid() == AODV_BROADCAST_ADDRESS) {
            if (routing_table_.contains(msg.destid())) {
                auto route = routing_table_.route(msg.destid());
                aodv_restart_route_timer(route);
                msg.receiveid(route.next_hop);
            }
            msg.sendid(id());
            msg.encode_header();
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
        } else {
            send_rreq(msg.destid(), -1);
            waiting_rreq_msg_queue.push_back(msg);
        }
    }

    void handle_user_message(ad_hoc_message msg) {
        if (id() == msg.destid()) {
            cout.write(read_msg_.body(), read_msg_.body_length());
            cout << endl;
        } else {
            //转发消息给下一跳
            auto route = routing_table_.route(msg.destid());
            aodv_restart_route_timer(route);
            msg.sendid(id());
            msg.receiveid(route.next_hop);
            msg.encode_header();
            write(msg);
        }
    }

    void handle_adov_message() {
        int *aodv_type = (int *) read_msg_.body();
        if (*aodv_type == AODV_RREQ) {
            auto rreq = (ad_hoc_aodv_rreq *) read_msg_.body();
            handle_rreq(read_msg_, *rreq);
        } else if (*aodv_type == AODV_RREP) {
            auto rrep = (ad_hoc_aodv_rrep *) read_msg_.body();
            handle_rrep(read_msg_, *rrep);
        } else if (*aodv_type == AODV_RERR) {
            auto rerr = (ad_hoc_aodv_rerr *) read_msg_.body();
            handle_rerr(read_msg_, *rerr);
        } else {
            handle_hello(read_msg_);
        }
    }

    void aodv_restart_route_timer(ad_hoc_client_routing_table_item item) {
        boost::asio::deadline_timer timer(io_context, boost::posix_time::seconds(10000000));
//        timer.async_wait(boost::bind(&ad_hoc_client::aodv_route_timeout, this, item));
    }

    /**
     * client节点接收到rreq后的处理
     *
     * @param msg
     * @param rreq
     */
    void handle_rreq(ad_hoc_message &msg, ad_hoc_aodv_rreq &rreq) {
        int hops = rreq.hops + 1;
        if (rreq_buffer.contains(rreq.orig, rreq.id)) {
            return;
        } else {
            auto timer = this->rreq_buffer.setup_path_discovery_timer(io_context, rreq.orig, rreq.id);
//            timer->async_wait(boost::bind(&ad_hoc_client::aodv_path_discovery_timeout, this, rreq.orig, rreq.id));
        }

        //建立反向路由表，便于反传rrep
        if (routing_table_.contains(rreq.orig)) {
            auto orig_route = routing_table_.route(rreq.orig);
            if (orig_route.seq < rreq.orig_seq) {
                //更新路由表中的seq
                orig_route.seq = rreq.orig_seq;
                aodv_restart_route_timer(orig_route);
            } else if (orig_route.seq == rreq.orig_seq) {
                if (orig_route.hops > hops) {
                    //更新路由表下一跳
                    orig_route.hops = hops;
                    orig_route.next_hop = msg.sendid();
                    aodv_restart_route_timer(orig_route);
                }
            } else if (orig_route.seq == -1) {
                orig_route.seq = rreq.orig_seq;
                aodv_restart_route_timer(orig_route);
            }
        } else {
            ad_hoc_client_routing_table_item route{rreq.orig, msg.sendid(), rreq.orig_seq, hops};
            routing_table_.insert(route);
            aodv_restart_route_timer(route);
        }
        routing_table_.print();

        //到达目的节点，返回rrep
        if (id() == rreq.dest) {
            this->aodv_seq += 1;
            send_rrep(rreq.orig, id(), this->aodv_seq, 0, msg.sendid());
            return;
        }

        //如果某个中间节点在路由表中查到了dest，且路由表中的seq大于（等于）rreq中的seq
        if (routing_table_.contains(rreq.dest)) {
            auto dest_route = routing_table_.route(rreq.dest);
            if (dest_route.seq >= rreq.dest_seq) {
                send_rrep(rreq.orig, rreq.dest, dest_route.seq, dest_route.hops, msg.sendid());
            }
        } else {
            rreq.hops += 1;
            forward_rreq(msg, rreq);
        }
    }

    void handle_rrep(ad_hoc_message &msg, ad_hoc_aodv_rrep &rrep) {
        int hops = rrep.hops + 1;

        if (id() == rrep.orig) {
            if (routing_table_.contains((rrep.dest))) {
                auto dest_route = routing_table_.route(rrep.dest);
                int dest_route_hops = dest_route.hops;
                if (dest_route_hops > hops) {
                    dest_route.hops = hops;
                    aodv_restart_route_timer(routing_table_.route(rrep.dest));
                }
            } else {
                ad_hoc_client_routing_table_item route{rrep.dest, msg.sendid(), rrep.dest_seq, hops};
                routing_table_.insert(route);
                aodv_restart_route_timer(route);
            }

            //pending message
            for (auto msg = waiting_rreq_msg_queue.begin(); msg != waiting_rreq_msg_queue.end();) {
                if (msg->destid() == rrep.dest) {
                    write(*msg);
                    msg = waiting_rreq_msg_queue.erase(msg);
                } else {
                    msg++;
                }
            }

        } else {
            if (routing_table_.contains(rrep.dest)) {
                auto dest_route = routing_table_.route(rrep.dest);
                dest_route.seq = rrep.dest_seq;
                aodv_restart_route_timer(dest_route);
            } else {
                ad_hoc_client_routing_table_item route{msg.destid(), msg.sendid(), rrep.dest_seq, hops};
                aodv_restart_route_timer(routing_table_.route(rrep.dest));
            }
            auto route = routing_table_.route(rrep.orig);
            forward_rrep(msg, route.next_hop);
        }

        routing_table_.print();
    }

    void handle_rerr(ad_hoc_message &msg, ad_hoc_aodv_rerr &rerr) {
        if (id() == rerr.dest) {
            return;
        }
        if (routing_table_.contains(rerr.dest)) {
            auto dest_route = routing_table_.route(rerr.dest);
            if (dest_route.next_hop == msg.sendid()) {
                routing_table_.erase(rerr.dest);
                forward_rerr(msg);
            }
        }
    }

    void handle_hello(ad_hoc_message &msg) {
        int neighbor = msg.sendid();
        if (this->neighbors.contains(neighbor)) {
            auto timer = neighbors.timer(neighbor);
            neighbors.discard(neighbor);
            timer = neighbors.setup_neighbor_timer(io_context, neighbor);
//            timer->async_wait(boost::bind(&ad_hoc_client::aodv_neighbor_timeout, this, neighbor));

            auto route = routing_table_.route(neighbor);
            aodv_restart_route_timer(route);
        } else {
            auto timer = neighbors.setup_neighbor_timer(io_context, neighbor);
//            timer->async_wait(boost::bind(&ad_hoc_client::aodv_neighbor_timeout, this, neighbor));

            if (routing_table_.contains(neighbor)) {
                auto route = routing_table_.route(neighbor);
                aodv_restart_route_timer(route);
            } else {
                ad_hoc_client_routing_table_item route{neighbor, neighbor, 1, 1};
                aodv_restart_route_timer(route);
            }
        }
    }

    void forward_rreq(ad_hoc_message &msg, ad_hoc_aodv_rreq &rreq) {
        memcpy(msg.body(), &rreq, msg.body_length());
        msg.sendid(id());
        msg.receiveid(AODV_BROADCAST_ADDRESS);
        msg.encode_header();
        write(msg);
    }

    void forward_rrep(ad_hoc_message &msg, int next_hop) {
        msg.sendid(id());
        msg.receiveid(next_hop);
        msg.encode_header();
        write(msg);
    }

    void forward_rerr(ad_hoc_message &msg) {
        msg.sendid(id());
        msg.receiveid(AODV_BROADCAST_ADDRESS);
        write(msg);
    }

    void send_rreq(int dest, int dest_seq) {
        this->aodv_rreq_id += 1;
        this->aodv_seq += 1;
        ad_hoc_message msg;
        msg.sourceid(id());
        msg.destid(dest);
        msg.sendid(id());
        msg.receiveid(AODV_BROADCAST_ADDRESS);
        msg.msg_type(AODV_MESSAGE);
        ad_hoc_aodv_rreq rreq{AODV_RREQ, 0, this->aodv_rreq_id, dest, dest_seq, id(), this->aodv_seq};
        msg.body_length(sizeof(rreq));
        memcpy(msg.body(), &rreq, msg.body_length());
        write(msg);
        auto timer = rreq_buffer.setup_path_discovery_timer(io_context, id(), rreq.id);
//        timer->async_wait(boost::bind(&ad_hoc_client::aodv_path_discovery_timeout, this, id(), rreq.id));
    }

    void send_rrep(int orig, int dest, int dest_seq, int hops, int next_hop) {
        ad_hoc_aodv_rrep rrep{AODV_RREP, hops, dest, dest_seq, orig};
        ad_hoc_message msg;
        msg.sendid(id());
        msg.receiveid(next_hop);
        msg.sourceid(id());
        msg.destid(orig);
        msg.msg_type(AODV_MESSAGE);
        msg.body_length(sizeof(rrep));
        memcpy(msg.body(), &rrep, msg.body_length());
        msg.encode_header();
        write(msg);
    }

    void send_rerr(int dest, int dest_seq) {
        dest_seq += 1;
        ad_hoc_message msg;
        msg.sendid(id());
        msg.receiveid(AODV_BROADCAST_ADDRESS);
        msg.sourceid(id());
        msg.destid(AODV_BROADCAST_ADDRESS);
        ad_hoc_aodv_rerr rerr{AODV_RERR, dest, dest_seq};
        msg.body_length(sizeof(rerr));
        memcpy(msg.body(), &rerr, msg.body_length());
        msg.encode_header();
        write(msg);
    }

    void send_hello() {
        ad_hoc_message msg;
        msg.sendid(id());
        msg.receiveid(AODV_BROADCAST_ADDRESS);
        msg.sourceid(id());
        msg.destid(AODV_BROADCAST_ADDRESS);
        ad_hoc_aodv_hello hello{AODV_HELLO};
        msg.body_length(sizeof(hello));
        memcpy(msg.body(), &hello, msg.body_length());
        msg.encode_header();
        write(msg);

        hello_timer.cancel();
        hello_timer.expires_from_now(boost::posix_time::seconds(AODV_HELLO_INTERVAL));
//        hello_timer.async_wait(boost::bind(&ad_hoc_client::send_hello, this));
    }

    void aodv_path_discovery_timeout(int node, int rreq_id) {
        if (rreq_buffer.contains(node, rreq_id)) {
            rreq_buffer.discard(node, rreq_id);
        }
    }

    void aodv_route_timeout(ad_hoc_client_routing_table_item &item) {
        routing_table_.erase(item.dest);
    }

    void aodv_neighbor_timeout(int neighbor) {
        auto dest_seq = routing_table_.route(neighbor).seq;
        routing_table_.erase(neighbor);
        neighbors.discard(neighbor);
        send_rerr(neighbor, dest_seq);
        send_rreq(neighbor, dest_seq + 1);
    }

    void do_close() {
        socket.close();
    }

    int id() {
        return socket.local_endpoint().port();
    }

    //数据成员，同ad_hoc_session中的对应成员。
    boost::asio::io_context &io_context;
    tcp::socket socket;
    ad_hoc_message read_msg_;
    ad_hoc_message_queue write_msgs_;
    ad_hoc_client_routing_table routing_table_;
    ad_hoc_message_queue waiting_rreq_msg_queue;
    ad_hoc_aodv_rreq_buffer rreq_buffer;
    ad_hoc_aodv_neighbor_list neighbors;
    boost::asio::deadline_timer hello_timer;
    int aodv_seq;
    int aodv_rreq_id;
};

#endif //ADHOC_SIMULATION_CLIENT_H
