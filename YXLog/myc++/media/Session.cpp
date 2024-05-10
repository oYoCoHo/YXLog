#include "Session.hpp"
#include "StreamPlayScheduler.hpp"

SimpleMemoryPool Session::simpleMemPool;

Session::Session(vl_uint32 rip, vl_uint16 rport, const char* root_dir, vl_uint32 engrossId, vl_uint16 lport) :
    sockAddr(rip, rport), outgoingMapLock(PTHREAD_MUTEX_INITIALIZER), incomingMapLock(PTHREAD_MUTEX_INITIALIZER), engrossId(engrossId)
{
    this->ready = defaultInitial(root_dir, lport);//初始化会话
    if(SPS_NORMAL_PERMIT != engrossId) {
        StreamPlayScheduler::getInstance()->enableEngross(this->engrossId);
    }
}



Session::Session(const char* rip, vl_uint16 rport, const char* root_dir, vl_uint32 engrossId, vl_uint16 lport) :
    sockAddr(rip, rport), outgoingMapLock(PTHREAD_MUTEX_INITIALIZER), incomingMapLock(PTHREAD_MUTEX_INITIALIZER), engrossId(engrossId) {

    this->ready = defaultInitial(root_dir, lport);

    if(SPS_NORMAL_PERMIT != engrossId) {
        StreamPlayScheduler::getInstance()->enableEngross(this->engrossId);
    }
}

Session::~Session()
{
    EndPointManager* eptMgr = EndPointManager::getInstance();
    disconnect();
    eptMgr->release(this->pEpt);

    if(engrossId != SPS_NORMAL_PERMIT)
    {
        StreamPlayScheduler::getInstance()->clearEngross();
    }
}

void Session::disconnect()
{
    printf("Session ====================== disconnect %d : %d ", sockAddr.ip(), sockAddr.port());
    EndPointManager::getInstance()->disableHeartBeat(pEpt, sockAddr.ip(), sockAddr.port());
}

vl_bool Session::defaultInitial(const char* rootDir, vl_uint16 lport) {

    /* 初始化状态 */
    recordFile = VL_TRUE;  // 是否打开本地存储，默认打开
    mute = VL_FALSE;  // 是否静音，默认关闭 否
    recording = VL_FALSE;  // 是否正在录音 否
    playing = VL_FALSE; // 是否播放状态 否

    /* 设置进入时间 */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts_in = tv.tv_sec;  // 进入该session的本地时间戳

    /* 设置群目录 */
    if(NULL != rootDir)
    {
        int len = strlen(rootDir);
        if(VL_MAX_PATH > len)
        {
            memcpy(this->root_dir, rootDir, len);
            this->root_dir[len] = 0;
        } else {
            return VL_FALSE;
        }
    }
    /* 获取本地端点，使能rtp编解码 */
    EndPointManager *eptMgr = EndPointManager::getInstance();
    pEpt = eptMgr->aquire(lport,NWK_UDP,&simpleMemPool);
    eptMgr->setupProtocol(pEpt,EPROTO_RTP);
    return VL_TRUE;

}

void Session::standby(EptHeartBeat& heartBeat)
{
    EndPointManager* eptMgr = EndPointManager::getInstance();
    eptMgr->enableHeartBeat(pEpt, heartBeat);

    printf("Session ====================== standby %d : %d \n", heartBeat.getRemoteIp() ,heartBeat.getRemotePort());
}


vl_uint32 Session::createIncomingTransaction(vl_uint32 ssrc, vl_bool isLocal) //创建接收队列
{

    printf(">> createIncomingTransaction: ip:port-%s:%d\n", inet_ntoa(sockAddr.ip_sin()), sockAddr.port());

    IncomingTransaction* incoming = new IncomingTransaction(sockAddr, pEpt, &simpleMemPool, (const char*)this->root_dir, ssrc, engrossId);

    if(VL_TRUE == StreamPlayScheduler::getInstance()->delegate(incoming))//1
    {
        return ssrc;

    } else {
        /* 已有相同ssrc在接收队列 */
        printf("create incoming transaction failed");
        delete incoming;
        return SESSION_INVALID_HANDLER;
    }

}

//SessionMsg Session::startPlayVoice(vl_uint32 handler) {
//    return EV_TRANS_DONE;
//    /*
//    IncomingTransaction* incoming = findIncomingTransaction(handler);
//    if(NULL == incoming) {
//      LOGE("session start play failed, transaction not exist");
//      return MSG_TRANS_NOT_EXIST;
//    }
//
//    vl_status ret = incoming->startTrack();
//    if(VL_SUCCESS != ret) {
//      LOGE("session start play failed, ret=%d", ret);
//      return MSG_AUDDEV_ERROR;
//    } else {
//      return MSG_SUCCESS;
//    }
//    */
//}

SessionMsg Session::stopPlayVoice(vl_uint32 handler) {

    
  IncomingTransaction* incoming = findIncomingTransaction(handler);
  if(NULL == incoming) {
    printf("session start play failed, transaction not exist\n");
    return MSG_TRANS_NOT_EXIST;
  }

    vl_status ret = incoming->stopTrack();
    if(VL_SUCCESS != ret) {
        printf("session stop play failed, ret=%d\n", ret);
        return MSG_AUDDEV_ERROR;
    } else {
        printf("session stop success, ret=%d\n", ret);
        return MSG_SUCCESS;
    }
    
//    return EV_TRANS_DONE;
}


SessionMsg Session::releaseIncomingTransaction(vl_uint32 handler, vl_uint32 waitMsec) {
    
  IncomingTransaction* incoming = findIncomingTransaction(handler);
  if(NULL == incoming) {
      printf("session release listen failed, transaction not exist\n");
    return MSG_TRANS_NOT_EXIST;
  }

    vl_status status = incoming->stopTrans(waitMsec);
    if(VL_SUCCESS == status) {
        delete incoming;
        popIncomingTransaction(handler);
        return MSG_SUCCESS;
    } else if(VL_ERR_IOQ_CLOSE_WAITING == status){
        return MSG_WAIT;
    } else {
        return MSG_ERROR;
    }
    
//    return EV_TRANS_DONE;
}

vl_uint32 Session::createOutgoingTransaction(TransactionOutputParam& param)
{
//      cleanOutgoingTransaction();
    printf(">> createOutgoingTransaction: %s:%d, extensionLen=%d, extId=%d \n", inet_ntoa(sockAddr.ip_sin()), sockAddr.port(),
    param.extensionLen, param.extId);
//      param.bandtype = WIDE_BAND;
//      param.qulity = 0;
//      param.format = AUDIO_FORMAT_UNKNOWN;
    OutgoingTransaction* outgoing = new OutgoingTransaction(sockAddr, pEpt, &simpleMemPool,(const char*)this->root_dir, param);

    if(VL_TRUE == StreamPlayScheduler::getInstance()->delegate(outgoing)) {
        return outgoing->getSSRC();
    } else {
        printf("Session delegate outgoing transaction failed");
        delete outgoing;
        return SESSION_INVALID_HANDLER;
    }
}

vl_uint32
Session::createOutgoingTransaction(vl_uint32 ssrc, void *rtpExtension, vl_size extensionLen,
                                   vl_uint16 extId) {
    //  cleanOutgoingTransaction();
    printf(">> createOutgoingTransaction: %s:%d, extensionLen=%d, extId=%d",
         inet_ntoa(sockAddr.ip_sin()), sockAddr.port(), extensionLen, extId);
    OutgoingTransaction *outgoing = new OutgoingTransaction(sockAddr, pEpt, &simpleMemPool,
                                                            (const char *) this->root_dir, ssrc,
                                                            rtpExtension, extensionLen, extId);

    if (VL_TRUE == StreamPlayScheduler::getInstance()->delegate(outgoing)) {
        return ssrc;
    } else {
        /* 已有相同ssrc在发送队列 */
        printf("Session delegate outgoing transaction failed");
        delete outgoing;
        return SESSION_INVALID_HANDLER;
    }
}



SessionMsg Session::startSpeak(vl_uint32 handler) {
    /*
  OutgoingTransaction* outgoing = findOutgoingTransaction(handler);
  if(NULL == outgoing) {
    LOGE("session start speak failed, transaction not exist");
    return MSG_TRANS_NOT_EXIST;
  }

    vl_status ret = outgoing->startRecord();
    if(VL_SUCCESS != ret) {
        LOGE("session start speak failed, ret=%d", ret);
        return MSG_AUDDEV_ERROR;
    } else {
        return MSG_SUCCESS;
    }
    */
        return MSG_SUCCESS;
}



// 停止录音函数由 schedualer 调用
SessionMsg Session::stopSpeak(vl_uint32 handler) {
    /*
  OutgoingTransaction* outgoing = findOutgoingTransaction(handler);
  if(NULL == outgoing) {
    LOGE("session start speak failed, transaction not exist");
    return MSG_TRANS_NOT_EXIST;
  }

    vl_status ret = outgoing->stopRecrod();
    if(VL_SUCCESS != ret) {
        LOGE("session stop speak failed, ret=%d", ret);
        return MSG_AUDDEV_ERROR;
    } else {
        return MSG_SUCCESS;
    }
    */
    printf("stopRecordAsync \n");
    StreamPlayScheduler::getInstance()->stopRecordAsync();
    return MSG_SUCCESS;
}

vl_bool Session::pushOutgoingTransaction(OutgoingTransaction *outgoing) {
    vl_bool ret;
    vl_uint32 ssrc = outgoing->getSSRC();

    printf("[MUTEX] LOCK %s %p", __FUNCTION__, &outgoingMapLock);
    pthread_mutex_lock(&outgoingMapLock);
    map<vl_uint32, OutgoingTransaction *>::iterator it = outgoingMap.find(ssrc);
    if (it == outgoingMap.end()) {
        outgoingMap.insert(pair<vl_uint32, OutgoingTransaction *>(ssrc, outgoing));
        ret = VL_TRUE;
    } else {
        printf("session push outgoing transaction failed, duplicated, ssrc=%d", ssrc);
        ret = VL_FALSE;
    }
    pthread_mutex_unlock(&outgoingMapLock);
    printf("[MUTEX] UNLOCK %s %p", __FUNCTION__, &outgoingMapLock);
    return ret;
}

void Session::popOutgoingTransaction(vl_uint32 ssrc) {
    printf("[MUTEX] LOCK %s %p", __FUNCTION__, &outgoingMapLock);
    pthread_mutex_lock(&outgoingMapLock);
    map<vl_uint32, OutgoingTransaction *>::iterator it = outgoingMap.find(ssrc);
    if (it != outgoingMap.end()) {
        outgoingMap.erase(it);
    } else {
        printf("session pop outgoing transaction failed, not exist ssrc=%d", ssrc);
    }
    pthread_mutex_unlock(&outgoingMapLock);
    printf("[MUTEX] UNLOCK %s %p", __FUNCTION__, &outgoingMapLock);
}

OutgoingTransaction *Session::findOutgoingTransaction(vl_uint32 ssrc) {
    OutgoingTransaction *trans = NULL;
    printf("[MUTEX] LOCK %s %p", __FUNCTION__, &outgoingMapLock);
    pthread_mutex_lock(&outgoingMapLock);

    map<vl_uint32, OutgoingTransaction *>::iterator it = outgoingMap.find(ssrc);
    if (it != outgoingMap.end()) {
        trans = (*it).second;
    }
    pthread_mutex_unlock(&outgoingMapLock);
    printf("[MUTEX] UNLOCK %s %p", __FUNCTION__, &outgoingMapLock);

    return trans;
}

vl_bool Session::pushIncomingTransaction(IncomingTransaction *incoming) {
    vl_uint32 ssrc = incoming->getSSRC();
    vl_bool ret;
    printf("[MUTEX] LOCK %s %p", __FUNCTION__, &incomingMapLock);
    pthread_mutex_lock(&incomingMapLock);
    map<vl_uint32, IncomingTransaction *>::iterator it = incomingMap.find(ssrc);
    if (it == incomingMap.end()) {
        incomingMap.insert(pair<vl_uint32, IncomingTransaction *>(ssrc, incoming));
        ret = VL_TRUE;
    } else {
        printf("session push incoming transaction failed, duplicated, ssrc=%d", ssrc);
        ret = VL_FALSE;
    }
    pthread_mutex_unlock(&incomingMapLock);
    printf("[MUTEX] UNLOCK %s %p", __FUNCTION__, &incomingMapLock);
    return ret;
}

void Session::popIncomingTransaction(vl_uint32 ssrc) {
    printf("[MUTEX] LOCK %s %p", __FUNCTION__, &incomingMapLock);
    pthread_mutex_lock(&incomingMapLock);
    map<vl_uint32, IncomingTransaction *>::iterator it = incomingMap.find(ssrc);
    if (it != incomingMap.end()) {
        incomingMap.erase(it);
    } else {
        printf("session pop incoming transaction failed, not exist ssrc=%d", ssrc);
    }
    pthread_mutex_unlock(&incomingMapLock);
    printf("[MUTEX] UNLOCK %s %p", __FUNCTION__, &incomingMapLock);
}

IncomingTransaction *Session::findIncomingTransaction(vl_uint32 ssrc) {
    IncomingTransaction *trans = NULL;

    printf("[MUTEX] LOCK %s %p\n", __FUNCTION__, &incomingMapLock);
    pthread_mutex_lock(&incomingMapLock);
    map<vl_uint32, IncomingTransaction *>::iterator it = incomingMap.find(ssrc);
    if (it != incomingMap.end()) {
        trans = (*it).second;
    }
    pthread_mutex_unlock(&incomingMapLock);
    printf("[MUTEX] UNLOCK %s %p\n", __FUNCTION__, &incomingMapLock);
    return trans;
}

void Session::invokeEvent(SessionMsg event, const SessionCBParam &param) {
    printf("session invoke event %d, ssrc=%d", event, param.ssrc);

    if (this->callback != NULL) {
        this->callback->onEvent(event, param);
    }
}



void Session::cleanOutgoingTransaction() {
    printf("[MUTEX] LOCK %s %p", __FUNCTION__, &outgoingMapLock);
    pthread_mutex_lock(&outgoingMapLock);
    auto iter = outgoingMap.begin();
    while (iter != outgoingMap.end()) {
        if (VL_TRUE == (iter->second)->readyForRelease()) {
            delete iter->second;
            outgoingMap.erase(iter++);
        } else {
            iter++;
        }
    }
    pthread_mutex_unlock(&outgoingMapLock);
    printf("[MUTEX] UNLOCK %s %p", __FUNCTION__, &outgoingMapLock);
}

// 释放录音函数由 schedualer 调用
SessionMsg Session::releaseOutgoingTransaction(vl_uint32 handler, vl_uint32 waitMsec) {
    return MSG_WAIT;
    /*
    OutgoingTransaction* outgoing = findOutgoingTransaction(handler);

    if(NULL == outgoing) {
      LOGE("session release speak failed, transaction not exist");
      return MSG_TRANS_NOT_EXIST;
    }
    vl_status status = outgoing->stopTrans(waitMsec);
    if(VL_SUCCESS == status) {
      delete outgoing;
      popOutgoingTransaction(handler);
      return MSG_SUCCESS;
    } else if(VL_ERR_IOQ_CLOSE_WAITING == status) {
      return MSG_WAIT;
    } else {
      return MSG_ERROR;
    }
    */
}
