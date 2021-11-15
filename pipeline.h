//
// Created by 邹迪凯 on 2021/11/10.
//

#ifndef ADHOC_SIMULATION_PIPELINE_H
#define ADHOC_SIMULATION_PIPELINE_H

#include <vector>

#include "boost/asio.hpp"
#include "message.h"

using namespace std;

class ad_hoc_client_context;

class ad_hoc_client_handler {
public:
    enum direction {
        INBOUND, OUTBOUND
    };

    ad_hoc_client_handler(direction direction) : direction(direction) {}

    direction direction;
};

class ad_hoc_client_inbound_handler : public ad_hoc_client_handler {
public:
    ad_hoc_client_inbound_handler() : ad_hoc_client_handler(INBOUND) {}

    virtual void read(ad_hoc_client_context &context, void *data, vector<void *> &out) = 0;
};

class ad_hoc_client_outbound_handler : public ad_hoc_client_handler {
public:
    ad_hoc_client_outbound_handler() : ad_hoc_client_handler(OUTBOUND) {}

    virtual void write(ad_hoc_client_context &context, void *data) = 0;
};

class ad_hoc_client_message_decoder : public ad_hoc_client_inbound_handler {
public:
    void read(ad_hoc_client_context &context, void *data, vector<void *> &out) override {
        auto buffer = (boost::asio::mutable_buffers_1 *) data;
        memcpy(accumulator + cursor, buffer->data(), buffer->size());
        cursor += buffer->size();
        if (cursor > ADHOCMESSAGE_HEADER_LENGTH && decode()) {
            out.push_back(&read_msg);
        }
    }

    bool decode() {
        memcpy(read_msg.data(), accumulator, ADHOCMESSAGE_HEADER_LENGTH);
        read_msg.decode_header();
        if (ADHOCMESSAGE_HEADER_LENGTH + read_msg.body_length() > cursor) {
            return false;
        }
        memcpy(read_msg.body(), accumulator + ADHOCMESSAGE_HEADER_LENGTH, read_msg.body_length());
        memmove(accumulator, accumulator + cursor, 1024 - cursor);
        cursor = 0;
        return true;
    }

private:
    char accumulator[1024];
    int cursor;
    ad_hoc_message read_msg;
};

class ad_hoc_client_message_logger : public ad_hoc_client_inbound_handler {
public:
    void read(ad_hoc_client_context &context, void *data, vector<void *> &out) override {
        auto msg = (ad_hoc_message *) data;
        cout << "[debug]" << endl;
    }
};

class ad_hoc_client_context {
public:
    ad_hoc_client_context(ad_hoc_client_handler *handler) : handler(handler) {}

    void read(void *data) {
        vector<void *> out;
        static_cast<ad_hoc_client_inbound_handler *>(handler)->read(*this, data, out);
        if (next == nullptr) {
            return;
        }
        for (auto o: out) {
            next->read(o);
        }
    }

    ad_hoc_client_context *next;
    ad_hoc_client_handler *handler;
private:
};

class ad_hoc_client_pipeline {
public:
    ad_hoc_client_pipeline() {
        inbound = outbound = nullptr;
    }

    void handle_channel_read(char *data, int length) {
        auto buffer = boost::asio::buffer(data, length);
        inbound->read(&buffer);
    }

    ad_hoc_client_pipeline &add(ad_hoc_client_context *chain, ad_hoc_client_handler *handler) {
        if (chain == nullptr) {
            chain = new ad_hoc_client_context(handler);
        } else {
            chain->next = new ad_hoc_client_context(handler);
        }
        return *this;
    }

    ad_hoc_client_pipeline &add(ad_hoc_client_handler *handler) {
        if (handler->direction == ad_hoc_client_handler::INBOUND) {
            return add(inbound, handler);
        } else {
            return add(outbound, handler);
        }
    }

    ad_hoc_client_pipeline &remove(ad_hoc_client_handler *handler) {
        remove(inbound, handler);
        remove(outbound, handler);
        return *this;
    }

    ad_hoc_client_pipeline &remove(ad_hoc_client_context *chain, ad_hoc_client_handler *handler) {
        ad_hoc_client_context *itr = chain;
        if (itr->handler == handler) {
            auto temp = itr->next;
            delete chain;
            chain = temp;
        }
        while (itr->next != nullptr) {
            if (itr->next->handler == handler) {
                auto temp = itr->next->next;
                delete itr->next;
                itr->next = temp;
                return *this;
            }
        }
        return *this;
    }

    ad_hoc_client_pipeline &add(ad_hoc_client_context *chain, int index, ad_hoc_client_handler *handler) {
        auto new_context = new ad_hoc_client_context(handler);
        if (index == 0) {
            new_context->next = chain;
            chain = new_context;
        } else {
            ad_hoc_client_context *itr = chain;
            while (--index > 0) {
                itr = itr->next;
            }
            new_context->next = itr->next;
            itr->next = new_context;
        }
        return *this;
    }

    ad_hoc_client_pipeline &add(int index, ad_hoc_client_handler *handler) {
        if (handler->direction == ad_hoc_client_handler::INBOUND) {
            return add(inbound, index, handler);
        } else {
            return add(outbound, index, handler);
        }
    }

private:
    ad_hoc_client_context *inbound;
    ad_hoc_client_context *outbound;
};

#endif //ADHOC_SIMULATION_PIPELINE_H
