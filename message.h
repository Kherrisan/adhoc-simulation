//
// Created by 邹迪凯 on 2021/10/28.
//

#ifndef ADHOC_SIMULATION_MESSAGE_H
#define ADHOC_SIMULATION_MESSAGE_H

const int ADHOCMESSAGE_HEADER_LENGTH = 16; // receive id & send id & body & type
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
        return ADHOCMESSAGE_HEADER_LENGTH + body_length_ ;
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
    int receiveid() const {
        return receive_id;
    }

    void receiveid(int id) {
        receive_id = id;
    }

    //发送的ID号码
    int sendid() const{
        return send_id;
    }

    void sendid(int id) {
        send_id = id;
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
        memcpy(data_ + sizeof(send_id) + sizeof(receive_id), &body_length_, sizeof(int));
        memcpy(data_ + sizeof(send_id) + sizeof(receive_id) + sizeof(body_length_), &msg_type_, sizeof(int));
    }

    /**
     * 解码消息首部
     *
     * @return
     */
    bool decode_header() {
        //根据协议中各个字段的偏移，进行解码
        send_id = *reinterpret_cast<int *>(data_);
        receive_id = *reinterpret_cast<int *>(data_+ sizeof(send_id));
        body_length_ = *reinterpret_cast<int *>(data_ + 8);
        msg_type_=*reinterpret_cast<int *>(data_ + 8 + sizeof(body_length_));
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
    int send_id;
    int receive_id;
    int msg_type_;  // ord=0 rreq=1 rrep=2 rerr=3
    size_t body_length_;
};

#endif //ADHOC_SIMULATION_MESSAGE_H
