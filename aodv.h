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

private:
    unordered_map<int, ad_hoc_client_routing_table_item> routes;
};

const int AODV_PATH_DISCOVERY_TIME = 10;

class ad_hoc_aodv_rreq_buffer {
public:

    bool contains(int src, int id) {
        ad_hoc_aodv_rreq rreq{};
        rreq.orig = src;
        rreq.id = id;
        return path_discovery_timers.find(rreq) != path_discovery_timers.end();
    }

    void discard(int src, int id) {
        ad_hoc_aodv_rreq rreq{};
        rreq.orig = src;
        rreq.id = id;
        auto timer = path_discovery_timers[rreq];
        delete timer;
        path_discovery_timers.erase(rreq);
    }

    boost::asio::deadline_timer *setup_path_discovery_timer(boost::asio::io_context &io_context, int src, int id) {
        ad_hoc_aodv_rreq rreq{};
        rreq.orig = src;
        rreq.id = id;
        auto timer = new boost::asio::deadline_timer(io_context, boost::posix_time::seconds(AODV_PATH_DISCOVERY_TIME));
        path_discovery_timers[rreq] = timer;
        return timer;
    }

private:
    unordered_map<ad_hoc_aodv_rreq, boost::asio::deadline_timer *> path_discovery_timers;
};

#endif //ADHOC_SIMULATION_AODV_H
