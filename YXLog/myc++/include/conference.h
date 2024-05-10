#ifndef CONFERENCE_H
#define CONFERENCE_H
#include "Session.hpp"
#include "SilkFileStreamSource.hpp"
#include "Session.hpp"
#include "TestObject-C-Interface.h"


extern char recvBuff[100];
extern  Session*sss;


class MyCtext{
public:
 
    
    void test();

    //第一次启动加载
    int OnLoad(void*reserved);
    
    //第一次启动加载
    int UnOnLoad(void*reserved);
    
    //创建会话
     Session* raiseSession1(int sid,int ip,short port,string root_dir,bool egross);

    
    //开始心跳
    int startHeartBeat(Session* session, int ip, short port,int uin, int expired, int interval, int timeout);
    
    //开始说话
    int createSpeakNetParam(Session *session, int ssrc, int srcId,
                            int dstId, int sesstype, int netType, int netSignal);
    //停止说话
    int stopSpeak(Session* session, int handle);
    
    //群组保持心跳
    void onMiniteTick();
    //释放会话 --uin
    int releaseSession(int sid);
    
    //播放文件
    void playLocalFile(string filePath, int ssrc);
    
    int setUin(int uin);
    
    //停止播放文件
    int stopPlayLocalFile(int ssrc);
    
    
    
    //创建监听--当前会话，ssrc=说话标识
    int createListen(Session* session, int ssrc);
    //开始监听---当前会话，
    int startListen(Session* session, int handle);
    //停止监听
    int stopListen(Session*  session, int handle);
    //释放监听
    int releaseListen(Session* session, int handle,int waitMilSec);

   
    
    
};

int getTransactionOutputParam(TransactionOutputParam &param, vl_uint32 ssrc,
                              void* extension, vl_size extensionLen, vl_uint16 extId,
                              vl_uint32 nettype, vl_uint32 netsignal);

#endif // CONFERENCE_H
