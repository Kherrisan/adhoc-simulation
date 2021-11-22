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

using boost::asio::ip::tcp;
using namespace std;

const int AODV_RREQ = 1;
const int AODV_RREP = 2;
const int AODV_RERR = 3;
const int AODV_HELLO = 4;

const int AODV_PATH_DISCOVERY_TIMEOUT = 30;
const int AODV_HELLO_TIMEOUT = 30;
const int AODV_ROUTE_NOT_FOUND_TIMEOUT = 5;
const int AODV_ROUTE_TIMEOUT = 30;

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
            auto t = make_tuple(r.sourceid(), r.destid());
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
    boost::asio::deadline_timer *timer;
};

class ad_hoc_client_routing_table {
public:
    ad_hoc_client_routing_table() {

    }

    ad_hoc_client_routing_table_item &route(int id) {
        return routes[id];
    }

    bool contains(int id) {
        if (id == 0) {
            return true;
        }
        return routes.find(id) != routes.end();
    }

    void insert(ad_hoc_client_routing_table_item route) {
        routes[route.dest] = route;
    }

    void discard(int dest) {
        if (!contains(dest)) {
            return;
        }
        auto route = routes[dest];
        route.timer->cancel();
        delete route.timer;
        routes.erase(dest);
    }

    int size() {
        return routes.size();
    }

    void print() {
        cout << left << setw(8) << "dest" << "|"
             << left << setw(8) << "next" << "|"
             << left << setw(8) << "seq" << "|"
             << left << setw(8) << "hops" << endl;
        for (auto route: routes) {
            cout << left << setw(8) << route.first << "|"
                 << left << setw(8) << route.second.next_hop << "|"
                 << left << setw(8) << route.second.seq << "|"
                 << left << setw(8) << route.second.hops << endl;
        }
        cout << endl;
    }

    boost::asio::deadline_timer *setup_timer(boost::asio::io_context &io_context, int dest) {
        if (!contains(dest)) {
            return nullptr;
        }
        auto route = routes[dest];
        route.timer = new boost::asio::deadline_timer(io_context,
                                                      boost::posix_time::seconds(AODV_ROUTE_TIMEOUT));
        return route.timer;
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

    void discard(int src, int id) {
        ad_hoc_aodv_rreq rreq{};
        rreq.orig = src;
        rreq.id = id;
        auto timer = rreq_timer_map[rreq];
        delete timer;
        rreq_timer_map.erase(rreq);
    }

    boost::asio::deadline_timer *setup_timer(boost::asio::io_context &io_context, int src, int id) {
        ad_hoc_aodv_rreq rreq{};
        rreq.orig = src;
        rreq.id = id;
        auto timer = new boost::asio::deadline_timer(io_context,
                                                     boost::posix_time::seconds(AODV_PATH_DISCOVERY_TIMEOUT));
        rreq_timer_map[rreq] = timer;
        return timer;
    }

private:
    unordered_map<ad_hoc_aodv_rreq, boost::asio::deadline_timer *> rreq_timer_map;
};

class ad_hoc_message_buffer {
public:
    bool contains(int src, int dest) {
        ad_hoc_message msg;
        msg.sourceid(src);
        msg.destid(dest);
        return msg_timer_map.find(msg) != msg_timer_map.end();
    }

    void discard(int src, int dest) {
        ad_hoc_message msg;
        msg.sourceid(src);
        msg.destid(dest);
        auto timer = msg_timer_map[msg];
        timer->cancel();
        delete timer;
        msg_timer_map.erase(msg);
    }

    boost::asio::deadline_timer *setup_timer(boost::asio::io_context &io_context, int src, int dest) {
        ad_hoc_message msg;
        msg.sourceid(src);
        msg.destid(dest);
        auto timer = new boost::asio::deadline_timer(io_context,
                                                     boost::posix_time::seconds(AODV_ROUTE_NOT_FOUND_TIMEOUT));
        msg_timer_map[msg] = timer;
        return timer;
    }

private:
    unordered_map<ad_hoc_message, boost::asio::deadline_timer *> msg_timer_map;
};

class ad_hoc_aodv_neighbor_list {
public:
    bool contains(int neighbor) {
        return neighbor_timer_map.find(neighbor) != neighbor_timer_map.end();
    }

    void discard(int neighbor) {
        auto timer = neighbor_timer_map[neighbor];
        timer->cancel();
        delete timer;
        neighbor_timer_map.erase(neighbor);
    }

    boost::asio::deadline_timer *timer(int neighbor) {
        return neighbor_timer_map[neighbor];
    }

    boost::asio::deadline_timer *setup_timer(boost::asio::io_context &io_context, int neighbor) {
        auto timer = new boost::asio::deadline_timer(io_context, boost::posix_time::seconds(AODV_HELLO_TIMEOUT));
        neighbor_timer_map[neighbor] = timer;
        return timer;
    }

    unordered_map<int, boost::asio::deadline_timer *> neighbor_timer_map;
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
