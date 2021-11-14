//
// Created by 邹迪凯 on 2021/10/28.
//

#ifndef ADHOC_SIMULATION_MESSAGE_H
#define ADHOC_SIMULATION_MESSAGE_H

const int ADHOCMESSAGE_HEADER_LENGTH = 12;
const int ADHOCMESSAGE_MAX_BODY_LENGTH = 1024;

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
    size_t body_length() {
        return body_length_;
    }

    /**
     * 设置数据载荷长度
     *
     * @param length
     */
    void body_length(size_t length) {
        body_length_ = length;
    }
    //接收的ID号码
    int receiver_id() const {
        return receiver_id_;
    }

    void receiver_id(int id) {
        receiver_id_ = id;
    }

    //发送的ID号码
    int sender_id() const{
        return sender_id_;
    }

    void sender_id(int id) {
        sender_id_ = id;
    }

    /**
     * 编码消息首部
     */
    void encode_header() {
        //根据协议中各个字段的偏移，进行编码
        memcpy(data_, &sender_id_, sizeof(sender_id_));
        memcpy(data_ + sizeof(sender_id_), &receiver_id_, sizeof(receiver_id_));
        memcpy(data_ + sizeof(sender_id_) + sizeof(receiver_id_), &body_length_, sizeof(int));
    }

    /**
     * 解码消息首部
     *
     * @return
     */
    bool decode_header() {
        //根据协议中各个字段的偏移，进行解码
        sender_id_ = *reinterpret_cast<int *>(data_);
        receiver_id_ = *reinterpret_cast<int *>(data_ + sizeof(sender_id_));
        body_length_ = *reinterpret_cast<int *>(data_ + sizeof(sender_id_) + sizeof(receiver_id_));
        if (body_length_ < 0 || body_length_ > ADHOCMESSAGE_MAX_BODY_LENGTH) {
            body_length_ = 0;
            return false;
        }
        return true;
    }

private:
    //存放消息首部和载荷的字节数组
    char data_[ADHOCMESSAGE_HEADER_LENGTH + ADHOCMESSAGE_MAX_BODY_LENGTH];

    //消息首部各字段，未来可以在此处添加字段
    int sender_id_;
    int receiver_id_;
    size_t body_length_;
};

#endif //ADHOC_SIMULATION_MESSAGE_H
