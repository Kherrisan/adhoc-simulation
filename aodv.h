//
// Created by 54571 on 2021/11/15.
//

#ifndef ADHOC_SIMULATION_AODV_H
#define ADHOC_SIMULATION_AODV_H

#include <string>
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

    void erase(int id) {
        routes.erase(id);
    }

    int size() {
        return routes.size();
    }

    void print() {
        cout << "routing table: " << endl;
        for (auto route: routes) {
            cout << route.first << ": " << route.second.next_hop << ", " << route.second.seq << ", "
                 << route.second.hops << endl;
        }
    }

private:
    unordered_map<int, ad_hoc_client_routing_table_item> routes;
};

const int AODV_PATH_DISCOVERY_TIMEOUT = 10;
const int AODV_HELLO_TIMEOUT = 30;

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

    boost::asio::deadline_timer *setup_path_discovery_timer(boost::asio::io_context &io_context, int src, int id) {
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

    boost::asio::deadline_timer *setup_neighbor_timer(boost::asio::io_context &io_context, int neighbor) {
        auto timer = new boost::asio::deadline_timer(io_context, boost::posix_time::seconds(AODV_HELLO_TIMEOUT));
        neighbor_timer_map[neighbor] = timer;
        return timer;
    }

    unordered_map<int, boost::asio::deadline_timer *> neighbor_timer_map;
};

#endif //ADHOC_SIMULATION_AODV_H
