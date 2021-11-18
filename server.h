//
// Created by 邹迪凯 on 2021/10/28.
//

#ifndef ADHOC_SIMULATION_SERVER_H
#define ADHOC_SIMULATION_SERVER_H

#include <string>
#include <boost/array.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <unordered_map>
#include <deque>
#include <ctime>

#define  MAX  8

#include "message.h"
#include "aodv.h"


using boost::asio::ip::tcp;
using namespace std;
//使用deque来实现串型消息队列，主要用于待发送消息队列
typedef deque<ad_hoc_message> message_queue;

void print(ad_hoc_message &msg) {
    cout << "[message] src: " << msg.sourceid() << ", dst: " << msg.destid() << ", sender: " << msg.sendid()
         << ", receiver: " << msg.receiveid() << ", type: " << msg.msg_type() << endl;
    if (msg.msg_type() == ORDINARY_MESSAGE) {
        cout.write(msg.body(), msg.body_length());
        cout << endl;
    } else {
        print_aodv(msg.body());
    }
    cout << endl;
}

class ad_hoc_participant {
public:
    virtual ~ad_hoc_participant() {}

    virtual void deliver(ad_hoc_message &msg) = 0;
};

typedef boost::shared_ptr<ad_hoc_participant> ad_hoc_participant_ptr;


//在session将message传递给scope时，要求转发给消息的接收端
//负责管理多个连接的ad_hoc_session，维护一个ID和session映射关系的哈希表
class ad_hoc_scope {
public:
    void join(int id, ad_hoc_participant_ptr participant) {
        session_map[id] = participant;

        static int i = 0;
        if (i >= 0) {
            node[i] = id;
            i++;
        }           //每进来一个ID号就作为邻接矩阵的顶点
    }

    void Create_MatrixUDG()    //创建网络拓扑图
    {
//        srand(3);
//        for (int column = 0; column < MAX; column++) {
//            for (int low = 0; low < MAX; low++) {
//                matrix[column][low] = 0;
//            }
//        }
//        for (int column = 0; column < MAX; column++) {
//            for (int low = 0; low < MAX; low++) {
//                if (rand() % MAX > (MAX * 1.0 / 1.3f) || column == low) {
//                    matrix[column][low] = 1;
//                    matrix[low][column] = 1;
//                }
//            }
//        }
    }

    void print_UDG()   //打印图
    {
        cout << "The node network topology is as follows: " << endl;
        for (int i = 0; i < MAX; i++) {
            for (int j = 0; j < MAX; j++) {
                cout << matrix[i][j] << " ";

            }
            cout << endl;
        }
    }

    void leave(int id) {
        session_map.erase(id);
    }

    bool judge_deliver(const ad_hoc_message &msg)   //判断是否转发消息
    {
        for (int k = 0; k < MAX; k++) {
            if (node[k] == msg.receiveid())       //msg.receiveid()是要消息要发送到的ID号
            {
                int row = k;
                for (int j = 0; j < MAX; j++) {
                    if (node[j] == msg.sendid())      //msg.sendid()是要发送方的ID号
                    {
                        int column = j;
                        if (matrix[column][row] == 1) {
                            return true;
                        }

                    }
                }

            }
        }
        return false;
    }

    /**
        * scope转发消息函数
        *
        * 在映射表中找到对应的session，交由session进行消息发送
        *
        * @param msg 待转发消息
        * @return
        */
    bool deliver(ad_hoc_message &msg) {
        if (msg.receiveid() == AODV_BROADCAST_ADDRESS) {
            //一跳范围内广播
            cout << "broadcasting" << endl;
            print(msg);
            broadcast(msg);
        } else if (session_map.find(msg.receiveid()) == session_map.end()) { //没有查到相应的ID，就返回错误
            return false;
        } else {
            if (judge_deliver(msg))       //根据网络拓扑图判断是否能转发信息
            {
                cout << "sending" << endl;
                print(msg);
                session_map[msg.receiveid()]->deliver(msg);    //调用ID号对应的session去发送信息
                return true;
            } else {
                reply_error(msg);
                return false;
            }

        }
        return false;
    }

    /**
     * 在 sender 所能直接联通(一跳)的范围内广播该消息
     *
     * @param msg
     */
    void broadcast(ad_hoc_message &msg) {
        for (int i = 0; i < MAX; i++) {
            if (node[i] == msg.sendid()) {
                for (int j = 0; j < MAX; j++) {
                    if (j != i && matrix[i][j] && session_map.find(node[j]) != session_map.end()) {
                        session_map[node[j]]->deliver(msg);
                    }
                }
                break;
            }
        }
    }

    void reply_error(ad_hoc_message &msg) {
        ad_hoc_message err(msg);
        err.msg_type(3);
        err.sendid(msg.receiveid());
        err.receiveid(msg.sendid());
        err.destid(msg.sourceid());
        err.sourceid(msg.destid());
        session_map[msg.sendid()]->deliver(err);
    }

private:
    unordered_map<int, ad_hoc_participant_ptr> session_map;
    int node[MAX];
    int matrix[MAX][MAX] = {
            {1, 0, 1, 0, 1, 0, 0, 1},
            {0, 1, 0, 0, 1, 0, 0, 1},
            {1, 0, 1, 1, 0, 0, 0, 0},
            {0, 0, 1, 1, 1, 0, 0, 0},
            {1, 1, 0, 1, 1, 0, 0, 0},
            {0, 0, 0, 0, 0, 1, 0, 1},
            {0, 0, 0, 0, 0, 0, 1, 1},
            {1, 1, 0, 0, 0, 1, 1, 1}
    };
};


//对于server端的socket连接，每个对象对应一个socket连接
class ad_hoc_session : public boost::enable_shared_from_this<ad_hoc_session>,
                       public ad_hoc_participant {
public:
    /**
    * 构造函数
    *
    * 初始化socket和scope成员
    *
    * @param ioContext 此session未来进行读写操作时，需要维护其IO事件的io_context。应该和server使用同一个io_context。
    * @param scope 此session隶属的scope。
    */
    ad_hoc_session(boost::asio::io_context &ioContext, ad_hoc_scope &scope) : socket_(ioContext), scope(scope) {
    }

    tcp::socket &socket() {
        return socket_;
    }

    /**
     * 启动session接收消息的循环
     */
    void start() {
        //加入隶属的scope
        scope.join(id(), shared_from_this());
        //发起异步的读数据操作，这个读数据操作只负责读取头部，参数：
        //1.socket。和client的连接socket，从该socket的接收缓冲区中读字节数据。
        //2.rreq_timer_map。创建一个地址是read_msg_的数据的起始位置，长度是ADHOCMESSAGE_HEADER_LENGTH的buffer。
        //      当async_read读满了这个buffer（读到了ADHOCMESSAGE_HEADER_LENGTH个字节），则本次读数据完成，会调用回调函数handle_read_header。
        //3.回调函数。通过bind方法绑定了一个参数：this指针，后两个参数是占位符。
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.data(), ADHOCMESSAGE_HEADER_LENGTH),
                                boost::bind(
                                        &ad_hoc_session::handle_read_header,
                                        shared_from_this(),
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
    }

    /**
        * 读消息头部的回调函数
        *
        * 会对消息头部进行解码，并发起读数据载荷（body）的操作。
        *
        * @param error
        * @param bytes_transferred
        */
    void handle_read_header(const boost::system::error_code &error, size_t bytes_transferred) {
        //若没有发生错误，且消息头部的解码成功（符合协议格式）
        if (!error && read_msg_.decode_header()) {
            //此时已经从头部得到了数据载荷的实际长度read_msg_.body_length()
            //创建一个buffer，地址为read_msg_的起点向后偏移HEADER_LENGTH，长度为body_length。
            //当async_read读满此buffer后（读到了body_length个字节），会调用回调函数handle_read_body。
            boost::asio::async_read(socket_,
                                    boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
                                    boost::bind(
                                            &ad_hoc_session::handle_read_body,
                                            shared_from_this(),
                                            boost::asio::placeholders::error));
        }
    }

    /**
        * 读取消息中的数据载荷的回调函数
        *
        * 将message交付给scope进行转发，完成转发后发起下一次异步读操作。
        *
        * @param error
        */
    void handle_read_body(const boost::system::error_code &error) {
        if (!error) {
            cout << "received" << endl;
            print(read_msg_);
            //由scope去查询该message里的目的ID，进行消息转发。
            scope.deliver(read_msg_);
            //发起下一次异步的读操作，等待读取的对象为下一个数据包的首部。
            read_msg_ = ad_hoc_message();
            boost::asio::async_read(socket_,
                                    boost::asio::buffer(read_msg_.data(), ADHOCMESSAGE_HEADER_LENGTH),
                                    boost::bind(
                                            &ad_hoc_session::handle_read_header,
                                            shared_from_this(),
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
        }
    }

    /**
        * 发送数据成功后的回调函数
        *
        * 在session对象发起异步的写数据操作成功后，会回调此函数。
        *
        * @param error
        */
    void handle_write(const boost::system::error_code &error) {
        if (!error) {
            //如果消息发送成功了，就从队列头部删除它。
            write_msgs_.pop_front();
            //如果队列非空，说明还存在待发消息，继续发送。此时队列非空有两种可能：
            //1. 在上一次async_write之前，队列中就已经有超过1个待发消息。
            //2. 在调用async_write但还未完成时，deliver函数又向队列放入了新的待发数据。
            //      此时由于deliver会检查write_in_progress，所以deliver内不会调用async_write。即不会对一个待发消息调用两次async_write。
            //      注意：在deliver检查write_in_progress，以及此处检查write_msgs_.empty()的代码前后，不会发生线程的切换。
            //              实际上这两段代码是在同一个线程上的同一个io_context中执行的，所以不会出现代码交错运行导致状态不一致的情况。
            if (!write_msgs_.empty()) {
                //创建一个新的buffer，buffer起始地址为待发队列中的第一个消息的起始地址，长度为第一个消息的完整长度（包括首部长度和载荷长度）。
                //在socket完成发送后，会调用回调函数handle_write（也就是此函数）
                boost::asio::async_write(socket_,
                                         boost::asio::buffer(write_msgs_.front().data(),
                                                             write_msgs_.front().length()),
                                         boost::bind(&ad_hoc_session::handle_write, shared_from_this(),
                                                     boost::asio::placeholders::error));
            }
        } else {
            scope.leave(id());
        }
    }

    /**
       * session主动发送数据的函数。
       *
       * 由于只有server端有session，因此只有在server转发数据时才会调用此函数，和client无关。
       * 在scope.deliver函数中会先查找对应的session，然后调用此session的deliver函数。
       *
       * @param msg 待发送的数据
       */
    void deliver(ad_hoc_message &msg) override {
        //判断队列中有没有未发完的消息。
        bool write_in_progress = !write_msgs_.empty();
        //向队列末端添加一个待发送的消息，实际的发送顺序服从于发起deliver的先后顺序。
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            boost::asio::async_write(socket_,
                                     boost::asio::buffer(write_msgs_.front().data(),
                                                         write_msgs_.front().length()),
                                     boost::bind(&ad_hoc_session::handle_write,
                                                 shared_from_this(),
                                                 boost::asio::placeholders::error));
        }
        //如果队列尾端有未发完的消息，那么这里不需要手动调用async_write函数，因为IO线程的handle_write是会发送队列中的剩余消息的。
        //只需要把消息存入队列即可。
    }

    int id() {
        return socket_.remote_endpoint().port();
    }

private:
    ad_hoc_scope &scope; //此session对象所属于的scope，一般会有多个session对象隶属于同一个scope
    tcp::socket socket_; //从server端到client端的socket连接，需要持有这个对象来进行读写操作
    ad_hoc_message read_msg_; //存放接收到的消息的存储空间。这个对象是复用的，不会重新初始化，但是其内部的数据在每次收到新消息后会被重新填充。
    //等待发送的消息队列。为了防止有多个用户线程同时发送数据，这里将多个待发送的数据存放在一个队列中，由IO线程逐一发送。
    message_queue write_msgs_;
};

typedef boost::shared_ptr<ad_hoc_session> ad_hoc_session_ptr;

class ad_hoc_server {
public:
    /**
     * ad_hoc_server 构造函数
     *
     * 实例化之后就会立刻开始监听端口，等待新连接到来。当新连接到来后，封装成session对象。
     *
     * @param endpoint server要监听的端口
     * @param io_context 负责server收发消息的IO事件循环。当异步函数绑定好回调函数之后，需要运行io_context.run()来启动事件循环。
     */
    ad_hoc_server(const tcp::endpoint &endpoint, boost::asio::io_context &io_context) : acceptor(io_context, endpoint),
                                                                                        io_context(io_context) {
        cout << "start listening at port " << endpoint.port() << endl;
        scope.Create_MatrixUDG();
        scope.print_UDG();
        //创建一个空的session对象
        ad_hoc_session_ptr new_session(new ad_hoc_session(io_context, scope));
        //服务器异步监听端口，直到有新连接到来
        //等待连接建立后，acceptor得到的新的socket会被填充到session中
        //boost::bind 给回调函数绑定2个局部变量（this和new_session）作为头两个参数
        //回调函数的第三个参数error是一个占位符，由async_accept在运行时负责填入这个参数
        acceptor.async_accept(new_session->socket(),
                              boost::bind(
                                      &ad_hoc_server::handle_accept,
                                      this,
                                      new_session,
                                      boost::asio::placeholders::error
                              ));
    }

private://接受client端主动发起的连接
    /**
    * 新连接建立的回调函数
    *
    * @param session 构造函数中绑定的参数new_session
    * @param error async_accept在运行时填入的、表示连接结果的状态码
    */
    void handle_accept(ad_hoc_session_ptr session, const boost::system::error_code &error) {
        //如果成功，则error为0
        if (!error) {
            cout << "accept incoming connection: " << session->socket().remote_endpoint().address() << ":"
                 << session->id() << endl;
            //启动该session的接收消息的循环

            session->start();
            //同构造函数里的步骤，等待下一个新连接的到来
            ad_hoc_session_ptr new_session(new ad_hoc_session(io_context, scope));
            acceptor.async_accept(new_session->socket(),
                                  boost::bind(
                                          &ad_hoc_server::handle_accept,
                                          this,
                                          new_session,
                                          boost::asio::placeholders::error
                                  ));
        } else {
            cerr << error << endl;
        }
    }

    tcp::acceptor acceptor; //端口监听器，接收新的连接并创建一个对应的socket
    boost::asio::io_context &io_context;
    ad_hoc_scope scope; //scope对象，每个server有一个scope，维护ID->session的映射表
};

#endif //ADHOC_SIMULATION_SERVER_H
