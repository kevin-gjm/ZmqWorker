#include <string>
#include <iostream>
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>
#include "zmq_server.h"
extern "C" {
#include "ini.h"
}

using namespace std;

zmq::context_t *g_pContext;



int main()
{
    google::InitGoogleLogging("FeatureDetectServer");
    google::InstallFailureSignalHandler();
    google::SetLogDestination(google::GLOG_INFO, "log");
    google::SetLogSymlink(google::GLOG_INFO,"");
    FLAGS_alsologtostderr = true;
    FLAGS_colorlogtostderr=true;



#if _WIN32
    ini_t *config = ini_load("D:\\SmartPark_PublicSetting.ini");
#else
    ini_t *config = ini_load("/etc/smartpark/SmartPark_PublicSetting.ini");
#endif
    if(config == NULL)
        return -1;
    char proxy_ip[60]{0};
    int port=0;
    int thread_num =1;
    if (!ini_sget(config, "ZmqWorker", "proxy_ip", "%s", &proxy_ip))return -1;
    if (!ini_sget(config, "ZmqWorker", "proxy_port", "%d", &port))return -1;
    if (!ini_sget(config, "ZmqWorker", "thread_num", "%d", &thread_num))return -1;

    ini_free(config);

    zmq::context_t ctx;
    g_pContext = &ctx;

    Server oServer(&ctx,port,proxy_ip);

    oServer.Start(thread_num);

    return 0;

}
