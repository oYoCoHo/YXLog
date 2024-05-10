#include "conference.h"
#include "StreamPlayScheduler.hpp"
#include "RTPHeartBeat.hpp"
#include <iostream>
//#include <WinSock2.h>
#include <map>
//#include <ptt.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include "TestObject.hpp"
static int currentUin = 0;
int run_count=0;

static const char* g_media_repport_ip = "112.124.1.212";
static const unsigned short g_media_report_port = 33446;
static uint32_t g_report_uin;
Session *sss;
std::map<int, Session*> activeSession;//会话容器
pthread_mutex_t g_session_mutex = PTHREAD_MUTEX_INITIALIZER;


static uint8_t gReportConvertNettype(uint8_t netType) { //报告转换网类型
    uint8_t report_type = 0;
    switch (netType) {
    case 1: // 2G
        report_type = 2;
        break;
    case 2: // 3G
        report_type = 3;
        break;
    case 3:  // wifi
        report_type = 1;
        break;
    }

    return report_type;
}

// 暂时通过流媒体回调设置传入
int OS_API_VERSION = -1;
static uint32_t convert_x(uint32_t x)
{
    return x>>24 | x>>8&0xff00 | x<<8&0xff0000 | x<<24;
}
//这个函数完成和htobe64一样的功能
static uint64_t convert(uint64_t x)
{
    return convert_x(x)+0ULL<<32 | convert_x(x>>32);
}
class StreamPlayCallback: public StreamPlayObserver
{
private:
    int send;

public:
    void reportToServer(void* data, vl_size len)
    {
        int sock_fd = 0;
        struct sockaddr_in remote;

        memset(&remote,0, sizeof(struct sockaddr_in));
        remote.sin_family = AF_INET;
        remote.sin_addr.s_addr = inet_addr(g_media_repport_ip);
        remote.sin_port = htons(g_media_report_port);

        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        sendto(sock_fd, (char*)data, len, 0, (struct sockaddr *) &remote, sizeof(struct sockaddr_in));
//        closesocket(sock_fd);
        close(sock_fd);
    }

    void reportRecvSummary(struct RecvTransInfo& recvInfo)
    {
        RecvTransInfo netInfo;
        netInfo.first_recv_ts = convert(recvInfo.first_recv_ts);
        netInfo.play_ts = convert(recvInfo.play_ts);
        netInfo.max_jitter = htonl(recvInfo.max_jitter);
        netInfo.total_jitter = htonl(recvInfo.total_jitter);
        netInfo.max_seq = htonl(recvInfo.max_seq);
        
        netInfo.ssrc = htonl(recvInfo.ssrc);
        netInfo.send_id = htonl(recvInfo.send_id);
        netInfo.group_id = htonl(recvInfo.group_id);
        netInfo.recv_id = htonl(g_report_uin);  // 固定为本地uin
        netInfo.flag = htons(recvInfo.flag);
        
        netInfo.dur_per_pack = htons(recvInfo.dur_per_pack);
        netInfo.block_times = htons(recvInfo.block_times);
        netInfo.normalEnd = recvInfo.normalEnd;
        netInfo.lost_persent = recvInfo.lost_persent;
        netInfo.send_os = recvInfo.send_os;
        netInfo.recv_os = 1;//安卓原代码上是1，以前的iOS是0
        netInfo.send_net_type = recvInfo.send_net_type;
        /* jni反射获取本地网络类型 */
        netInfo.recv_net_type =1;
        
        
        
        reportToServer(&netInfo, sizeof(struct RecvTransInfo));
    }
    /*
     * 开始接收流媒体数据事件
     */
    void onReceiveStart(vl_uint32 ssrc) {

        return;
    }
    /*
     * 结束接收流媒体数据事件
     */
    void onReceiveEnd(vl_uint32 ssrc) {

        return;
    }

    /**
     * 流媒体处理完毕，并已经释放占用资源
     */
    void onRelease(vl_uint32 ssrc, struct SpsResultInfo& info) {

        /* 上报接收情况 */
        reportRecvSummary(info.streamInfoDetail.recfInfo);

    }

    /**
     * 开始播放流媒体事件
     */
    void onPlayStarted(vl_uint32 ssrc) {
        return;
    }
    /**
     * 结束播放流媒体事件
     */
    void onPlayStoped(vl_uint32 ssrc) {
        return ;
    }

    void onMediaHeartbeatTimeout()
    {
        return;
    }
    void onNewIncomingTransByJNI(int type, int send_id, int recv_id, int ssrc)
    {
        return;
    }

    void onMediaHeartbeatRecv(int i)
    {
        return;
    }
    void onMediaHeartbeatFailed()
    {
        return;
    }

};





StreamPlayCallback mediaCallback;


void notifyMediaHeartbeatTimeout() {
    mediaCallback.onMediaHeartbeatTimeout();
}



Session * getSessionById(int sid)
{
    Session *sess = NULL;
    pthread_mutex_lock(&g_session_mutex);
    std::map<int, Session*>::iterator iter = activeSession.find(sid);
    if (iter != activeSession.end())
    {
        sess = iter->second;
    } else {
    }
    
    pthread_mutex_unlock(&g_session_mutex);
    return sess;
}

int MyCtext::releaseSession(int sid)
{
//    printf("释放会话releaseSession beg -- sid:%d\n", sid);
    pthread_mutex_lock(&g_session_mutex);
    std::map<int, Session*>::iterator iter = activeSession.find(sid);
    if (iter != activeSession.end()) {
        delete iter->second;
        activeSession.erase(iter);
    } else {
    }
    
    
    
    printf("releaseSession end");
    pthread_mutex_unlock(&g_session_mutex);
    return 0;
}


void MyCtext::test(){
    }

Session* MyCtext::raiseSession1(int sid,int ip,short port,string root_dir,bool egross)
{
    printf("raiseSession1 begin\n");
    const char *dir = root_dir.data();//获取本地地址安卓
    ip = ntohl(ip);

    //绑定地址
    sockaddr_in remote;
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(ip);
    remote.sin_port = htons(port);
    printf(">>> raiseSession - sid:%d, ip:port:%s:%d, dir:%s\n", sid,inet_ntoa(remote.sin_addr), ntohs(remote.sin_port), dir);

    Session *sess = getSessionById(sid); //通过sid从容器中获取会话
    if (NULL == sess)
    {
        if (egross) //是否点对点
        {
            sess = new Session(ip, port, dir, sid);
        } else {
            sess = new Session(ip, port, dir);
        }
        pthread_mutex_lock(&g_session_mutex);
//        activeSession[sid]=sess;
        activeSession.insert(std::pair<int, Session *>(sid, sess));
        pthread_mutex_unlock(&g_session_mutex);

    } else
    {
        printf("session already exist !!\n");
    }
    printf("raiseSession1 end %p\n", sess);


    return  sess;
}


int MyCtext::startHeartBeat(Session*  session, int ip, short port,int uin, int expired, int interval, int timeout)
{
    printf("startHeartBeat begin\n");
    g_report_uin = uin;//用户标识
    ip = ntohl(ip);

    //设置地址端口等
    sockaddr_in remote;
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(ip);
    remote.sin_port = htons(port);

    printf(
        "startHeartBeat - ip:%s, port:%d, uid:%d, expired:%d, interval:%d, timeout:%d\n",
        inet_ntoa(remote.sin_addr), ntohs(remote.sin_port), uin, expired,
        interval, timeout);


    if (NULL != session)
    {

        SessionHeartbeat heartBeat(ip, port, uin, expired, interval, timeout); // memory leak 内存泄漏???
        session->standby(heartBeat);

        printf("startHeartBeat done\n");
        return 0;
    }
    return -1;
}


int startListen(Session* session, int handle)
{
    printf("startListen begin - --------%d-----------%d\n",session,handle);
    if (NULL != session)
    {
        return session->startPlayVoice(handle);
    }
    return -1;
}

SessionMsg Session::startPlayVoice(vl_uint32 handler)
{
   return MSG_SUCCESS;//2022/8/23变动
//    return EV_TRANS_DONE;
    
}

int createListen(Session* session, int ssrc)
{
   printf("createListen -- ----- ssrc:%d \n" ,ssrc);
    
    
    if (NULL != session) {
        
        int ret = session->createIncomingTransaction(ssrc, false);//创建接收队列
        return ret;
    }
    
    return 0;
}


int MyCtext::stopListen(Session*  session, int handle)
{
    printf("stopListen ------ handle:%d\n",  handle);
    if (NULL != session) {
        
//        StreamPlayScheduler::getInstance()->terminatePlay();
        
        return session->stopPlayVoice(handle);
    }
    printf("【停止活动】stopListen error【接收】\n");
    return -1;
}

int releaseListen(Session*  session, int handle,int waitMilSec)
{
    printf("releaseListen  handle:%d, wait:%d",  handle,waitMilSec);
    if (NULL != session)
    {
        return session->releaseIncomingTransaction(handle, waitMilSec);
    }
    printf("releaseListen error");
    return -1;
}

void notifyMediaHeartbeatReceive(int i) {
    mediaCallback.onMediaHeartbeatRecv(i);

}
char recvBuff[100];
void incoming_trigger(int type, int send_id, int recv_id, int ssrc)
{
/**
 c++要获取我的id。
 */
    char buffer[256];
    strcpy(buffer,getenv("HOME"));
    strcat(buffer,"/Documents/uid.txt");
    char str[100];//缓冲区，用来储存数据
    FILE *f = fopen(buffer,"rb");
    if(f)
    {
        fgets(str,100,f);
    }
    int num1=atoi(str);

    Session *sess = getSessionById(num1); //通过sid从容器中获取会话
    createListen(sess,ssrc);
    startListen(sess,ssrc);
    mediaCallback.onNewIncomingTransByJNI(type, send_id, recv_id, ssrc);
    
    
    
    time_t starttime;
    time(&starttime);
    
    printf("没赋值前recvBuff>>>>>>%s\n", recvBuff);
    
    memset(recvBuff,0,sizeof(recvBuff));
    
    sprintf(recvBuff, "%d,%d,%d,%d,%ld",type, send_id, recv_id, ssrc,starttime);
    
    printf("赋值后recvBuff>>>>>>%s\n", recvBuff);
    
}




void initialCrashTrack() {
    return;
}




int MyCtext::OnLoad(void*reserved)
{
    printf("jni loading 20150703---------------------\n");
    StreamPlayScheduler::getInstance()->setObserver(&mediaCallback);
    bool a = StreamPlayScheduler::getInstance()->startWork();
    if(a)
        printf("work success............\n");
    initialCrashTrack();

    return 0;
}

int MyCtext::UnOnLoad(void*reserved)
{
    printf("jni loading 20150703---------------------\n");
//    StreamPlayScheduler::getInstance()->setObserver(&mediaCallback);
//    StreamPlayScheduler::getInstance()->stopWork();
//    StreamPlayScheduler::getInstance()->~StreamPlayScheduler();
    StreamPlayScheduler::getInstance()->stopWork();
    pthread_mutex_lock(&g_session_mutex);

    std::map<int, Session *>::iterator iter = activeSession.begin();
    while (iter != activeSession.end()) {
        delete iter->second;
        iter++;
    }
    activeSession.clear();

    pthread_mutex_unlock(&g_session_mutex);
    
    printf("work success............\n");
    
    return 0;
}


int MyCtext::createSpeakNetParam(Session *session, int ssrc, int srcId,
    int dstId, int sesstype, int netType, int netSignal)
{
    if (NULL != session)
    {
        srcId = htonl(srcId);
        dstId = htonl(dstId);

        unsigned int report_net_type = gReportConvertNettype(netType);//获取网络类型 pc端直接设置为1  WiFi
        unsigned int report_os_type;

        
//        report_os_type = 2;
        report_os_type = 1;
        ExternData ext = { (vl_uint32) srcId, (vl_uint32) dstId,(vl_uint32) sesstype, report_net_type, report_os_type, 0 };

        int len = sizeof(ext);
        int id = 0;

        TransactionOutputParam param;
        int ret = getTransactionOutputParam(param, ssrc, &ext, len, id, netType,netSignal);
        if (0 == ret) {
            return session->createOutgoingTransaction(param);
        }

    }
    return 0;
}



int getTransactionOutputParam(TransactionOutputParam &param, vl_uint32 ssrc,
                              void* extension, vl_size extensionLen, vl_uint16 extId,
                              vl_uint32 nettype, vl_uint32 netsignal) {

    param.format = AUDIO_FORMAT_SILK;

    if (1 == nettype) {
        param.bandtype = NARROW_BAND;
    } else if (2 == nettype) {
        param.bandtype = MEDIUM_BAND;
    } else if (3 == nettype) {
        param.bandtype = WIDE_BAND;
    } else {
        param.bandtype = MEDIUM_BAND;
    }
    if (netsignal < 0 || netsignal > 10) {
        netsignal = 5;
    }
    param.qulity = netsignal;
    param.ssrc = ssrc;
    param.dtx = VL_FALSE;
    param.fec = VL_TRUE;
    param.cbr = VL_TRUE;
    param.extension = extension;
    param.extensionLen = extensionLen;
    param.extId = extId;

    return 0;
}

int startSpeak(Session* session, int handle)
{
    printf("startSpeak -  handle:%d\n",  handle);
//    Session *sess = (Session *) session;
    if (NULL != session) {
        SessionMsg ret = session->startSpeak(handle);
        if (MSG_SUCCESS == ret)
        {
            return 0;
        } else {
            return -2;
        }
    }
    return -1;
}

int MyCtext::stopSpeak(Session* session, int handle)
{
 
    printf("stopSpeak -  handle:%d\n",  handle);
//    Session *sess = (Session *) session;
    if (NULL != session)
    {
        SessionMsg ret = session->stopSpeak(handle);
        if (MSG_SUCCESS == ret)
        {
            
//            StreamPlayScheduler::getInstance()->resumePlay();
            return 0;
        } else {
            return -2;
        }
    }
    return -1;
}


void MyCtext::playLocalFile(string filePath, int ssrc)
{
    
    locale_afilePlay=1;
    const char *file = filePath.data();
    printf("打开本地文件playLocalFile: %s \n ssrc:%d\n", file, ssrc);
    SilkFileStreamSource* silkFileSource = new SilkFileStreamSource(file, ssrc);
    vl_bool ret = false;
    if (silkFileSource->spsReadyForPlay())
    {
        ret = StreamPlayScheduler::getInstance()->delegate(silkFileSource,VL_TRUE);
    }
    return ;
}


int MyCtext::setUin(int uin)
{
    currentUin = uin;
    if(currentUin>0)
        return 1;
    return -1;
}

int MyCtext::stopPlayLocalFile(int ssrc)
{
    printf("stopPlayLocalFile - ssrc:%d\n", ssrc);
    vl_int32 ret = 0;
    if (0 == ssrc) {
        StreamPlayScheduler::getInstance()->terminatePlay();
    } else {
        StreamPlayScheduler::getInstance()->terminatePlay(ssrc);
    }
    return ret;
}


void MyCtext::onMiniteTick() {
    EndPointManager::getInstance()->rtcKeepAlive();
}
