//
// Created by 邹迪凯 on 2021/11/14.
//

#ifndef ADHOC_SIMULATION_WORMHOLE_H
#define ADHOC_SIMULATION_WORMHOLE_H

#include "server.h"
#include "client.h"

class ad_hoc_wormhole_scope : public ad_hoc_scope {
public:
    ad_hoc_wormhole_scope() {
        w1_index = w2_index = -1;
        w1_id = w2_id = -1;
    }

    void setup() {
        ad_hoc_scope::setup();
        setup_wormhole();
    }

    void setup_wormhole() {
        auto cycle = find_longest_cycle();
        cout << "find longest cycle: ";
        for (auto n: *cycle) {
            cout << n << ",";
        }
        cout << endl;
        cout << "choose " << cycle->front() << " as the sender, " << cycle->at(1) << ", " << cycle->at(2)
             << " as the wormhole, " << cycle->at(3) << " as the receiver." << endl;
        cout << "the first path is ";
        for (int i = 0; i < 4; i++) {
            cout << cycle->at(i) << "->";
        }
        cout << endl;
        cout << "the second path is ";
        cout << cycle->front() << "->";
        for (int i = cycle->size() - 1; i > 3; i--) {
            cout << cycle->at(i) << "->";
        }
        cout << cycle->at(3) << endl;
        w1_index = cycle->at(1);
        w2_index = cycle->at(2);
    }

    vector<int> *find_longest_cycle() {
        auto longest_cycle = new vector<int>;
        auto *trace = new vector<int>;
        bool *visited = new bool[MAX];
        memset(visited, false, MAX);
        find_longest_cycle(trace, 0, visited, longest_cycle);
        delete[] visited;
        delete trace;
        return longest_cycle;
    }

    void find_longest_cycle(vector<int> *stack, int node, bool *visited, vector<int> *longest_cycle) {
        stack->push_back(node);
        visited[node] = true;
        for (int i = 0; i < MAX; i++) {
            if (node == i || !matrix[node][i]) {
                continue;
            }
            if (!visited[i]) {
                find_longest_cycle(stack, i, visited, longest_cycle);
            } else {
                for (int j = 0; j < stack->size(); j++) {
                    if (stack->at(j) == i) {
                        if (stack->size() - j <= longest_cycle->size()) {
                            break;
                        }
                        longest_cycle->clear();
                        for (int k = j; k < stack->size(); k++) {
                            longest_cycle->push_back(stack->at(k));
                        }
                        break;
                    }
                }
            }
        }
        stack->pop_back();
        visited[node] = false;
    }

    void join(int id, ad_hoc_participant_ptr participant) {
        session_map[id] = participant;

        static int i = 0;
        if (i >= 0) {
            node[i] = id;
            i++;
        }           //每进来一个ID号就作为邻接矩阵的顶点

        if (i - 1 == w1_index) {
            w1_id = id;
        } else if (i - 1 == w2_index) {
            w2_id = id;
        }

        char *body = "wormhole";

        if (w1_id != -1 && w2_id != -1) {
            ad_hoc_message msg_to_w1;
            msg_to_w1.sender_id(w2_id);
            msg_to_w1.receiver_id(w1_id);
            memcpy(msg_to_w1.body(), body, 8);
            deliver(msg_to_w1);

            ad_hoc_message msg_to_w2;
            msg_to_w1.sender_id(w1_id);
            msg_to_w1.receiver_id(w2_id);
            memcpy(msg_to_w2.body(), body, 8);
            deliver(msg_to_w2);
        }
    }

private:
    int w1_index;
    int w2_index;
    int w1_id;
    int w2_id;
};

class ad_hoc_wormhole_client {

};

#endif //ADHOC_SIMULATION_WORMHOLE_H
