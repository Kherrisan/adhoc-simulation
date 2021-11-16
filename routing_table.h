//
// Created by 54571 on 2021/11/15.
//

#ifndef ADHOC_SIMULATION_ROUTING_TABLE_H
#define ADHOC_SIMULATION_ROUTING_TABLE_H
#include <string>
#include <deque>
#include <set>
using boost::asio::ip::tcp;
using namespace std;

class ad_hoc_client_routing_table{
public:
    ad_hoc_client_routing_table() : next_ip(){

    }

    void insert_node(int next_id){
        next_ip.insert(next_id);
    }

    void erase_node(int ese_id)
    {
        for (set<int>::iterator it=next_ip.begin(); it!=next_ip.end(); it++)
        {
            if(*it==ese_id) next_ip.erase(it);
        }
    }

    int table_size()
    {
        return next_ip.size();
    }

private:
    set<int> next_ip;
};
#endif //ADHOC_SIMULATION_ROUTING_TABLE_H
