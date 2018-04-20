#include "zmq_server.h"
#include <vector>
#include <exception>
#if (!defined (WIN32))
#include <sys/time.h>
#endif

using namespace  std;

#define UNKNOWN_REPLY -1
#define HEARTBEAT_LIVENESS  3               //  3-5 is reasonable
#define HEARTBEAT_INTERVAL  1000            //  msecs
#define HEARTBEAT_KEY       "HEARTBEAT"
#define INTERVAL_INIT       1000
#define ZMQ_POLL_MSEC       1000            //  usec
#define INTERVAL_MAX        32000

string g_sError;

static int64_t s_clock (void)
{
#if (defined (WIN32))
    SYSTEMTIME st;
    GetSystemTime (&st);
    return (int64_t) st.wSecond * 1000 + st.wMilliseconds;
#else
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) (tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}


void Server::Start(int thread_count)
{
    int iRet = UNKNOWN_REPLY;
    g_sError.clear();
    g_sError.append((char*)&iRet,sizeof(iRet));

    running_ = true;
    thread_.reserve(thread_count);
    for (int i = 0; i < thread_count; ++i)
    {
        thread_.push_back(std::thread(std::bind(&Server::ProxyRoutine,this)));
    }

    proxy_sock_->bind("inproc://proxy");




    int liveness = HEARTBEAT_LIVENESS;
    int interval = INTERVAL_INIT;

    int64_t heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;


    int rc;
    LOG(INFO)<<"Proxy start";
    while(running_)
    {
        zmq_pollitem_t pollitems [] =
        {
            { *sock_, 0, ZMQ_POLLIN, 0 },
            { *proxy_sock_, 0, ZMQ_POLLIN, 0 },
        };
        rc = zmq::poll(pollitems, 2,HEARTBEAT_INTERVAL*ZMQ_POLL_MSEC);
        if (rc == -1)
        {
            LOG(ERROR)<<"Get Poll error.exit;";
            break;
        }
        if (pollitems[0].revents & ZMQ_POLLIN)
        {

            // message
            //  - 3-part  request
            //  - 1-part  heartbeat
            zmq::multipart_t msg(*sock_);
            if(msg.size() == 3)
            {
                msg.send(*proxy_sock_);
                liveness = HEARTBEAT_LIVENESS;
            }
            else if(msg.size()==1)
            {
                zmq::message_t data_frame = msg.pop();
                string data((char*)data_frame.data(),data_frame.size());
                if(data==HEARTBEAT_KEY)
                {
                    liveness = HEARTBEAT_LIVENESS;
                }
            }
            interval = INTERVAL_INIT;
        }
        if (pollitems[1].revents & ZMQ_POLLIN)
        {
            zmq::multipart_t msg(*proxy_sock_);
            msg.send(*sock_);
        }else if (--liveness == 0)
        {
            LOG(INFO)<<"heartbeat failure, can't reach";
            LOG(INFO)<<"reconnecting in " << interval << " msec..." ;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            if (interval < INTERVAL_MAX)
            {
                interval *= 2;
            }
            ReConnect();
            liveness = HEARTBEAT_LIVENESS;
        }
        if (s_clock () > heartbeat_at)
        {
            heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;
            LOG(INFO) << " worker heartbeat" ;
            zmq::multipart_t send;
            send.addstr(HEARTBEAT_KEY);
            send.send(*sock_);
        }

    }

}

void Server::ProxyRoutine()
{

    std::shared_ptr<zmq::socket_t> receiver(new zmq::socket_t(*ctx_,ZMQ_DEALER));
    receiver->connect("inproc://proxy");
    LOG(INFO)<<"Work thread start";

    zmq_pollitem_t pollitems [] =
    {
        { *receiver, 0, ZMQ_POLLIN, 0 },
    };


    int rc;
    while(running_)
    {
        rc = zmq::poll(pollitems, 1,3000);
        assert (rc >= 0);
        if (pollitems[0].revents & ZMQ_POLLIN)
        {
            zmq::multipart_t oMsgRecv(*receiver);
            LOG(INFO)<<"Recv some info";
            if(oMsgRecv.size()<2)
            {
                continue;
            }

            zmq::message_t oIdentity = oMsgRecv.pop();
            ClientInfo client(oIdentity,true);

            std::shared_ptr<zmq::multipart_t> back_msg = client.MakeNewMsg();

            zmq::message_t oMessage = oMsgRecv.remove();
            std::string sResult;


            if(sResult.empty())
            {
                sResult = g_sError;
                LOG(ERROR)<<"Result is empty. Make it to error";
            }

            back_msg->addstr(sResult);
            bool bRet = back_msg->send(*receiver);
            if(bRet)
            {
                LOG(INFO)<<"Send to client success!";
            }else
            {
                LOG(ERROR)<<"Send to client error!";
            }
        }
    }
}

void Server::ReConnect()
{
    sock_ = std::shared_ptr<zmq::socket_t>(new zmq::socket_t(*ctx_,ZMQ_DEALER));
    std::string endpoint =std::string("tcp://")+ip_+":"+std::to_string(port_);
    LOG(INFO)<<"Reconnect to "<<endpoint;
    sock_->connect(endpoint);
    zmq::multipart_t send;
    send.addstr(HEARTBEAT_KEY);
    send.send(*sock_);
}

