//
// Created by 54571 on 2021/11/15.
//

#ifndef ADHOC_SIMULATION_AODV_H
#define ADHOC_SIMULATION_AODV_H

#include <string>
#include <iomanip>
#include <unordered_map>
#include <deque>
#include <set>
#include "boost/functional/hash.hpp"
#include "boost/asio.hpp"
#include <memory>

typedef shared_ptr<boost::asio::deadline_timer> timer_ptr;

using boost::asio::ip::tcp;
using namespace std;

const int AODV_RREQ = 1;
const int AODV_RREP = 2;
const int AODV_RERR = 3;
const int AODV_HELLO = 4;

const int AODV_PATH_DISCOVERY_TIMEOUT = 30;
const int AODV_HELLO_TIMEOUT = 30;
const int AODV_MESSAGE_WAITING_ROUTE_TIMEOUT = 5;

struct ad_hoc_aodv_rreq {
    int type;
    int hops;
    int id;
    int dest;
    int dest_seq;
    int orig;
    int orig_seq;

    bool operator==(const ad_hoc_aodv_rreq &r) const {
        return orig == r.orig && id == r.id;
    }
};

namespace std {
    template<>
    struct hash<ad_hoc_aodv_rreq> {
        size_t operator()(const ad_hoc_aodv_rreq &r) const noexcept {
            auto t = make_tuple(r.orig, r.id);
            return boost::hash_value(t);
        }
    };

    template<>
    struct hash<ad_hoc_message> {
        size_t operator()(const ad_hoc_message &r) const noexcept {
            auto t = make_tuple(r.sourceid(), r.destid(), r.sendid(), r.receiveid(), r.msg_type(), r.length());
            return boost::hash_value(t);
        }
    };
}

struct ad_hoc_aodv_rrep {
    int type;
    int hops;
    int dest;
    int dest_seq;
    int orig;
    int lifetime;
};

struct ad_hoc_aodv_rerr {
    int type;
    int dest;
    int dest_seq;
};

struct ad_hoc_aodv_hello {
    int type;
};

class ad_hoc_client_routing_table_item {
public:
    int dest;
    int next_hop;
    int seq;
    int hops;
    timer_ptr timer;
};

class ad_hoc_client_routing_table {
public:
    ad_hoc_client_routing_table() {

    }

    ad_hoc_client_routing_table_item &route(int id) {
        return routes[id];
    }

    bool contains(int id) {
        if (id == AODV_BROADCAST_ADDRESS) {
            return true;
        }
        return routes.find(id) != routes.end();
    }

    void insert(ad_hoc_client_routing_table_item route) {
        routes[route.dest] = route;
    }

    void remove(int id) {
        routes.erase(id);
    }

    int size() {
        return routes.size();
    }

    void print() {
        cout << left
             << setw(8) << "dest" << "|"
             << setw(8) << "next" << "|"
             << setw(8) << "seq" << "|"
             << setw(8) << "hops" << endl;
        for (int i = 0; i < 32; i++) {
            cout << "-";
        }
        cout << endl;
        for (const auto &route: routes) {
            cout << setw(8) << route.first << "|"
                 << setw(8) << route.second.next_hop << "|"
                 << setw(8) << route.second.seq << "|"
                 << setw(8) << route.second.hops << endl;
        }
        cout << endl;
    }

private:
    unordered_map<int, ad_hoc_client_routing_table_item> routes;
};

class ad_hoc_aodv_rreq_buffer {
public:
    bool contains(int src, int id) {
        ad_hoc_aodv_rreq rreq{};
        rreq.orig = src;
        rreq.id = id;
        return rreq_timer_map.find(rreq) != rreq_timer_map.end();
    }

    void remove(int src, int id) {
        ad_hoc_aodv_rreq rreq{};
        rreq.orig = src;
        rreq.id = id;
        rreq_timer_map.erase(rreq);
    }

    timer_ptr new_timer(boost::asio::io_context &io_context, int src, int id) {
        ad_hoc_aodv_rreq rreq{};
        rreq.orig = src;
        rreq.id = id;
        rreq_timer_map[rreq] = make_shared<boost::asio::deadline_timer>(io_context, boost::posix_time::seconds(
                AODV_PATH_DISCOVERY_TIMEOUT));
        return rreq_timer_map[rreq];
    }

private:
    unordered_map<ad_hoc_aodv_rreq, timer_ptr> rreq_timer_map;
};

class ad_hoc_aodv_message_buffer {
public:

    bool contains(ad_hoc_message &msg) {
        return msg_timer_map.find(msg) != msg_timer_map.end();
    }

    void remove(ad_hoc_message &msg) {
        msg_timer_map.erase(msg);
    }

    timer_ptr new_timer(boost::asio::io_context &io_context, ad_hoc_message &msg) {
        msg_timer_map[msg] = make_shared<boost::asio::deadline_timer>(io_context, boost::posix_time::seconds(
                AODV_MESSAGE_WAITING_ROUTE_TIMEOUT));
        return msg_timer_map[msg];
    }

    unordered_map<ad_hoc_message, timer_ptr> msg_timer_map;
};

class ad_hoc_aodv_neighbor_list {
public:
    bool contains(int neighbor) {
        return neighbor_timer_map.find(neighbor) != neighbor_timer_map.end();
    }

    void remove(int neighbor) {
        auto timer = neighbor_timer_map[neighbor];
        if (timer != nullptr) {
            timer->cancel();
        }
        neighbor_timer_map.erase(neighbor);
    }

    timer_ptr timer(int neighbor) {
        return neighbor_timer_map[neighbor];
    }

    timer_ptr new_timer(boost::asio::io_context &io_context, int neighbor) {
        neighbor_timer_map[neighbor] = make_shared<boost::asio::deadline_timer>(io_context);
        return neighbor_timer_map[neighbor];
    }

    void print() {
        cout << left << setw(8) << "neighbor" << endl;
        for (int i = 0; i < 8; i++) {
            cout << "-";
        }
        cout << endl;
        for (const auto &neighbor: neighbor_timer_map) {
            cout << setw(8) << neighbor.first << "|" << endl;
        }
        cout << endl;
    }

private:
    unordered_map<int, timer_ptr> neighbor_timer_map;
};

void print_aodv(const char *data) {
    auto type = (int *) data;
    switch (*type) {
        case AODV_RREQ: {
            auto rreq = (ad_hoc_aodv_rreq *) data;
            cout << "[rreq] hops: " << rreq->hops << ", id: " << rreq->id << ", dest: " << rreq->dest << ", dest_seq: "
                 << rreq->dest_seq << ", orig: " << rreq->orig << ", orig_seq: " << rreq->orig_seq << endl;
            break;
        }
        case AODV_RREP: {
            auto rrep = (ad_hoc_aodv_rrep *) data;
            cout << "[rrep] hops: " << rrep->hops << ", dest: " << rrep->dest << ", dest_seq: "
                 << rrep->dest_seq << ", orig: " << rrep->orig << endl;
            break;
        }
        case AODV_RERR: {
            auto rerr = (ad_hoc_aodv_rerr *) data;
            cout << "[rerr] dest: " << rerr->dest << ", dest_seq: "
                 << rerr->dest_seq << endl;
            break;
        }
        case AODV_HELLO:
            cout << "[hello]" << endl;
            break;
    }
}

#endif //ADHOC_SIMULATION_AODV_H
