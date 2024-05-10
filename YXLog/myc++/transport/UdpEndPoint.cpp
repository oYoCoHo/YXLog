#include <pthread.h>
#include <memory.h>
#include <stdio.h>
#include "vl_types.h"
#include "vl_const.h"
#include "UdpEndPoint.hpp"
#include "Time.hpp"

#define MAX_MTU (32000)

static unsigned int sendtm = 0, sendlen = 0,sendios = 0;
static unsigned int recvtm = 0, recvlen = 0,recvios = 0;

/*
   * 每条ioq对应EndPoint中的一个外网ip端口，并指定一种应用层协议。
   * proto [in] : 应用层协议用于确定产生的EptData的协议类型。
   * rip [in] : 用于确认在该EndPoint上对应的外网ip
   * rport [in] : 用于确定在该EndPoint上对应的外网端口
   */
vl_status UdpEndPoint::sendFromIOQ(IOQueue* ioq, vl_size* count)
{
    *count = 0;
    if (NULL == ioq) {
        printf("UdpEndPoint sendFromIOQ failed, ioq is null\n");
        return VL_ERR_EPT_SEND_INVALID;
    }

    /*
    * 网络传输应用层协议
    */
    EProtocol protocol = ioq->getProtocal();//协议类型
    Transmittable * pTrans = NULL;//可传输数据封装包含传输内容，传输协议类型，远程端口

    struct sockaddr_in * remoteAddr = ioq->getRemoteAddr().getNativeAddr();
    void * platData;
    vl_size platLen;
    int sockfd = getSockFd();

    EptProtocol* encoder = protocols[protocol];//编解码  对象
    vl_status retprot;

    /* 没有对应的协议编解码器 */
    if (protocol >= EPROTO_MAX || protocol <= EPROTO_UNKNOW || encoder == NULL)
    {
        while ((pTrans = ioq->read()) != NULL)
        {

            ProtoObject* dataObj = pTrans->getObject();

            if(NULL == dataObj)
            {
                printf("UdpEndPoint sendFromIOQ ProtoObject is null\n");
                continue;
            }

            /* 优先获取编码数据 */
            if(dataObj->isEncoded())
            {
                platData = dataObj->getPacket();
                platLen = dataObj->getPacketLength();
            }

            if(VL_SUCCESS == retprot)
            {
                //发送编码数据过去zjw
                size_t sended = ::sendto(sockfd, (char *)platData, platLen, 0,(struct sockaddr *) remoteAddr, sizeof(sockaddr_in));
                if(sended == platLen) {
                    sendlen += platLen,sendios++;
                    if(sendtm!=(unsigned int)time(NULL))
                    {
                        sendlen = 0;
                        sendios = 0;
                        sendtm=(unsigned int)time(NULL);
                    }
                    increaseSendedCount(sended);//增加已经发送的字节数zjw
                } else if(sended < 0) {
                    reopen(ESOCK_DGRAM);
                } else if(sended < platLen) {

                }
                (*count)++;
            } else {
                printf("UdpEndPoint sendFromIOQ data encode failed\n");
            }
        }
    } else {
        while ((pTrans = ioq->read()) != NULL)
        {
            ProtoObject* dataObj = pTrans->getObject();
            if(NULL == dataObj) {
                printf("UdpEndPoint sendFromIOQ ProtoObject is null\n");
                delete pTrans;
                continue;
            }

            if(VL_TRUE != dataObj->isEncoded()) {
                retprot = encoder->encode(dataObj);
            }

            dataObj->onSent();

            platData = dataObj->getPacket();
            platLen = dataObj->getPacketLength();

            if(VL_SUCCESS == retprot)
            {
                size_t sended = ::sendto(sockfd, (char*)platData, platLen, 0,(struct sockaddr *) remoteAddr, sizeof(sockaddr_in));
                if(sended>0)
                    printf("UdpEpt have send to server %s:%d size=%d \n", inet_ntoa(remoteAddr->sin_addr), ntohs(remoteAddr->sin_port), platLen);

                if(sended == platLen) {
                    sendlen += platLen,sendios++;
                    if(sendtm!=(unsigned int)time(NULL)){
                        //LOGW("send l=%u p=%u",sendlen, sendios);
                        sendlen = 0;
                        sendios = 0;
                        sendtm=(unsigned int)time(NULL);
                    }
                    increaseSendedCount(sended);
                } else if(sended < 0) {
                    reopen(ESOCK_DGRAM);
                } else if(sended < platLen) {

                }
                (*count)++;
            } else {
                printf("UdpEndPoint sendFromIOQ data encode failed\n");
            }
            delete dataObj;
            delete pTrans;
        }
    }

    return VL_SUCCESS;
}

void* udp_ept_send_looper(void* userData)
{
    if (NULL == userData) {
        printf("enter send looper, userData is null\n");
        return NULL;
    }
    UdpEndPoint * udpEpt = (UdpEndPoint*) userData;
    while (VL_TRUE == udpEpt->sending)
    {
        vl_size number;
        pthread_mutex_lock(&udpEpt->sqLock);
        if (VL_TRUE != udpEpt->sending) {
            pthread_mutex_unlock(&udpEpt->sqLock);
            break;
        }
        pthread_cond_wait(&udpEpt->sqCond, &udpEpt->sqLock);

        /* 遍历所有协议队列 */
        for (int i = 0; i < EPROTO_MAX; i++)
        {
            //创建一个发送队列：ioq
            list<IOQueue*>* ioqList = &udpEpt->sendIOQs[i];

            if (ioqList->size() > 0)
            {
                list<IOQueue*>::iterator it = ioqList->begin();

                /* 遍历指定协议的发送队列 */
                while (it != ioqList->end())
                {
                    udpEpt->sendFromIOQ(*it, &number);
                    it++;
                }

            }
        }
        pthread_mutex_unlock(&udpEpt->sqLock);
    }
    
    return 0;
}

/* NOTE : 贴膏药 */
#include "RTPProtocol.hpp"



vl_status UdpEndPoint::recvToIOQ(UdpEndPoint * endpoint, void* data, vl_size length, sockaddr_in * addr_in)
{
    pthread_mutex_lock(&hbMutex);
    auto iter = heartBeatExecs.begin();
    /* 先判断是否是心跳 */
    
    while(iter != heartBeatExecs.end())
    {
        if(((*iter)->heartbeart.isResponse(data, length)))
        {
            (*iter)->heartbeart.onResponse();
            (*iter)->updateHBRspTime();
            pthread_mutex_unlock(&hbMutex);
            return VL_SUCCESS;
        }
        iter ++;
    }
    pthread_mutex_unlock(&hbMutex);

    /* 收到数据，遍历应用层协议解码
    * 具体的协议编解码对应一个编解码枚举号。
    * 大部分网络编码解码只是在头部和尾部简单加上包头和包尾，这种类型的编解码可以优化内存拷贝次数，通过isSimpleWrap()接口判断。
    * 编码操作不申请内存；
    * 解码时先调用parse函数，确认包是否可使用该解码实例解码，若能解码，由外部申请一个对象传入解码。
    * 具体的内存策略需要看具体的编解码实现。
    */
    
//    printf("【】网络传输应用层协议总数------%d\n",EPROTO_MAX);

    for (int i = 0; i < EPROTO_MAX; i++)
    {
        EptProtocol * decoder = protocols[i];
        ProtoObject* dataObj = NULL;
        RTPObject* rtpObj;
        
        int ret;
        if(decoder!=NULL)
        {
            //解码时先调用parse函数，确认包是否可使用该解码实例解码，若能解码，由外部申请一个对象传入解码
            if(VL_TRUE == decoder->parse(data,length))
            {
                
//                printf("【】解码时先调用parse函数，确认包是否可使用该解码实例解码，若能解码，由外部申请一个对象传入解码\n");
                dataObj = decoder->allocObject(data, length);//申请一个对象传入解码
//                printf("【】解码 长度 %d\n",length);
                ret=decoder->decode(dataObj, data, length);//解码
                rtpObj = dynamic_cast<RTPObject*>(dataObj);
//                printf("【】解码 对象rtpObj %u\n",rtpObj);
//                printf("【】解码 类型getPayloadType %d\n",rtpObj->getPayloadType());

                if(ret != VL_SUCCESS || (rtpObj->getPayloadType() != RTP_VIDEO_PAYTYPE && rtpObj->getPayloadType() != RTP_MEDIACODEC_PAYTYPE && length > 1500) ){
                    decoder->freeObject(dataObj);
//                    printf("【】decoder->freeObject(dataObj)------%d\n",dataObj);
                    continue;
                }

                //可传输数据封装，包含传输内容，传输协议类型，远程端口
                Transmittable* pTrans = new Transmittable((EProtocol)i, new SockAddr(addr_in), dataObj);
                /* 遍历接收ioq，直到接收成功 */
                list<IOQueue*>* ioqList = &recvIOQs[i];
                pthread_mutex_lock(&rqLock);
                list<IOQueue*>::iterator it = ioqList->begin();
                         
                while(it!=ioqList->end())
                {
                    if(VL_TRUE == (*it)->canAccept(pTrans))
                    {
                        (*it)->write(pTrans);
                        pTrans = NULL;
                        dataObj = NULL;
                        break;
                    }else{
                    }
                    it++;
                }
                pthread_mutex_unlock(&rqLock);

                /* NOTE : 贴膏药 */
                if(EPROTO_RTP == i && NULL != pTrans) {
                    RTPObject* rtpObj = dynamic_cast<RTPObject*>(pTrans->getObject());
                    
                    ExternData* externData = (ExternData*)rtpObj->getExtensionData();

                    printf("解码获取数据externData    >>>>%s\n", externData);
                    
                    if(externData != NULL)
                    {
                        vl_uint32 sid = ntohl(externData->send_id);
                        vl_uint32 rid = ntohl(externData->recv_id);
                        printf("UdpEndPoint may raise a incoming transaction : type=%d, from=%d, to=%d, ssrc=%u\n",externData->flag, sid, rid, rtpObj->getSSRC());
                        
//                        printf("【】解码 更新对象rtpObj。 %u\n",rtpObj);
//                        printf("【】解码 更新类型getPayloadType  %d\n",rtpObj->getPayloadType());
                        
                        if(rtpObj != NULL && (rtpObj->getPayloadType() == RTP_VIDEO_PAYTYPE || rtpObj->getPayloadType() == RTP_MEDIACODEC_PAYTYPE)){
                            time_t t;
                            time(&t);
//                            printf("【】解码 被终止 %d\n",t);
                            
                            delete pTrans;
                            pTrans = NULL;
                            delete dataObj;
                            dataObj = NULL;
                        }else{
                            
                            time_t t;
                            time(&t);
                            printf("【rtpObj】写入播放器 时间戳  %ld\n",t);
                            printf("【rtpObj】写入播放器 sid  %d\n",sid);
                            printf("【rtpObj】写入播放器 rid  %d\n",rid);
                            printf("【rtpObj】写入播放器 getSSRC %d\n",rtpObj->getSSRC());
                            
                            incoming_trigger(externData->flag, sid, rid, rtpObj->getSSRC());//安卓操作
                            
                            
                        }
                    }
                    /* 没有io队列接收，放入缓存 */
                    if(NULL != pTrans) {
                        if(VL_TRUE != recvEncache(pTrans))
                        {
                            
                            printf("没有io队列接收，放入缓存\n");
                            
                            if(NULL != dataObj) {
                                delete dataObj;
                            }
                            /* 没有ioq能接收该数据 */
                            if(NULL != pTrans) {
                                delete pTrans;
                            }
                        }
                    }
                }

                printf("结束写入数据，数据包为1\n");
            }
        }
    }
    return VL_SUCCESS;
}



void* udp_ept_recv_looper(void* userData) {

    if (NULL == userData) {
        printf("enter recv looper, userData is null\n\n");
        return NULL;
    }
    UdpEndPoint* udpEpt = (UdpEndPoint*) userData;
    int sockfd = udpEpt->getSockFd();

    int ret = 0;
    fd_set read_fds;
    fd_set exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);

    static struct sockaddr_in remoteAddr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
   
    vl_uint8* buffer = NULL;
    vl_uint8* rtemp = NULL;
    struct timeval st;

    memset(&remoteAddr,0, sizeof(remoteAddr));

    while (VL_TRUE == udpEpt->recving) {
//        printf("udpEpt->recving状态是VL_TRUE\n");
        FD_ZERO(&read_fds);             //这个不知道有没有关
        FD_SET(sockfd, &read_fds);
        FD_SET(sockfd, &exception_fds);

        if(NULL == buffer) {
            buffer = (vl_uint8*)__MALLOC(MAX_MTU);
            memset(buffer, 0,MAX_MTU);
        }

        st.tv_sec = 0;
        st.tv_usec = 500 * 1000;
        ret = select(sockfd + 1, &read_fds, NULL, &exception_fds, &st);

        if (ret < 0) {
            if (buffer != NULL) {
                __FREE(buffer);
            }
            break;
        }
        static int lastRecv = 0;
        if (FD_ISSET(sockfd, &read_fds)) {
            
            ret = recvfrom(sockfd, (char *)buffer, MAX_MTU, 0, (struct sockaddr *)&remoteAddr, &addr_len);
            printf("Udp ept recv looper---%s:%d size=%d \n", inet_ntoa(remoteAddr.sin_addr), ntohs(remoteAddr.sin_port), ret);
 
            if (ret < 0)
            {
                if (buffer != NULL) {
                    __FREE(buffer);
                }
                printf("UdpEndPoint recvfrom failed\n");
                break;
            }
            if(ret > 0)
            {
                if (lastRecv == ret && ret == 128) {
                    printf("other last packet.....\n");
                }else{
                    lastRecv = ret;
                    rtemp = (vl_uint8*)__MALLOC(ret);
                    if(rtemp){
                        memcpy(rtemp, buffer, ret);
                        udpEpt->recvToIOQ(udpEpt, rtemp, ret, &remoteAddr);
                        /* 内存已交付后续流程管理 */
//                        printf("内存已交付后续流程管理\n");
                        udpEpt->increaseRecvCount(ret);
                    }
                    recvlen += ret,recvios++;
                    if(recvtm!=(unsigned int)time(NULL)){
                        //LOGW("recv l=%u p=%u",recvlen, recvios);
                        recvlen = 0;
                        recvios = 0;
                        recvtm=(unsigned int)time(NULL);
                    }
                    
                    // TODO: 叶炽华 联调
//                    time_t _time;
//                    char time_buffer[40];
//                    struct tm* tm_info;
//                    time(&_time);
//                    tm_info = localtime(&_time);
                    
//                    free(rtemp);
                }
                

            }
        }
        
        
    }
    
    return 0;
}



vl_status UdpEndPoint::initial()
{
    int ret = 0;

    do {
        /* 创建接收线程 */
        recving = VL_TRUE;
        ret = pthread_create(&recv_thread, NULL, udp_ept_recv_looper, this);
        if (ret != 0)
        {
            printf("UdpEndPoint create recv thread failed, ret = %d\n", ret);
            recv_thread =(pthread_t) -1;
            recving = VL_FALSE;
            break;
        }

        /* 创建发送线程 */
        sending = VL_TRUE;
        ret = pthread_create(&send_thread, NULL, udp_ept_send_looper, this);
        if (ret != 0)
        {
            printf("UdpEndPoint create send thread failed, ret = %d\n", ret);
            send_thread = (pthread_t) -1;
            sending = VL_FALSE;
            break;
        }

        status = EEPT_STATE_2;
        return VL_SUCCESS;
    } while (0);

    return VL_ERR_EPT_INIT_FAILED;
}



vl_status UdpEndPoint::sendTo(vl_uint32 remoteIp, vl_uint16 remotePort,
                              void* data, vl_size length) {
    if (NULL == data || length == 0) {
        printf("UdpEndPoint send empty data\n\n");
        return VL_ERR_EPT_TRANS_NODATA;
    }

    sockaddr_in remote;
    int ret;

    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(remoteIp);
    remote.sin_port = htons(remotePort);

    ret = ::sendto(getSockFd(), (char*)data, length, 0,
                   (const struct sockaddr *) &remote, sizeof(remote));
   
    if(ret == length) {
        increaseSendedCount(length);
    } else if(ret < 0) {
        reopen(ESOCK_DGRAM);
    } else if(ret < length) {

    }

    if (ret < 0) {
//        sock.reinitSock();
        perror("---UdpEndPoint send data failed ---\n");
        printf("UdpEndPoint send data failed %d\n", ret);
        return VL_ERR_EPT_SEND_FAILED;
    }

    return VL_SUCCESS;
}

vl_status UdpEndPoint::sendTo(Transmittable* data) {
    if (NULL == data) {
        printf("UdpEndPoint send data is null\n\n");
        return VL_ERR_EPT_SEND_INVALID;
    }

    struct sockaddr_in * remoteAddr = data->getAddress().getNativeAddr();
    void* platData;
    vl_size platLen;
    int ret;
    vl_status retprot;

    /* 从transmittable中获取要发送的数据流 */
    ProtoObject* dataObj = data->getObject();
    if(NULL == dataObj) {
        printf("UdpEndPoint sendto failed, dataObj is null\n\n");
        return VL_ERR_EPT_TRANS_NODATA;
    }

    /* 优先获取编码数据 */
    if(dataObj->isEncoded()) {
        //retprot = dataObj->getPacket((vl_uint8**)&platData, &platLen);
        platData = dataObj->getPacket();
        platLen = dataObj->getPacketLength();
    } else {
        //retprot = dataObj->getPacketNoEncode((vl_uint8**)&platData, &platLen);
    }

    if(VL_SUCCESS == retprot) {
        int ret = ::sendto(getSockFd(), (char*)platData, platLen, 0,(struct sockaddr *) remoteAddr, sizeof(sockaddr_in));

        if(ret == platLen) {
            increaseSendedCount(platLen);
        } else if(ret < 0) {
            reopen(ESOCK_DGRAM);
        } else if(ret < platLen) {

        }
        if (ret < 0) {
            printf("UdpEndPoint send data failed %d\n", ret);
            return VL_ERR_EPT_SEND_FAILED;
        } else {
            return VL_SUCCESS;
        }
    } else {
        printf("UdpEndPoint transmittable has no data %d\n", retprot);
    }
    return 0;
}



vl_status UdpEndPoint::send(void* data, vl_size length) {
    return 0;
}

vl_status UdpEndPoint::stopRecv() {
    
    if(recv_thread!=NULL) {
        void* ret;
        pthread_join(recv_thread, &ret);
        recv_thread = (pthread_t)-1;
    }
    if(VL_TRUE == recving) {
        recving = VL_FALSE;
    }
    
    return 0;
}

vl_status UdpEndPoint::stopSend() {
    if(VL_TRUE == sending) {
        sending = VL_FALSE;
    }

    pthread_cond_signal(&sqCond);

    if(send_thread!=NULL&&send_thread!=NULL) {
        void * ret;
        pthread_join(send_thread, &ret);
        send_thread = (pthread_t)-1;
    }
    return 0;
}

vl_status UdpEndPoint::stop() {
    sending = VL_FALSE;
    recving = VL_FALSE;

    pthread_cond_signal(&sqCond);
    return 0;
}

vl_status UdpEndPoint::registerSendQueue(IOQueue* ioq) {
    /* 注册的协议不存在析器 */
    EProtocol proto = (EProtocol)ioq->getProtocal();
    if (ioq->getProtocal() >= EPROTO_MAX) {
        printf("register send  ioq protocol not support\n\n");
        return VL_ERR_EPT_REG_INVALID;
    }
    pthread_mutex_lock(&sqLock);
    list<IOQueue*>::iterator it = this->sendIOQs[proto].begin();
    while(it != this->sendIOQs[proto].end()) {
        if(ioq == (*it))
            break;
        it++;
    }
    if(it == this->sendIOQs[proto].end()) {
        sendIOQs[ioq->getProtocal()].push_back(ioq);
        ioq->setObserver(this);
        if(EPROTO_RTP == proto){
            int sockfd = getSockFd();
            if(sockfd > 0)
            {
                int i=8192;
                setsockopt (sockfd, SOL_SOCKET, SO_SNDBUF, (char *) &i, sizeof (i));
                printf("SETFDD\n");
                printf("--------【jni】-这部设置没执行----------------\n");
            }
        }
    }
    //    LOGE("4444\n");
    pthread_mutex_unlock(&sqLock);
//    printf("registerSendQueue ioq %d, size=%d\n", ioq->getProtocal(), sendIOQs[ioq->getProtocal()].size());
    
    return 0;
}

vl_status UdpEndPoint::registerRecvQueue(IOQueue* ioq) {

    EProtocol proto = (EProtocol)ioq->getProtocal();
    /* 注册的协议不存在析器 */
    if (ioq->getProtocal() >= EPROTO_MAX) {
        printf("udp register receive queue protocol not support\n\n");
        return VL_ERR_EPT_REG_INVALID;
    }

    pthread_mutex_lock(&rqLock);
    list<IOQueue*>::iterator it = this->recvIOQs[proto].begin();
    while (it != this->recvIOQs[proto].end())
    {
        if(ioq == (*it))
            break;
        it++;
    }
    if(it == this->recvIOQs[proto].end())
    {
//        printf()<< "recvioq push_back"<<ioq->getProtocal();
        recvIOQs[ioq->getProtocal()].push_back(ioq);
    }
    pthread_mutex_unlock(&rqLock);
    disposeCache(ioq);
    return 0;
}

vl_status UdpEndPoint::unregisterSendQueue(IOQueue* ioq) {
    if(NULL == ioq) {
        printf("unregister recvq failed, ioq=null\n\n");
        return VL_ERR_EPT_PROTO_INVALID;
    }

    EProtocol proto = (EProtocol)ioq->getProtocal();
    printf("unregister send queue, protoid=%d\n", proto);
    if(proto >= EPROTO_MAX) {
        printf("udp unregister sendq protocol not support\n");
        return VL_ERR_EPT_REG_INVALID;
    }

    pthread_mutex_lock(&sqLock);
    list<IOQueue*>::iterator it = this->sendIOQs[proto].begin();
    while(it != this->sendIOQs[proto].end()) {
        if(ioq == (*it)) {
            sendIOQs[proto].erase(it);
            printf("unregister send queue success\n\n");
            break;
        }
        it++;
    }
    pthread_mutex_unlock(&sqLock);
    return 0;
}

vl_status UdpEndPoint::unregisterRecvQueue(IOQueue* ioq) {
    if(NULL == ioq) {
        printf("unregister recvq failed, ioq=null\n");
        return VL_ERR_EPT_PROTO_INVALID;
    }

    EProtocol proto = (EProtocol)ioq->getProtocal();
//    printf("unregister recv queue, protoid=%d\n", proto);
    if(proto >= EPROTO_MAX) {
        printf("udp unregister sendq protocol not support\n\n");
        return VL_ERR_EPT_REG_INVALID;
    }

    pthread_mutex_lock(&rqLock);
    list<IOQueue*>::iterator it = this->recvIOQs[proto].begin();
    while (it != this->recvIOQs[proto].end()) {
        if(ioq == (*it)) {
            recvIOQs[proto].erase(it);
            printf("unregister recv queue success\n\n");
            break;
        }
        it++;
    }
    pthread_mutex_unlock(&rqLock);
    return 0;

}

void UdpEndPoint::onIOQueueUpdated()
{
//    printf("onIOQueueUpdated   1111111111111111111\n") ;
    pthread_cond_signal(&sqCond);
}

vl_status UdpEndPoint::addProFilter(EptProtocol* pto)
{
    EProtocol protoId = pto->getProtocol();
    /* 注册的协议不存在析器 */
    if (protoId >= EPROTO_MAX) {
        printf("UdpEndPoint add unknown protocol\n\n");
        return VL_ERR_EPT_REG_INVALID;
    }
    protocols[protoId] = pto;
    return VL_SUCCESS;
}








