//
// Created by 邹迪凯 on 2021/10/28.
//

#ifndef ADHOC_SIMULATION_MESSAGE_H
#define ADHOC_SIMULATION_MESSAGE_H

const int ADHOCMESSAGE_HEADER_LENGTH = 8;
const int ADHOCMESSAGE_MAX_BODY_LENGTH = 1024;

class ad_hoc_message {
public:
    ad_hoc_message() : body_length_(0) {

    }

    char *data() {
        return data_;
    }

    char *body() {
        return data_ + ADHOCMESSAGE_HEADER_LENGTH;
    };

    size_t length() const {
        return ADHOCMESSAGE_HEADER_LENGTH + body_length_;
    }

    size_t body_length() {
        return body_length_;
    }

    void body_length(size_t length) {
        body_length_ = length;
    }

    int id() const {
        return id_;
    }

    void id(int id) {
        id_ = id;
    }

    void encode_header() {
        memcpy(data_, &id_, sizeof(id_));
        memcpy(data_ + sizeof(id_), &body_length_, sizeof(int));
    }

    bool decode_header() {
        id_ = *reinterpret_cast<int *>(data_);
        body_length_ = *reinterpret_cast<int *>(data_ + 4);
        if (body_length_ < 0 || body_length_ > ADHOCMESSAGE_MAX_BODY_LENGTH) {
            body_length_ = 0;
            return false;
        }
        return true;
    }

private:
    char data_[ADHOCMESSAGE_HEADER_LENGTH + ADHOCMESSAGE_MAX_BODY_LENGTH];
    int id_;
    size_t body_length_;
};

#endif //ADHOC_SIMULATION_MESSAGE_H
