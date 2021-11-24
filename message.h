//
// Created by 邹迪凯 on 2021/10/28.
//

#ifndef ADHOC_SIMULATION_MESSAGE_H
#define ADHOC_SIMULATION_MESSAGE_H

#include <iostream>

using namespace std;

const int ADHOCMESSAGE_HEADER_LENGTH = 24; // receive id & send id & body & type & source & dest
const int ADHOCMESSAGE_MAX_BODY_LENGTH = 1024;

const int AODV_BROADCAST_ADDRESS = 0;

const int ORDINARY_MESSAGE = 0;
const int AODV_MESSAGE = 1;
const int WORMHOLE_MESSAGE = 2;

// message : sendid -> receiveid -> sourceid -> destid -> body -> type

class ad_hoc_message {
public:
    ad_hoc_message() : body_length_(0) {

    }

    /**
     * 字节数组的首地址，也是消息首部的首地址
     *
     * @return
     */
    char *data() {
        return data_;
    }

    /**
     * 消息中数据载荷的起始地址
     *
     * @return
     */
    char *body() {
        return data_ + ADHOCMESSAGE_HEADER_LENGTH;
    };

    /**
     * 消息总长度，包括消息首部长度和载荷长度
     *
     * @return
     */
    size_t length() const {
        return ADHOCMESSAGE_HEADER_LENGTH + body_length_;
    }

    /**
     * 返回数据载荷的长度
     *
     * @return
     */
    int body_length() {
        return body_length_;
    }

    /**
     * 设置数据载荷长度
     *
     * @param length
     */
    void body_length(int length) {
        body_length_ = length;
    }

    //接收的ID号码
    int receiveid() const {
        return receive_id;
    }

    void receiveid(int id) {
        receive_id = id;
    }

    //发送的ID号码
    int sendid() const {
        return send_id;
    }

    void sendid(int id) {
        send_id = id;
    }

    int sourceid() const {
        return source_id;
    }

    void sourceid(int id) {
        source_id = id;
    }

    int destid() const {
        return dest_id;
    }

    void destid(int id) {
        dest_id = id;
    }

    int msg_type() const {
        return msg_type_;
    }

    void msg_type(int msgtype) {
        msg_type_ = msgtype;
    }

    /**
     * 编码消息首部
     */
    void encode_header() {
        //根据协议中各个字段的偏移，进行编码
        memcpy(data_, &send_id, sizeof(send_id));
        memcpy(data_ + sizeof(send_id), &receive_id, sizeof(receive_id));
        memcpy(data_ + sizeof(send_id) + sizeof(receive_id), &source_id, sizeof(int));
        memcpy(data_ + sizeof(send_id) + sizeof(receive_id) + sizeof(source_id), &dest_id, sizeof(int));
        memcpy(data_ + sizeof(send_id) + sizeof(receive_id) + sizeof(source_id) + sizeof(dest_id), &body_length_,
               sizeof(int));
        memcpy(data_ + sizeof(send_id) + sizeof(receive_id) + sizeof(source_id) + sizeof(dest_id) +
               sizeof(body_length_), &msg_type_, sizeof(int));
    }

    /**
     * 解码消息首部
     *
     * @return
     */
    bool decode_header() {
        //根据协议中各个字段的偏移，进行解码
        send_id = *reinterpret_cast<int *>(data_);
        receive_id = *reinterpret_cast<int *>(data_ + 4);
        source_id = *reinterpret_cast<int *>(data_ + 8);
        dest_id = *reinterpret_cast<int *>(data_ + 12);
        body_length_ = *reinterpret_cast<int *>(data_ + 16);
        msg_type_ = *reinterpret_cast<int *>(data_ + 20);
        if (body_length_ < 0 || body_length_ > ADHOCMESSAGE_MAX_BODY_LENGTH) {
            body_length_ = 0;
            return false;
        }
        return true;
    }

    bool operator==(const ad_hoc_message &r) const {
        return source_id == r.source_id && dest_id == r.dest_id;
    }

private:
    //存放消息首部和载荷的字节数组
    char data_[ADHOCMESSAGE_HEADER_LENGTH + ADHOCMESSAGE_MAX_BODY_LENGTH];

    //消息首部各字段，未来可以在此处添加字段
    int send_id;
    int receive_id;
    int source_id;
    int dest_id;
    int msg_type_;  // ord=0 aodv=1
    int body_length_;
};

void print_message(ad_hoc_message &msg) {
    cout << "[message] src: " << msg.sourceid() << ", dst: " << msg.destid() << ", sender: " << msg.sendid()
         << ", receiver: " << msg.receiveid() << ", type: " << msg.msg_type() << endl;
}

#endif //ADHOC_SIMULATION_MESSAGE_H
