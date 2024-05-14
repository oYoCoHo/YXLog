#include "StreamPlayScheduler.hpp"
#include <printf.h>
#include "TestObject.hpp"

// TODO: 叶炽华 联调
#include "conference.h"


StreamPlayScheduler* StreamPlayScheduler::instance = NULL;




void* streamplay_monitor_looper(void* param) {
    if (NULL == param) {
        printf("StreamPlayScheduler looper loop failed, param = null !!!!\n");
        return NULL;
    }
    
    StreamPlayScheduler* monitor = (StreamPlayScheduler*) param;
    time_t tt;
    struct timeval tv;
    
    int number = 0;
    
    int nullNum = 0;
    
    time_t startTime = 0;
    
    while (VL_TRUE == monitor->working)
    {
//        printf("[MUTEX] LOCK %s %p\n",__FUNCTION__,&monitor->evMutex);
        time(&tt);
        struct timespec out;
        
        gettimeofday(&tv, NULL);
        
        if (number == 1) {
            time(&startTime);
        }
        
        
        out.tv_sec = tv.tv_sec + 2;
        out.tv_nsec = tv.tv_usec * 1000 + 2000000;
        pthread_mutex_lock(&monitor->evMutex);
        pthread_cond_timedwait(&monitor->wakeCond, &monitor->evMutex, &out);
        if (VL_TRUE == monitor->working)
        {
            // 优先处理录音相关事件
            monitor->schedualRecord();
//            printf("streamplay_monitor_looper执行schedualPlay函数\n");
            monitor->schedualPlay();
            
            if(NULL != monitor->activeSouce){
                //这是有语音内容的时候
                number++;
                nullNum = 0;
                
                time_t now;
                time(&now);
                printf("开始时间是startTime>>>>%ld, 结束时间是now>>>>>%ld \n", startTime, now);
                if (number > 1) {
                    double diff = difftime(now, startTime);
//                    if (diff >= 63.0)
//                    {
//                        monitor->activeSouce->spsSetPlayState(SPS_NO_MORE);
//                        startTime = now;
//                    }
                }
                
                
            }else{
                //这是activeSouce为null，但是跳不出去的时候
                
                if (number > 1) {
//                    monitor->activeSouce->spsSetPlayState(SPS_NO_MORE);
                    time(&startTime);
                }
                
                number = 0;
                nullNum++;
            }
            
        }
        pthread_mutex_unlock(&monitor->evMutex);
//        printf("[MUTEX] UNLOCK %s %p\n",__FUNCTION__,&monitor->evMutex);
        if(number > 300){
            printf("计算器到300了\n");
            monitor->activeSouce->spsSetPlayState(SPS_NO_MORE);
            number = 0;
        }
//        if(nullNum > 100){
//            printf("计算器到100了\n");
//            nullNum = 0;
//            monitor->wakeup();
////            pthread_mutex_lock(&monitor->evMutex);
////            monitor->pauseState = EP_PAUSING;
////            monitor->waitPause = canDelay;
////            monitor->dropWhenPause = drop;
////            pthread_mutex_unlock(&monitor->evMutex);
//            
//        }
        
    }
    
    return NULL;
}





void StreamPlayScheduler::schedualPlay()
{
//    printf("进入schedualPlay进行处理播放\n");
    if (EP_PAUSING == pauseState) {
        printf("schedualplay schedualPlay pausing .....\n");
        if (!waitPause) { // 立刻暂停
            pausePlayInternal();
        }
        pauseState = EP_PAUSED;
    } else if (EP_RESUMING == pauseState) {
        printf("schedualplay schedualPlay resuming .....\n");
        resumePlayInternal();
        pauseState = EP_PLAY;
    }
    
    /* 处理上一个播放 */
//    printf("处理上一个播放 \n");
    processSourceEnd();
    //    printf("schedualplay schedualPlay processSourceEnd .....\n");
    /* 选举下一个播放流程，有抢占的优先抢占，无抢占情况下，需要在EP_PLAY状态才选举 */
    if (NULL != preemptionSource) { // 抢占比暂停优先
        /* 处理抢占 */
        processPreemption();
    }
    
    if (EP_PLAY == pauseState) {
        if (SPS_NORMAL_PERMIT != engrossId) {
            /* 处理独占 */
            processEngross();
        } else {
            /* 处理普通优先级 */
            printf("pauseState的状态>>>>>>>%d,  engrossId>>>>>>>%d\n", pauseState, engrossId);
            processNormal();
        }
    } else if(EP_PAUSED == pauseState) {
        /* 不缓冲接收流，将语音流标记为已播放 */
        if(VL_TRUE == dropWhenPause) {
            for(auto iter = sourceList.begin(); iter != sourceList.end(); iter ++) {
                StreamPlaySource* source = (*iter);
                if(SPS_WAITING == source->spsGetPlayState()) {
                    source->spsMarkPlayed();
                }
            }
        }
    } else {
        printf("StreamPlayScheduler schedualPlay pausing or resuming .....\n");
    }
    
    /* 回收资源 */
    if (sourceList.size() > 0) {
        list<StreamPlaySource*>::iterator iter = sourceList.begin();
        
        for (; iter != sourceList.end();) {
            vl_bool erased = VL_FALSE;
            StreamPlaySource* source = (*iter);
            //            if (SPS_PLAYED == source->spsGetPlayState()) {
            if (VL_TRUE == source->spsReadyForDestroy()) {
                /* 释放资源 */
                source->spsDispose();
                /* 从队列移除 */
                sourceList.erase(iter++);
                erased = VL_TRUE;
                //                printf("StreamPlayScheduler schedual list size = %u \n", sourceList.size());
                /* 通知观察者 */
                if (NULL != observer) {
                    struct SpsResultInfo info;
                    source->spsGetResultInfo(info);
                    observer->onRelease(source->spsGetIdentify(), info);
                    printf("StreamPlayScheduler onRelease .....\n");
                }
                delete source;
            }
            //            }
            
            if (VL_FALSE == erased) {
                iter++;
            }
        }
    }
}

void StreamPlayScheduler::processPreemption() {
    /* 处理抢占类型 */
    printf("processPreemption >>>>>> 处理抢占类型 \n");
    if (NULL != preemptionSource) {
        processSourceEnd(); // 掐断上一个播放
        if (VL_SUCCESS == preemptionSource->spsStartTrack()) {
            activeSouce = preemptionSource;
            if (NULL != observer) {
                observer->onPlayStarted(preemptionSource->spsGetIdentify());
                // TODO: 叶炽华 联调
                TestObject().testFunction(recvBuff);
                // TODO: 叶炽华 联调
                TestObject().playFunction();
            }
            printf("StreamPlayScheduler raise a Preemption source !!!\n");
        } else {
            printf("StreamPlayScheduler schedual to play ssrc %d failed !!!\n", preemptionSource->spsGetIdentify());
        }
        preemptionSource = NULL;
    }
}



void StreamPlayScheduler::processEngross() {
    /* 处理独占类型 */
    if (NULL == activeSouce) {
        list<StreamPlaySource*>::iterator iter = sourceList.begin();
        
        /* 遍历托管中的会话，选出一个连续播放时长达标的会话播放 */
        while (iter != sourceList.end()) {
            if (SPS_WAITING == (*iter)->spsGetPlayState() && (*iter)->spsReadyForPlay()
                && engrossId == (*iter)->spsGetPermitId()) {
                /* 已有一个会话达标 */
                StreamPlaySource* source = (*iter);
                if (VL_SUCCESS == source->spsStartTrack()) {
                    activeSouce = source;
                    if (NULL != observer) {
//                        observer->onPlayStarted(source->spsGetIdentify());
//                        // TODO: 叶炽华 联调
//                        TestObject().testFunction(recvBuff);
//                        // TODO: 叶炽华 联调
//                        TestObject().playFunction();
                        // TODO: song
                        incoming_trigger_( source->spsGetIdentify());
                    }
                    printf("StreamPlayScheduler engross source finded and raised, eid = %d, id = %d \n",
                           source->spsGetPermitId(), source->spsGetIdentify());
                    break;
                } else {
                    printf("StreamPlayScheduler engross source finded but raise failed, eid = %d, id = %d\n ",
                           source->spsGetPermitId(), source->spsGetIdentify());
                }
            }
            iter++;
        }
    }
}


void StreamPlayScheduler::processNormal() {
//    /* 选举下一个可获取播放设备的会话 */
//    if (NULL == activeSouce) {
//        list<StreamPlaySource*>::iterator iter = sourceList.begin();
//        
//        /* 遍历托管中的会话，选出一个连续播放时长达标的会话播放 */
//        while (iter != sourceList.end()) {
//            if (SPS_WAITING == (*iter)->spsGetPlayState() && (*iter)->spsReadyForPlay()) {
//                /* 已有一个会话达标 */
//                StreamPlaySource* source = (*iter);
//                if (VL_SUCCESS == source->spsStartTrack()) {
//                    activeSouce = source;
//                    
//                    if (NULL != observer) {
//                        observer->onPlayStarted(source->spsGetIdentify());
//                    }
//                    printf("StreamPlayScheduler raise a listen transaction !!!\n");
//                    break;
//                } else {
//                    printf("StreamPlayScheduler schedual to play ssrc %d failed !!!\n", source->spsGetIdentify());
//                }
//            }
//            iter++;
//        }
//    }
    
    printf("进入processNormal----------\n");
    /* 处理独占类型 */
        if (NULL == activeSouce) {
            bool found = false; // 标记是否找到符合条件的会话
            for (auto& source : sourceList) {
                if (SPS_WAITING == source->spsGetPlayState() && source->spsReadyForPlay()
                    && engrossId == source->spsGetPermitId()) {
                    /* 已有一个会话达标 */
                    if (VL_SUCCESS == source->spsStartTrack()) {
                        activeSouce = source;
                        if (NULL != observer) {
//                            observer->onPlayStarted(source->spsGetIdentify());
//                            // TODO: 叶炽华 联调
//                            TestObject().testFunction(recvBuff);
//                            // TODO: 叶炽华 联调
//                            TestObject().playFunction();
                            // TODO: song
                            incoming_trigger_( source->spsGetIdentify());
                        }
                        printf("StreamPlayScheduler engross source finded and raised, eid = %d, id = %d \n",
                               source->spsGetPermitId(), source->spsGetIdentify());
                        found = true;
                        break;
                    } else {
                        printf("StreamPlayScheduler engross source finded but raise failed, eid = %d, id = %d\n ",
                               source->spsGetPermitId(), source->spsGetIdentify());
                    }
                }
            }

            if (!found) {
                // 如果没有找到符合条件的会话，则直接返回，避免继续遍历sourceList
//                vl_uint32 id = activeSouce->spsGetIdentify();
//                if (NULL != observer)
//                {
//                    observer->onPlayStoped(id);
//                }
               
                pausePlayInternal();
//                stopRecordAsync();
                wakeup();
                if (NULL != activeSouce){
                    printf("释放语音设备\n");
                    activeSouce->spsStopTrack();
                    activeSouce = NULL;
                    vl_uint32 id = activeSouce->spsGetIdentify();
                    if (NULL != observer)
                    {
                        observer->onPlayStoped(id);
                        
                        // TODO：叶炽华 联调
                        time_t stoptime;
                        time(&stoptime);
                        char buff[100];
                        sprintf(buff, "%d,%ld",0,stoptime);
                        TestObject().stopFunction(buff);
                        locale_afilePlay=0;
                        memset(recvBuff, 0, sizeof(recvBuff));
                    }
                }else{

                }
                return;
            }
        }
}


void StreamPlayScheduler::processSourceEnd() {
    /* 处理已播放结束的源 */
    if (NULL != activeSouce) {
        
        printf("进入判断，activeSouce->spsGetPlayState()的状态是什么：%u\n", activeSouce->spsGetPlayState());
        
        vl_bool isStopped = (SPS_NO_MORE == activeSouce->spsGetPlayState());
        vl_bool isEngross = false;
        vl_bool isPreemption = (NULL != preemptionSource);
        printf("【停止活动】---处理已播放结束的源\n isStopped:%d  isEngross:%d  isPreemption:%d\n",isStopped,isEngross,isPreemption);
        if (SPS_NORMAL_PERMIT != engrossId)
        {
            /* 处理独占 */
            printf("处理独占\n");
            if (engrossId != activeSouce->spsGetPermitId()) {
                // 存在问题：如果通话前群聊有人在说话，后面会被掐断???
                isEngross = true;
            }
        }
        if (isStopped || isPreemption || isEngross)
        {
            vl_uint32 id = activeSouce->spsGetIdentify();
            /* 释放语音设备 */
            printf("释放语音设备\n");
            activeSouce->spsStopTrack();
            activeSouce = NULL;
            
            if (NULL != observer)
            {
                observer->onPlayStoped(id);
                
                // TODO：叶炽华 联调
                time_t stoptime;
                time(&stoptime);
                char buff[100];
                sprintf(buff, "%d,%ld",0,stoptime);
                TestObject().stopFunction(buff);
                locale_afilePlay=0;
                memset(recvBuff, 0, sizeof(recvBuff));
            }
            printf("【停止活动】StreamPlayScheduler active source stop track !!! stop:%s, preemption:%s, engross:%s\n",
                   isStopped ? "true" : "false", isPreemption ? "true" : "false", isEngross ? "true" : "false");
        }else{
            printf("没有释放\n");
        }
        
    }
}


StreamPlayScheduler::StreamPlayScheduler() :
wakeCond(PTHREAD_COND_INITIALIZER),
evMutex(PTHREAD_MUTEX_INITIALIZER),
workStateMutex(PTHREAD_MUTEX_INITIALIZER),
working(VL_FALSE), activeSouce(NULL),
observer(NULL), workThread(),
preemptionSource(NULL),
pauseState(EP_PLAY),
waitPause(false),
engrossId(SPS_NORMAL_PERMIT),
recSource(NULL){
    
}

StreamPlayScheduler::~StreamPlayScheduler() {
    stopWork();
    pthread_cond_destroy(&wakeCond);
    pthread_mutex_destroy(&evMutex);
    pthread_mutex_destroy(&workStateMutex);
}



vl_bool StreamPlayScheduler::delegate(StreamPlaySource *source, vl_bool preemption)
{
    printf("\n----数据流开始传输............\n");
    if (NULL == source)
    {
        printf("StreamPlayScheduler try to delegate incoming transaction that is null !!!!!!\n");
        return VL_FALSE;
    }
    if (VL_TRUE != working)
    {
        printf("StreamPlayScheduler delegate when looper is not working !!!!!!!!!!\n");
        return VL_FALSE;
    }
    printf("\nStreamPlayScheduler delegate incomging ssrc=%d\n", source->spsGetIdentify());
    
    if(NULL != recSource)
    {
        printf("recSource->srsGetRecrodState的状态是>>>>>>%u\n", recSource->srsGetRecrodState());
        
        SRS_STATE state = recSource->srsGetRecrodState();
        
        if (state == SRS_RECORING || state == SRS_WAITING || state == SRS_INITIALED) {
            printf("recSource->srsGetRecrodState的状态正在录音\n");
            return VL_FALSE;
        }
    }
    
    vl_bool exist = VL_FALSE;
    
    printf("---------pthread mutex lock evMutex..........\n");
    pthread_mutex_lock(&evMutex);
    
    /* 抢占方式 */
    if (VL_TRUE == preemption) {
        preemptionSource = source;
    }
    
#if 0
    /* 查找是否有相同的id在列表中 */
    list<StreamPlaySource*>::iterator iter = sourceList.begin();

    while(iter != sourceList.end()) {
        if((*iter) != NULL && (*iter)->spsGetIdentify() == source->spsGetIdentify()) {
            LOGE("StreamPlayScheduler delegate incoming transaction that has same ssrc exist !!!!!!!!!!");
            exist = VL_TRUE;
            break;
        }
        iter ++;
    }
#endif
    
    /* 若没有找到，直接增加 */
    if (VL_FALSE == exist)
    {
        sourceList.push_back(source);
        source->spsStandby();
    }
    /* 唤醒调度器 */
    wakeup();
    pthread_mutex_unlock(&evMutex);
    printf("--------StreamPlayScheduler , unlock mutex success.\n ");
    
    if (exist == VL_TRUE) {
        return VL_FALSE;
    } else {
        return VL_TRUE;
    }
}



vl_bool StreamPlayScheduler::startWork()
{
    printf("StreamPlayScheduler::startWork.\n");
    pthread_mutex_lock(&workStateMutex);
    if (VL_TRUE == working) {
        return VL_TRUE;
    }
    working = VL_TRUE;
    if (0 != pthread_create(&workThread, NULL, streamplay_monitor_looper, this))
    {
        working = VL_FALSE;
    }
    pthread_mutex_unlock(&workStateMutex);
    printf("pthread_mutex_unlock.............workStateMutex.......\n");
    return working;
}


void StreamPlayScheduler::stopWork()
{
    printf("stopWork--pthread mutex lock evMutex..........\n");
    pthread_mutex_lock(&evMutex);
    working = VL_FALSE;
    wakeup();
    pthread_mutex_unlock(&evMutex);
    printf("stopWork--pthread mutex unlock evMutex..........\n");
}

StreamPlayScheduler* StreamPlayScheduler::getInstance() {
    if (instance == NULL) {
        instance = new StreamPlayScheduler();
    }
    return instance;
}

void StreamPlayScheduler::wakeup() {
    pthread_cond_signal(&wakeCond);
}


void StreamPlayScheduler::wakeupSeraial()
{
    printf("wakeupSeraial--pthread mutex lock evMutex..........\n");
    pthread_mutex_lock(&evMutex);
    pthread_cond_signal(&wakeCond);
    pthread_mutex_unlock(&evMutex);
    printf("wakeupSeraial--pthread mutex lock evMutex..........\n");
}



void StreamPlayScheduler::terminatePlay() {
    printf("StreamPlayScheduler::terminatePlay");
    if (NULL != activeSouce) {
        printf("StreamPlayScheduler::terminatePlay*********");
        activeSouce->spsSetPlayState(SPS_NO_MORE);
        wakeup();
    }
}


void StreamPlayScheduler::terminatePlay(vl_uint32 ssrc) {
    printf("StreamPlayScheduler::terminatePlay()");
    if (NULL != activeSouce && activeSouce->spsGetIdentify() == ssrc) {
        printf("StreamPlayScheduler::terminatePlay()*************");
        activeSouce->spsSetPlayState(SPS_NO_MORE);
        wakeup();
    }
}


void StreamPlayScheduler::waitingPlay(vl_uint32 ssrc) {
    if (NULL != activeSouce && activeSouce->spsGetIdentify() == ssrc) {
        activeSouce->spsSetPlayState(SPS_TALKING);
        wakeup();
    }
}


//没有的加进来
vl_bool StreamPlayScheduler::pausePlay(vl_bool canDelay, vl_bool drop) {
    if (EP_PAUSING == pauseState || EP_PAUSED == pauseState) {
        return VL_TRUE;
    } else if (EP_RESUMING == pauseState) {
        return VL_FALSE;
    } else {
        printf("StreamPlaySchedualer trigger pause !\n");
        pthread_mutex_lock(&evMutex);
        pauseState = EP_PAUSING;
        this->waitPause = canDelay;
        this->dropWhenPause = drop;
        pthread_mutex_unlock(&evMutex);
        wakeup();
        return VL_TRUE;
    }
}


//没有的加进来
vl_bool StreamPlayScheduler::resumePlay() {
    if (EP_PLAY == pauseState || EP_RESUMING == pauseState) {
        return VL_TRUE;
    } else if (EP_PAUSING == pauseState) {
        return VL_FALSE;
    } else {
        pthread_mutex_lock(&evMutex);
        pauseState = EP_RESUMING;
        waitPause = false;
        dropWhenPause = true;
        pthread_mutex_unlock(&evMutex);
        wakeup();
        return VL_TRUE;
    }
}






void StreamPlayScheduler::pausePlayInternal()
{
    printf("StreamPlayScheduler pause play\n");
    if (NULL != activeSouce) {
        activeSouce->spsPauseTrack();
    }
}

void StreamPlayScheduler::resumePlayInternal()
{
    printf("StreamPlayScheduler resume play\n");
    if (NULL != activeSouce) {
        activeSouce->spsResumeTrack();
    }
}



void StreamPlayScheduler::enableEngross(vl_uint32 engrossId)
{
    pthread_mutex_lock(&evMutex);
    this->engrossId = engrossId;
    pthread_mutex_unlock(&evMutex);
    wakeup();
}

void StreamPlayScheduler::clearEngross()
{
    pthread_mutex_lock(&evMutex);
    this->engrossId = SPS_NORMAL_PERMIT;
    pthread_mutex_unlock(&evMutex);
    wakeup();
}



vl_bool StreamPlayScheduler::delegate(StreamRecordSource* source)
{
    vl_bool result = VL_FALSE;
    pthread_mutex_lock(&evMutex);
    if(NULL != recSource) {
        printf("recSource release\n");
        if(recSource->srsReadyForDestroy()) {
            printf("StreamPlayScheduler release PREVIOUS record source \n");
            delete recSource;
            recSource = NULL;
        }
    }
    
    if (NULL != preemptionSource || NULL != activeSouce || EP_PLAY == pauseState) {
        printf("preemptionSource is start \n");
//        TestObject().offTalkFunction();
//        return result;
    }
    
    if(NULL == recSource)
    {
        printf("recSource is null \n");
        if(VL_SUCCESS == source->srsStandby())
        {
            recSource = source;
            result = VL_TRUE;
            wakeup();
        }
    }
    pthread_mutex_unlock(&evMutex);
    return result;
}


void StreamPlayScheduler::schedualRecord()
{
    
    if(NULL != recSource)
    {
        printf("recSource->srsGetRecrodState的状态是>>>>>>%u\n", recSource->srsGetRecrodState());
        
        SRS_STATE state = recSource->srsGetRecrodState();
        switch(state)
        {
            case SRS_WAITING:
                if(recSource->srsReadyForRecord())
                {
                    recSource->srsStartRecord();
//                    pausePlay(VL_TRUE, VL_FALSE);
                }
                break;
            case SRS_INITIALED:
            case SRS_INVALID:
            case SRS_RECORING:
            {
                
                break;
            }
            case SRS_STOPING:
                if(recSource->srsReadyForDispose())
                {
                    recSource->srsDispose();
//                    resumePlay();
                }
                break;
            case SRS_STOPED:
                if(recSource->srsReadyForDestroy())
                {
                    resumePlayInternal();
                    delete recSource;
                    recSource = NULL;
                }
                break;
        }
    }
}


vl_bool StreamPlayScheduler::stopRecordAsync()
{
//    if (pthread_mutex_trylock(&evMutex) != 0) {
//        pthread_mutex_unlock(&evMutex);
//    }
    
    pthread_mutex_lock(&evMutex);
    if(NULL != recSource)
    {
        printf("streamplay stop record........\n");
        recSource->srsStopRecord(true);
        wakeup();
    }
    pthread_mutex_unlock(&evMutex);
    
    time_t starttime;
    time(&starttime);
    printf("【接收---stopRecordAsync】当前时间戳：%ld\n",starttime);
    
    return VL_TRUE;
}





















