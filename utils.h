//
// Created by 邹迪凯 on 2021/11/24.
//

#ifndef ADHOC_SIMULATION_UTILS_H
#define ADHOC_SIMULATION_UTILS_H

#include "message.h"

#define DEBUG true
#define DYNAMIC true

void print_time() {
    time_t now = time(nullptr);
    tm *ltm = localtime(&now);
    char loc_date[20];
    sprintf(loc_date, "%d:%02d:%02d", ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
    cout << loc_date << endl;
}

void print(ad_hoc_message &msg) {
    if (msg.msg_type() == AODV_MESSAGE && *(int *) (msg.body()) == AODV_HELLO) {
        return;
    }
    print_time();
    if (msg.msg_type() == ORDINARY_MESSAGE) {
        cout << "[message] src: " << msg.sourceid() << ", dst: " << msg.destid() << ", sender: " << msg.sendid()
             << ", receiver: " << msg.receiveid() << ", type: " << msg.msg_type() << endl;
        cout << "[user_message] ";
        cout.write(msg.body(), msg.body_length());
        cout << endl;
        cout << endl;
    } else if (msg.msg_type() == AODV_MESSAGE) {
#if DEBUG
        cout << "[message] src: " << msg.sourceid() << ", dst: " << msg.destid() << ", sender: " << msg.sendid()
             << ", receiver: " << msg.receiveid() << ", type: " << msg.msg_type() << endl;
        print_aodv(msg.body());
        cout << endl;
#endif
    } else {
#if DEBUG
        cout << "[wormhole] src: " << msg.sourceid() << ", dst: " << msg.destid() << ", sender: " << msg.sendid()
             << ", receiver: " << msg.receiveid() << ", type: " << msg.msg_type() << endl;
        ad_hoc_message body_msg;
        memcpy(body_msg.data(), msg.body(), msg.body_length());
        body_msg.decode_header();
        print(body_msg);
#endif
    }
}

#endif //ADHOC_SIMULATION_UTILS_H
