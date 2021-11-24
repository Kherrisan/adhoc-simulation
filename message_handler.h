//
// Created by 邹迪凯 on 2021/11/24.
//

#ifndef ADHOC_SIMULATION_MESSAGE_HANDLER_H
#define ADHOC_SIMULATION_MESSAGE_HANDLER_H

#include "message.h"

class ad_hoc_message_handler {
public:
    virtual void handle_message(ad_hoc_message &, bool) = 0;
};

#endif //ADHOC_SIMULATION_MESSAGE_HANDLER_H
