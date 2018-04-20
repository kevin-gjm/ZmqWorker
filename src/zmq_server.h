#ifndef _SQLSTORE_SERVER_H_
#define _SQLSTORE_SERVER_H_
#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <functional>
#include "client_info.h"
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>


using std::string;

class Server
{
public:
    Server(zmq::context_t* context,int port,std::string ip):
        ip_(ip),
        port_(port),
        running_(false),
        ctx_(context),
        sock_(new zmq::socket_t(*ctx_,ZMQ_DEALER)),
        proxy_sock_(new zmq::socket_t(*ctx_,ZMQ_DEALER))
    {
        std::string endpoint =std::string("tcp://")+ip_+":"+std::to_string(port_);
        sock_->connect(endpoint);
    }
    ~Server(){}

    void Start(int thread_count);
    void Stop()
    {
        running_ = false;
        std::vector<std::thread>::iterator iter;
        for (iter = thread_.begin(); iter != thread_.end();++iter)
        {
            (*iter).join();
        }
    }
private:
    void ProxyRoutine();
    void ReConnect();

private:
    std::string ip_;
    int port_;
    bool running_;
    zmq::context_t* ctx_;
    std::shared_ptr<zmq::socket_t> sock_;
    std::shared_ptr<zmq::socket_t> proxy_sock_;
    std::vector<std::thread> thread_;

};




#endif //_SQLSTORE_SERVER_H_
