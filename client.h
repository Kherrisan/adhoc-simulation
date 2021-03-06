//
// Created by 邹迪凯 on 2021/10/28.
//

#ifndef ADHOC_SIMULATION_CLIENT_H
#define ADHOC_SIMULATION_CLIENT_H

#include <string>
#include <deque>
#include <ctime>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include "unordered_map"

#include "message.h"
#include "aodv.h"
#include "wormhole.h"
#include "message_handler.h"
#include "utils.h"

using boost::asio::ip::tcp;
using namespace std;

typedef deque<ad_hoc_message> ad_hoc_message_queue;

const int AODV_HELLO_INTERVAL = 10;
const int AODV_ACTIVE_ROUTE_TIMEOUT = 300;


class ad_hoc_client : public ad_hoc_message_handler {
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

    ad_hoc_client(tcp::endpoint &endpoint, boost::asio::io_context &io_context, int id, int another_wormhole)
            : socket(io_context),
              io_context(io_context),
              hello_timer(io_context, boost::posix_time::seconds(AODV_HELLO_INTERVAL)),
              wormhole(another_wormhole) {
        aodv_seq = 0;
        aodv_rreq_id = 0;
        //第一个参数指向某个IP主机的IP端口，第二个是偏函数对象，实际代码地址指向成员函数handle_connect
#if BINDING_PORT
        socket.open(boost::asio::ip::tcp::v4());
        socket.bind(tcp::endpoint(
                boost::asio::ip::address::from_string("127.0.0.1"),
                id
        ));
#endif
        socket.async_connect(endpoint,
                             boost::bind(
                                     &ad_hoc_client::handle_connect,
                                     this,
                                     boost::asio::placeholders::error));
        if (wormhole != -1) {
            tcp::endpoint wormhole_endpoint(boost::asio::ip::address::from_string("127.0.0.1"), wormhole);
            wormhole_client = new ad_hoc_wormhole_client(wormhole_endpoint, io_context, this);
        }
    }

    /**
        * 主动发送消息
        *
        * 此函数由用户/上层服务在用户线程主动调用，但并不是立即发送，而是由io_context在事件循环中轮训到发数据事件时调用do_write函数
        *
        * @param msg 待发消息
        */
    void write(ad_hoc_message msg) {
        if (wormhole == -1 && watchdog.is_malicious(msg.receiveid())) {
            return;
        }
        io_context.post(boost::bind(&ad_hoc_client::do_write, this, msg));
    }

    void write_to_wormhole(ad_hoc_message &msg) {
        ad_hoc_message wormhole_msg;
        msg.sourceid(id());
        wormhole_msg.body_length(msg.length());
        wormhole_msg.msg_type(WORMHOLE_MESSAGE);
        memcpy(wormhole_msg.body(), msg.data(), msg.length());
        wormhole_msg.encode_header();
        wormhole_client->write(wormhole_msg);
    }

    void close() {
        io_context.post(boost::bind(&ad_hoc_client::do_close, this));
    }

    int get_sent_id() {
        return socket.local_endpoint().port();
    }

    ~ad_hoc_client() {

    }

    void handle_message(ad_hoc_message &msg, bool through_wormhole) {
        if (wormhole == -1 && watchdog.is_malicious(msg.sendid())) {
            return;
        }
#if DEBUG
        LOG_HANDLE(msg);
#endif
        if (msg.msg_type() == AODV_MESSAGE) {
            handle_adov_message(msg, through_wormhole);
        } else {
#if !DEBUG
            LOG_HANDLE(msg);
#endif
            handle_user_message(msg, through_wormhole);
        }
    }

    void send_user_message(int dest, const char *text, int len) {
        ad_hoc_message msg;
        msg.msg_type(ORDINARY_MESSAGE);
        msg.body_length(len);
        memcpy(msg.body(), text, len);
        msg.destid(dest);
        msg.sourceid(id());
        msg.sendid(id());
        msg.receiveid(-1);
        msg.encode_header();
        write(msg);
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
#if DYNAMIC
            send_hello();
#endif
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
#if DEBUG
            LOG_RECEIVED(read_msg_);
#endif
            handle_message(read_msg_, false);
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
#if DEBUG
            //            cout << "sent" << endl;
            //            print_time();
            //            print_message(msg);
            //            if (msg.msg_type() == ORDINARY_MESSAGE) {
            //                cout.write(msg.body(), msg.body_length());
            //                cout << endl;
            //            } else {
            //                print_aodv(msg.body());
            //            }
            //            cout << endl;
#endif
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
        if (wormhole == -1 && watchdog.is_malicious(msg.receiveid())) {
            return;
        }
        if (routing_table_.contains(msg.destid()) || msg.receiveid() == AODV_BROADCAST_ADDRESS) {
            if (routing_table_.contains(msg.destid())) {
                auto route = routing_table_.route(msg.destid());
                //重新启动该路由表项的定时器
                aodv_restart_route_timer(route);
                msg.receiveid(route.next_hop);
            }
            msg.sendid(id());
            msg.encode_header();
#if DEBUG
            print("do_write", msg);
#endif
            bool write_in_progress = !write_msgs_.empty();
            write_msgs_.push_back(msg);
            if (!write_in_progress) {
                if (msg.msg_type() == ORDINARY_MESSAGE) {
                    broadcast_back(msg);
                }
                boost::asio::async_write(socket,
                                         boost::asio::buffer(write_msgs_.front().data(),
                                                             write_msgs_.front().length()),
                                         boost::bind(&ad_hoc_client::handle_write,
                                                     this,
                                                     boost::asio::placeholders::error));
            }
        } else {
            send_rreq(msg.destid(), -1);
            auto timer = msg_buffer.new_timer(io_context, msg);
            timer->expires_from_now(boost::posix_time::seconds(AODV_MESSAGE_WAITING_ROUTE_TIMEOUT));
            timer->async_wait(
                    boost::bind(&ad_hoc_client::aodv_msg_waiting_route_timeout, this, msg,
                                boost::asio::placeholders::error));
        }
    }

    void handle_user_message(ad_hoc_message msg, bool through_wormhole) {
        broadcast_back(msg);
        if (id() == msg.destid()) {
#if DEBUG
            cout.write(read_msg_.body(), read_msg_.body_length());
            cout << endl;
#endif
        } else if (wormhole != -1 && !through_wormhole) {
            write_to_wormhole(msg);
        } else {
            //转发消息给下一跳
            auto route = routing_table_.route(msg.destid());
            aodv_restart_route_timer(route);
            msg.sendid(id());
            msg.receiveid(route.next_hop);
            msg.encode_header();
            write(msg);
            broadcast_back(msg);
        }
    }

    void handle_adov_message(ad_hoc_message &msg, bool through_wormhole) {
        int *aodv_type = (int *) msg.body();
        if (*aodv_type == AODV_RREQ) {
            auto rreq = (ad_hoc_aodv_rreq *) msg.body();
            handle_rreq(msg, *rreq, through_wormhole);
        } else if (*aodv_type == AODV_RREP) {
            auto rrep = (ad_hoc_aodv_rrep *) msg.body();
            handle_rrep(msg, *rrep, through_wormhole);
        } else if (*aodv_type == AODV_RERR) {
            auto rerr = (ad_hoc_aodv_rerr *) msg.body();
            handle_rerr(msg, *rerr, through_wormhole);
        } else if (*aodv_type == AODV_HELLO) {
            handle_hello(msg, through_wormhole);
        } else if (*aodv_type == AODV_BACK && wormhole == -1) {
            auto back = (ad_hoc_aodv_back *) msg.body();
            watchdog.handle_back(*back, routing_table_);
        } else if (*aodv_type == AODV_ARC) {
            auto arc = (ad_hoc_aodv_arc *) msg.body();
            handle_arc(msg, *arc);
        }
    }

    void aodv_restart_route_timer(ad_hoc_client_routing_table_item item) {
#if DYNAMIC && AODV_ROUTE_TIMEOUT
        if (item.timer == nullptr) {
            item.timer = make_shared<boost::asio::deadline_timer>(io_context);
        }
        item.timer->expires_from_now(boost::posix_time::seconds(AODV_ACTIVE_ROUTE_TIMEOUT));
        item.timer->async_wait(
                boost::bind(&ad_hoc_client::aodv_route_timeout, this, item, boost::asio::placeholders::error));
#endif
    }

    /**
     * client节点接收到rreq后的处理
     *
     * @param msg
     * @param rreq
     */
    void handle_rreq(ad_hoc_message &msg, ad_hoc_aodv_rreq &rreq, bool through_wormhole) {
        if (id() == rreq.orig) {
            return;
        }

        int current_hops;
        if (through_wormhole) {
            current_hops = rreq.hops;
        } else {
            current_hops = rreq.hops + 1;
        }

        //建立反向路由，便于反传rrep
        if (routing_table_.contains(rreq.orig)) {
            auto orig_route = routing_table_.route(rreq.orig);
            if (orig_route.seq < rreq.orig_seq) {
                //更新路由表中的seq
                orig_route.seq = rreq.orig_seq;
                aodv_restart_route_timer(orig_route);
            } else if (orig_route.seq == rreq.orig_seq) {
                if (orig_route.hops > current_hops) {
                    //更新路由表下一跳
                    orig_route.hops = current_hops;
                    orig_route.next_hop = msg.sendid();
                    aodv_restart_route_timer(orig_route);
                }
            } else if (orig_route.seq == -1) {
                orig_route.seq = rreq.orig_seq;
                aodv_restart_route_timer(orig_route);
            }
        } else {
            ad_hoc_client_routing_table_item route{rreq.orig, msg.sendid(), rreq.orig_seq, current_hops,
                                                   make_shared<boost::asio::deadline_timer>(io_context,
                                                                                            boost::posix_time::seconds(
                                                                                                    AODV_ACTIVE_ROUTE_TIMEOUT))};
            routing_table_.insert(route);
            aodv_restart_route_timer(route);
        }

#if DEBUG

#endif

        if (rreq_buffer.contains(rreq.orig, rreq.id)) {
            return;
        } else {
            auto timer = this->rreq_buffer.new_timer(io_context, rreq.orig, rreq.id);
            timer->async_wait(boost::bind(&ad_hoc_client::aodv_path_discovery_timeout, this, rreq.orig, rreq.id));
        }

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
                if (wormhole != -1 && through_wormhole) {
                    //若rreq来自虫洞，则向虫洞返回rrep
                    ad_hoc_aodv_rrep rrep{AODV_RREP, dest_route.hops, rreq.dest, dest_route.seq, rreq.orig};
                    ad_hoc_message wrap_msg(AODV_MESSAGE, id(), msg.sendid(), id(), rreq.orig);
                    wrap_msg.body_length(sizeof(rrep));
                    memcpy(wrap_msg.body(), &rrep, wrap_msg.body_length());
                    wrap_msg.encode_header();
                    write_to_wormhole(wrap_msg);
                } else {
                    send_rrep(rreq.orig, rreq.dest, dest_route.seq, dest_route.hops, msg.sendid());
                }
            }
        } else {
            if (wormhole != -1) {
                //此节点是虫洞节点
                if (through_wormhole) {
                    broadcast_rreq(msg, rreq);
                } else {
                    rreq.hops += 1;
                    msg.body(rreq);
                    write_to_wormhole(msg);
                }
            } else {
                //此节点是普通节点
                rreq.hops += 1;
                msg.body(rreq);
                broadcast_rreq(msg, rreq);
            }
        }
    }

    void handle_rrep(ad_hoc_message &msg, ad_hoc_aodv_rrep &rrep, bool through_wormhole) {
        int current_hops;
        if (through_wormhole) {
            current_hops = rrep.hops;
        } else {
            current_hops = rrep.hops + 1;
        }

        if (id() == rrep.orig) {
            if (routing_table_.contains((rrep.dest))) {
                auto dest_route = routing_table_.route(rrep.dest);
                int dest_route_hops = dest_route.hops;
                if (dest_route_hops > current_hops) {
                    dest_route.hops = current_hops;
                    dest_route.next_hop = msg.sendid();
                    routing_table_.insert(dest_route);
                    aodv_restart_route_timer(routing_table_.route(rrep.dest));
                }
            } else {
                ad_hoc_client_routing_table_item route{rrep.dest, msg.sendid(), rrep.dest_seq, current_hops,
                                                       make_shared<boost::asio::deadline_timer>(io_context,
                                                                                                boost::posix_time::seconds(
                                                                                                        AODV_ACTIVE_ROUTE_TIMEOUT))};
                routing_table_.insert(route);
                aodv_restart_route_timer(route);
            }

            for (auto buffered_msg = msg_buffer.msg_timer_map.begin(); buffered_msg != msg_buffer.msg_timer_map.end();) {
                if (buffered_msg->first.destid() == rrep.dest) {
                    write(buffered_msg->first);
                    buffered_msg = msg_buffer.msg_timer_map.erase(buffered_msg);
                }else{
                    buffered_msg++;
                }
            }

        } else {
            if (routing_table_.contains(rrep.dest)) {
                auto dest_route = routing_table_.route(rrep.dest);
                dest_route.seq = rrep.dest_seq;
                aodv_restart_route_timer(dest_route);
            } else {
                ad_hoc_client_routing_table_item route{rrep.dest, msg.sendid(), rrep.dest_seq, current_hops,
                                                       make_shared<boost::asio::deadline_timer>(io_context,
                                                                                                boost::posix_time::seconds(
                                                                                                        AODV_ACTIVE_ROUTE_TIMEOUT))};
                routing_table_.insert(route);
                aodv_restart_route_timer(routing_table_.route(rrep.dest));
            }

            if (wormhole != -1) {
                //此节点是虫洞节点
                if (through_wormhole) {
                    //rrep从虫洞而来
                    auto route = routing_table_.route(rrep.orig);
                    forward_rrep(msg, route.next_hop);
                } else {
                    //rrep从普通路径而来，正要进入虫洞
                    rrep.hops += 1;
                    msg.body(rrep);
                    write_to_wormhole(msg);
                }
            } else {
                //此节点不是虫洞节点，正常操作
                auto route = routing_table_.route(rrep.orig);
                rrep.hops += 1;
                msg.body(rrep);
                forward_rrep(msg, route.next_hop);
            }
        }

#if DEBUG

#endif
    }

    void handle_rerr(ad_hoc_message &msg, ad_hoc_aodv_rerr &rerr, bool through_wormhole) {
        if (id() == rerr.dest) {
            return;
        }
        if (routing_table_.contains(rerr.dest)) {
            auto dest_route = routing_table_.route(rerr.dest);
            if (dest_route.next_hop == msg.sendid()) {
                routing_table_.remove(rerr.dest);
                if (wormhole != -1 && !through_wormhole) {
                    write_to_wormhole(msg);
                } else {
                    broadcast_rerr(msg);
                }
#if DEBUG

#endif
            }
        }
    }

    void handle_hello(ad_hoc_message &msg, bool through_wormhole) {
        int neighbor = msg.sendid();
        if (this->neighbors.contains(neighbor)) {
            auto timer = neighbors.timer(neighbor);
            timer->expires_from_now(boost::posix_time::seconds(AODV_HELLO_TIMEOUT));
            timer->async_wait(boost::bind(&ad_hoc_client::aodv_neighbor_timeout, this, neighbor,
                                          boost::asio::placeholders::error));

            auto route = routing_table_.route(neighbor);
            aodv_restart_route_timer(route);
        } else {
            auto timer = this->neighbors.new_timer(io_context, neighbor);
            timer->expires_from_now(boost::posix_time::seconds(AODV_HELLO_TIMEOUT));
            timer->async_wait(boost::bind(&ad_hoc_client::aodv_neighbor_timeout, this, neighbor,
                                          boost::asio::placeholders::error));

            if (routing_table_.contains(neighbor)) {
                auto route = routing_table_.route(neighbor);
                aodv_restart_route_timer(route);
            } else {
                ad_hoc_client_routing_table_item route{neighbor, neighbor, 1, 1,
                                                       make_shared<boost::asio::deadline_timer>(io_context,
                                                                                                boost::posix_time::seconds(
                                                                                                        AODV_ACTIVE_ROUTE_TIMEOUT))};
                routing_table_.insert(route);
#if DEBUG

#endif
                aodv_restart_route_timer(route);
            }
        }
    }

    void handle_arc(ad_hoc_message &msg, ad_hoc_aodv_arc &arc) {
        cout << "send success in this hop" << endl;
    }

    void broadcast_rreq(ad_hoc_message &msg, ad_hoc_aodv_rreq &rreq) {
        if (neighbors.neighbor_timer_map.empty()) {
            memcpy(msg.body(), &rreq, msg.body_length());
            msg.sendid(id());
            msg.receiveid(AODV_BROADCAST_ADDRESS);
            msg.sourceid(id());
            msg.destid(AODV_BROADCAST_ADDRESS);
            msg.encode_header();
            write(msg);
            return;
        }
        for (auto neighbor: neighbors.neighbor_timer_map) {
            memcpy(msg.body(), &rreq, msg.body_length());
            msg.sendid(id());
            msg.receiveid(neighbor.first);
            msg.sourceid(id());
            msg.destid(neighbor.first);
            msg.encode_header();
            write(msg);
        }
    }

    void forward_rrep(ad_hoc_message &msg, int next_hop) {
        msg.sendid(id());
        msg.receiveid(next_hop);
        msg.encode_header();
        write(msg);
    }

    void broadcast_rerr(ad_hoc_message &msg) {
        if (neighbors.neighbor_timer_map.empty()) {
            msg.sendid(id());
            msg.receiveid(AODV_BROADCAST_ADDRESS);
            msg.sourceid(id());
            msg.destid(AODV_BROADCAST_ADDRESS);
            write(msg);
            return;
        }
        for (auto neighbor: neighbors.neighbor_timer_map) {
            msg.sendid(id());
            msg.receiveid(neighbor.first);
            msg.sourceid(id());
            msg.destid(neighbor.first);
            write(msg);
        }
    }

    void broadcast_back(ad_hoc_message &msg) {
        ad_hoc_aodv_back back{AODV_BACK, msg.sendid(), msg.receiveid(), msg.sourceid(), msg.destid()};
        for (auto neighbor: neighbors.neighbor_timer_map) {
            ad_hoc_message broadcast_msg;
            broadcast_msg.msg_type(AODV_MESSAGE);
            broadcast_msg.body_length(sizeof(back));
            memcpy(broadcast_msg.body(), &back, broadcast_msg.body_length());
            broadcast_msg.sendid(id());
            broadcast_msg.receiveid(neighbor.first);
            broadcast_msg.sourceid(id());
            broadcast_msg.destid(neighbor.first);
            broadcast_msg.encode_header();
            write(broadcast_msg);
        }
    }

    void send_rreq(int dest, int dest_seq) {
        this->aodv_rreq_id += 1;
        this->aodv_seq += 1;
        ad_hoc_message msg;
        msg.sourceid(id());
        msg.destid(dest);
        msg.msg_type(AODV_MESSAGE);
        ad_hoc_aodv_rreq rreq{AODV_RREQ, 0, this->aodv_rreq_id, dest, dest_seq, id(), this->aodv_seq};
        msg.body_length(sizeof(rreq));
        memcpy(msg.body(), &rreq, msg.body_length());
        broadcast_rreq(msg, rreq);
        //启动一个定时器，记录rreq在缓存中的存活时间，在存活时间之内的相同source和id的rreq都会被丢弃
        auto timer = rreq_buffer.new_timer(io_context, id(), rreq.id);
        timer->async_wait(boost::bind(&ad_hoc_client::aodv_path_discovery_timeout, this, id(), rreq.id));
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
        ad_hoc_aodv_rerr rerr{AODV_RERR, dest, dest_seq};
        msg.body_length(sizeof(rerr));
        msg.msg_type(AODV_MESSAGE);
        memcpy(msg.body(), &rerr, msg.body_length());
        msg.encode_header();
        broadcast_rerr(msg);
//        write(msg);
    }

    void send_hello() {
        if (neighbors.empty()) {
            ad_hoc_message msg;
            msg.sendid(id());
            msg.receiveid(AODV_BROADCAST_ADDRESS);
            msg.sourceid(id());
            msg.destid(AODV_BROADCAST_ADDRESS);
            ad_hoc_aodv_hello hello{AODV_HELLO};
            msg.body_length(sizeof(hello));
            msg.msg_type(AODV_MESSAGE);
            memcpy(msg.body(), &hello, msg.body_length());
            msg.encode_header();
            write(msg);
        } else {
            for (auto neighbor: neighbors.neighbor_timer_map) {
                ad_hoc_message msg;
                msg.sendid(id());
                msg.receiveid(neighbor.first);
                msg.sourceid(id());
                msg.destid(neighbor.first);
                ad_hoc_aodv_hello hello{AODV_HELLO};
                msg.body_length(sizeof(hello));
                msg.msg_type(AODV_MESSAGE);
                memcpy(msg.body(), &hello, msg.body_length());
                msg.encode_header();
                write(msg);
            }
        }

        hello_timer.expires_from_now(boost::posix_time::seconds(AODV_HELLO_INTERVAL));
        hello_timer.async_wait(boost::bind(&ad_hoc_client::send_hello, this));
    }

    void send_ack(int dest) {
        ad_hoc_aodv_arc arc{AODV_ARC, id(), dest};
        ad_hoc_message msg;
        msg.sendid(id());
        msg.receiveid(dest);
        msg.sourceid(id());
        msg.destid(dest);
        msg.msg_type(AODV_MESSAGE);
        msg.body_length(sizeof(arc));
        memcpy(msg.body(), &arc, msg.body_length());
        msg.encode_header();
        write(msg);
    }

    void aodv_msg_waiting_route_timeout(ad_hoc_message msg, const boost::system::error_code &err) {
        if (err == boost::asio::error::operation_aborted) {
            msg_buffer.remove(msg);
            return;
        }
        print("timeout", msg);
        msg_buffer.remove(msg);
    }

    void aodv_path_discovery_timeout(int node, int rreq_id) {
        if (rreq_buffer.contains(node, rreq_id)) {
            rreq_buffer.remove(node, rreq_id);
        }
    }

    void aodv_route_timeout(ad_hoc_client_routing_table_item &item, const boost::system::error_code &err) {
#if AODV_ROUTE_TIMEOUT
        if (err == boost::asio::error::operation_aborted) {
            return;
        }
        cout << "route_timeout: " << item.dest << endl;
        routing_table_.remove(item.dest);
#endif
    }

    void aodv_neighbor_timeout(int neighbor, const boost::system::error_code &err) {
#if AODV_NEIGHBOR_TIMEOUT
        if (err == boost::asio::error::operation_aborted || watchdog.is_wormhole(neighbor)) {
            return;
        }
        cout << "neighbor: " << neighbor << " timeout" << endl;
        auto dest_seq = routing_table_.route(neighbor).seq;
        routing_table_.remove(neighbor);
        neighbors.remove(neighbor);
        send_rerr(neighbor, dest_seq);
        send_rreq(neighbor, dest_seq + 1);
#endif
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
    ad_hoc_aodv_rreq_buffer rreq_buffer;
    ad_hoc_aodv_message_buffer msg_buffer;
    ad_hoc_aodv_neighbor_list neighbors;
    ad_hoc_wormhole_watchdog watchdog;
    boost::asio::deadline_timer hello_timer;
    int aodv_seq;
    int aodv_rreq_id;

    ad_hoc_wormhole_client *wormhole_client;
    int wormhole;
};

#endif //ADHOC_SIMULATION_CLIENT_H
