#include <csignal>
#include <cstdint>
#include <iostream>
#include <sstream> 
#include <string>
#include <vector> 
#include <algorithm> 
#include "IAgoraLinuxSdkCommon.h"
#include "IAgoraRecordingEngine.h"
#include "AgoraSdk.h"

#include "base/atomic.h"
#include "base/log.h" 
#include "base/opt_parser.h" 
namespace agora {

AgoraSdk::AgoraSdk(): IRecordingEngineEventHandler() 
    , m_level(agora::linuxsdk::AGORA_LOG_LEVEL_INFO)
{
  m_engine = NULL;
  m_stopped.store(false);
  m_storage_dir = "./";
  m_layoutMode = DEFAULT_LAYOUT;
  m_maxVertPreLayoutUid = -1;
}

AgoraSdk::~AgoraSdk() {
  if (m_engine) {
    m_engine->release();
  }
}

bool AgoraSdk::stopped() const {
  return m_stopped;
}

bool AgoraSdk::release() {
  if (m_engine) {
    m_engine->release();
    m_engine = NULL;
  }

  return true;
}

bool AgoraSdk::createChannel(const string &appid, const string &channelKey, const string &name,
    uint32_t uid, 
    agora::recording::RecordingConfig &config) 
{
    if ((m_engine = agora::recording::IRecordingEngine::createAgoraRecordingEngine(appid.c_str(), this)) == NULL)
        return false;

    m_engine->setLogLevel(m_level);

    if(linuxsdk::ERR_OK != m_engine->joinChannel(channelKey.c_str(), name.c_str(), uid, config))
        return false;

    //m_engine->setUserBackground(30000, "test.jpg");

    m_config = config;
    return true;
}

bool AgoraSdk::leaveChannel() {
  if (m_engine) {
    m_engine->leaveChannel();
    m_stopped = true;
  }

  return true;
}

bool AgoraSdk::stoppedOnError() {
  if (m_engine) {
    m_engine->stoppedOnError();
    m_stopped = true;
  }

  return true;
}

int AgoraSdk::startService() {
  if (m_engine) 
    return m_engine->startService();

  return 1;
}

int AgoraSdk::stopService() {
  if (m_engine) 
    return m_engine->stopService();

  return 1;
}

//Customize the layout of video under video mixing model
int AgoraSdk::setVideoMixLayout()
{
    recording::RecordingConfig *pConfig = getConfigInfo();
    size_t max_peers = pConfig->channelProfile == linuxsdk::CHANNEL_PROFILE_COMMUNICATION ? 7:17;
    if(!m_mixRes.m_videoMix) return 0;

    LAYOUT_MODE_TYPE layout_mode = m_layoutMode;
    uint32_t maxResolutionUid = m_maxVertPreLayoutUid;
    CM_LOG_DIR(m_logdir.c_str(), INFO, "setVideoMixLayout: user size: %d, permitted max_peers:%d, layout mode:%d, maxResolutionUid:%ld", m_peers.size(), max_peers, layout_mode, maxResolutionUid);

    agora::linuxsdk::VideoMixingLayout layout;
    layout.canvasWidth = m_mixRes.m_width;
    layout.canvasHeight = m_mixRes.m_height;
    layout.backgroundColor = "#23b9dc";

    layout.regionCount = static_cast<uint32_t>(m_peers.size());

    if (!m_peers.empty()) {

        CM_LOG_DIR(m_logdir.c_str(), INFO, "setVideoMixLayout: peers not empty");
        agora::linuxsdk::VideoMixingLayout::Region * regionList = new agora::linuxsdk::VideoMixingLayout::Region[m_peers.size()];
        if(layout_mode == BESTFIT_LAYOUT) {
            adjustBestFitVideoLayout(regionList);
        }else if(layout_mode == VERTICALPRESENTATION_LAYOUT) {
            adjustVerticalPresentationLayout(maxResolutionUid, regionList);
        }else {
            adjustDefaultVideoLayout(regionList);
        }
        layout.regions = regionList;

//    CM_LOG_DIR(m_logdir.c_str(), INFO, "region 0 uid: %d, x: %f, y: %f, width: %f, height: %f, zOrder: %d, alpha: %f", regionList[0].uid, regionList[0].x, regionList[0].y, regionList[0].width, regionList[0].height, regionList[0].zOrder, regionList[0].alpha);
    }
    else {
        layout.regions = NULL;
    }

    int res = setVideoMixingLayout(layout);
    if(layout.regions)
        delete []layout.regions;

    return res;
}

void AgoraSdk::setLogLevel(agora::linuxsdk::agora_log_level level)
{
    m_level = level;
}

int AgoraSdk::setVideoMixingLayout(const agora::linuxsdk::VideoMixingLayout &layout)
{
   int result = -agora::linuxsdk::ERR_INTERNAL_FAILED;
   if(m_engine)
      result = m_engine->setVideoMixingLayout(layout);
   return result;
}

int AgoraSdk::setUserBackground(agora::linuxsdk::uid_t uid, const char* image_path)
{
    int result = -agora::linuxsdk::ERR_INTERNAL_FAILED;
    if(m_engine)
        result = m_engine->setUserBackground(uid, image_path);
    return result;
}

const agora::recording::RecordingEngineProperties* AgoraSdk::getRecorderProperties(){
    return m_engine->getProperties();
}

void AgoraSdk::onErrorImpl(int error, agora::linuxsdk::STAT_CODE_TYPE stat_code) {
    cerr << "Error: " << error <<",with stat_code:"<< stat_code << endl;
    stoppedOnError();
}

void AgoraSdk::onWarningImpl(int warn) {
    cerr << "warn: " << warn << endl;
    if(static_cast<int>(linuxsdk::WARN_RECOVERY_CORE_SERVICE_FAILURE) == warn) {
    cerr << "clear peer list " << endl;
        m_peers.clear();
    }
    //  leaveChannel();
}

void AgoraSdk::onJoinChannelSuccessImpl(const char * channelId, agora::linuxsdk::uid_t uid) {
    cout << "join channel Id: " << channelId << ", with uid: " << uid << endl;
}

void AgoraSdk::onLeaveChannelImpl(agora::linuxsdk::LEAVE_PATH_CODE code) {
    cout << "leave channel with code:" << code << endl;
}

void AgoraSdk::onUserJoinedImpl(unsigned uid, agora::linuxsdk::UserJoinInfos &infos) {
    cout << "User " << uid << " joined, RecordingDir:" << (infos.storageDir? infos.storageDir:"NULL") <<endl;
    if(infos.storageDir)
    {
        m_logdir = m_storage_dir;
    }

    m_peers.push_back(uid);

    //When the user joined, we can re-layout the canvas
    setVideoMixLayout();
}


void AgoraSdk::onUserOfflineImpl(unsigned uid, agora::linuxsdk::USER_OFFLINE_REASON_TYPE reason) {
    cout << "User " << uid << " offline, reason: " << reason << endl;
    m_peers.erase(std::remove(m_peers.begin(), m_peers.end(), uid), m_peers.end());

    //When the user is offline, we can re-layout the canvas
    setVideoMixLayout();
}

void AgoraSdk::audioFrameReceivedImpl(unsigned int uid, const agora::linuxsdk::AudioFrame *pframe) const
{
  char uidbuf[65];
  snprintf(uidbuf, sizeof(uidbuf),"%u", uid);
  std::string info_name = m_storage_dir + std::string(uidbuf) /*+ timestamp_per_join_*/;

  const uint8_t* data = NULL;
  uint32_t size = 0;
  if (pframe->type == agora::linuxsdk::AUDIO_FRAME_RAW_PCM) {
    info_name += ".pcm";

    agora::linuxsdk::AudioPcmFrame *f = pframe->frame.pcm;
    data = f->pcmBuf_;
    size = f->pcmBufSize_;

    cout << "User " << uid << ", received a raw PCM frame ,channels:" << f->channels_  << std::endl;

  } else if (pframe->type == agora::linuxsdk::AUDIO_FRAME_AAC) {
    static uint32_t s_channels = 0;
    static uint32_t index = 0;
 
    agora::linuxsdk::AudioAacFrame *f = pframe->frame.aac;
    data = f->aacBuf_;
    size = f->aacBufSize_;  

    if(s_channels != f->channels_) {
        s_channels = f->channels_;
        ++index;
    }
    if(index != 1) {
        char indexBuf[65];
        snprintf(indexBuf, sizeof(indexBuf),"%u", index);      
        info_name += "_" + std::string(indexBuf);
    }
    info_name += ".aac";

    cout << "User " << uid << ", received an AAC frame" << std::endl;

  }

  FILE *fp = fopen(info_name.c_str(), "a+b");
  if(fp == NULL) {
      cout << "failed to open: " << info_name;
      cout<< " ";
      cout << "errno: " << errno;
      cout<< endl;
      return;
  }

  ::fwrite(data, 1, size, fp);
  ::fclose(fp);
}

void AgoraSdk::videoFrameReceivedImpl(unsigned int uid, const agora::linuxsdk::VideoFrame *pframe) const {
  char uidbuf[65];
  snprintf(uidbuf, sizeof(uidbuf),"%u", uid);
  const char * suffix=".vtmp";
  if (pframe->type == agora::linuxsdk::VIDEO_FRAME_RAW_YUV) {
    agora::linuxsdk::VideoYuvFrame *f = pframe->frame.yuv;
    suffix=".yuv";

    cout << "User " << uid << ", received a yuv frame, width: "
        << f->width_ << ", height: " << f->height_ ;
    cout<<",ystride:"<<f->ystride_<< ",ustride:"<<f->ustride_<<",vstride:"<<f->vstride_ << std::endl;
    
  } else if(pframe->type == agora::linuxsdk::VIDEO_FRAME_JPG) {
    suffix=".jpg";
    agora::linuxsdk::VideoJpgFrame *f = pframe->frame.jpg;

    cout << "User " << uid << ", received an jpg frame, timestamp: "
    << f->frame_ms_ << std::endl;

    struct tm date;
    time_t t = time(NULL);
    localtime_r(&t, &date);
    char timebuf[128];
    sprintf(timebuf, "%04d%02d%02d%02d%02d%02d", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);
    std::string file_name = m_storage_dir + std::string(uidbuf) + "_" + std::string(timebuf) + suffix;
    FILE *fp = fopen(file_name.c_str(), "w");
    if(fp == NULL) {
        cout << "failed to open: " << file_name;
        cout<< " ";
        cout << "errno: " << errno;
        cout<< endl;
        return;
    }

    ::fwrite(f->buf_, 1, f->bufSize_, fp);
    ::fclose(fp);
    return;
  } else {
    suffix=".h264";
    agora::linuxsdk::VideoH264Frame *f = pframe->frame.h264;

    cout << "User " << uid << ", received an h264 frame, timestamp: "
        << f->frame_ms_ << ", frame no: " << f->frame_num_ 
        << std::endl;
  }

  std::string info_name = m_storage_dir + std::string(uidbuf) /*+ timestamp_per_join_ */+ std::string(suffix);
  FILE *fp = fopen(info_name.c_str(), "a+b");
  if(fp == NULL) {
        cout << "failed to open: " << info_name;
        cout<< " ";
        cout << "errno: " << errno;
        cout<< endl;
        return;
  }


  //store it as file
  if (pframe->type == agora::linuxsdk::VIDEO_FRAME_RAW_YUV) {
      agora::linuxsdk::VideoYuvFrame *f = pframe->frame.yuv;
      ::fwrite(f->buf_, 1, f->bufSize_, fp);
  }
  else {
      agora::linuxsdk::VideoH264Frame *f = pframe->frame.h264;
      ::fwrite(f->buf_, 1, f->bufSize_, fp);
  }
  ::fclose(fp);

}

void AgoraSdk::onActiveSpeakerImpl(uid_t uid) {
    cout << "User: " << uid << " is speaking" << endl;
}

void AgoraSdk::adjustDefaultVideoLayout(agora::linuxsdk::VideoMixingLayout::Region * regionList) {

    regionList[0].uid = m_peers[0];
    regionList[0].x = 0.f;
    regionList[0].y = 0.f;
    regionList[0].width = 1.f;
    regionList[0].height = 1.f;
    regionList[0].zOrder = 0;
    regionList[0].alpha = 1.f;
    regionList[0].renderMode = 2;

    CM_LOG_DIR(m_logdir.c_str(), INFO, "region 0 uid: %u, x: %f, y: %f, width: %f, height: %f, zOrder: %d, alpha: %f", regionList[0].uid, regionList[0].x, regionList[0].y, regionList[0].width, regionList[0].height, regionList[0].zOrder, regionList[0].alpha);


    float canvasWidth = static_cast<float>(m_mixRes.m_width);
    float canvasHeight = static_cast<float>(m_mixRes.m_height);

    float viewWidth = 0.235f;
    float viewHEdge = 0.012f;
    float viewHeight = viewWidth * (canvasWidth/canvasHeight);
    float viewVEdge = viewHEdge * (canvasWidth/canvasHeight);

    for (size_t i=1; i<m_peers.size(); i++) {

        regionList[i].uid = m_peers[i];

        float xIndex = static_cast<float>((i-1) % 4);
        float yIndex = static_cast<float>((i-1) / 4);
        regionList[i].x = xIndex * (viewWidth + viewHEdge) + viewHEdge;
        regionList[i].y = 1 - (yIndex + 1) * (viewHeight + viewVEdge);
        regionList[i].width = viewWidth;
        regionList[i].height = viewHeight;
        regionList[i].zOrder = 0;
        regionList[i].alpha = static_cast<double>(i + 1);
        regionList[i].renderMode = 2;
    }
}

void AgoraSdk::setMaxResolutionUid(int number, unsigned int maxResolutionUid, agora::linuxsdk::VideoMixingLayout::Region * regionList, double weight_ratio) {
    regionList[number].uid = maxResolutionUid;
    regionList[number].x = 0.f;
    regionList[number].y = 0.f;
    regionList[number].width = 1.f * weight_ratio;
    regionList[number].height = 1.f;
    regionList[number].zOrder = number;
    regionList[number].alpha = 1.f;
    regionList[number].renderMode = 1;
}
void AgoraSdk::changeToVideo7Layout(unsigned int maxResolutionUid, agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    CM_LOG_DIR(m_logdir.c_str(), INFO,"changeToVideo7Layout");
    adjustVideo7Layout(maxResolutionUid, regionList);
}
void AgoraSdk::changeToVideo9Layout(unsigned int maxResolutionUid, agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    CM_LOG_DIR(m_logdir.c_str(), INFO,"changeToVideo9Layout");
    adjustVideo9Layout(maxResolutionUid, regionList);
}
void AgoraSdk::changeToVideo17Layout(unsigned int maxResolutionUid, agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    CM_LOG_DIR(m_logdir.c_str(), INFO,"changeToVideo17Layout");
    adjustVideo17Layout(maxResolutionUid, regionList);
}

void AgoraSdk::adjustVideo5Layout(unsigned int maxResolutionUid, agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    bool flag = false;
    CM_LOG_DIR(m_logdir.c_str(), INFO, "adjustVideo5Layou tegion 0 uid: %u, x: %f, y: %f, width: %f, height: %f, zOrder: %d, alpha: %f", regionList[0].uid, regionList[0].x, regionList[0].y, regionList[0].width, regionList[0].height, regionList[0].zOrder, regionList[0].alpha);

    float canvasWidth = static_cast<float>(m_mixRes.m_width);
    float canvasHeight = static_cast<float>(m_mixRes.m_height);

    float viewWidth = 0.235f;
    float viewHEdge = 0.012f;
    float viewHeight = viewWidth * (canvasWidth/canvasHeight);
    float viewVEdge = viewHEdge * (canvasWidth/canvasHeight);
    int number = 0;

    size_t i=0;
    for (; i<m_peers.size(); i++) {
        if(maxResolutionUid == m_peers[i]){
            CM_LOG_DIR(m_logdir.c_str(), INFO, "adjustVideo5Layout equal with configured user uid:%u", maxResolutionUid);
            flag = true;
            setMaxResolutionUid(number,  maxResolutionUid, regionList,0.8);
            number++;
            continue;
        }
        regionList[number].uid = m_peers[i];
        //float xIndex = ;
        float yIndex = flag?static_cast<float>(number-1 % 4):static_cast<float>(number % 4);
        regionList[number].x = 1.f * 0.8;
        regionList[number].y = (0.25) * yIndex;
        regionList[number].width = 1.f*(1-0.8);
        regionList[number].height = 1.f * (0.25);
        regionList[number].zOrder = 0;
        regionList[number].alpha = static_cast<double>(number);
        regionList[number].renderMode = 2;
        number++;
        if(i == 4 && !flag){
            changeToVideo7Layout(maxResolutionUid, regionList);
        }
    }

}
void AgoraSdk::adjustVideo7Layout(unsigned int maxResolutionUid, agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    bool flag = false;
    CM_LOG_DIR(m_logdir.c_str(), INFO, "adjustVideo7Layou tegion 0 uid: %u, x: %f, y: %f, width: %f, height: %f, zOrder: %d, alpha: %f", regionList[0].uid, regionList[0].x, regionList[0].y, regionList[0].width, regionList[0].height, regionList[0].zOrder, regionList[0].alpha);

    float canvasWidth = static_cast<float>(m_mixRes.m_width);
    float canvasHeight = static_cast<float>(m_mixRes.m_height);

    float viewWidth = 0.235f;
    float viewHEdge = 0.012f;
    float viewHeight = viewWidth * (canvasWidth/canvasHeight);
    float viewVEdge = viewHEdge * (canvasWidth/canvasHeight);
    int number = 0;

    size_t i=0;
    for (; i<m_peers.size(); i++) {
        if(maxResolutionUid == m_peers[i]){
            CM_LOG_DIR(m_logdir.c_str(), INFO, "adjustVideo7Layout equal with configured user uid:%u", maxResolutionUid);
            flag = true;
            setMaxResolutionUid(number,  maxResolutionUid, regionList,6.f/7);
            number++;
            continue;
        }
        regionList[number].uid = m_peers[i];
        float yIndex = flag?static_cast<float>(number-1 % 6):static_cast<float>(number % 6);
        regionList[number].x = 6.f/7;
        regionList[number].y = (1.f/6) * yIndex;
        regionList[number].width = (1.f/7);
        regionList[number].height = (1.f/6);
        regionList[number].zOrder = 0;
        regionList[number].alpha = static_cast<double>(number);
        regionList[number].renderMode = 2;
        number++;
        if(i == 6 && !flag){
            changeToVideo9Layout(maxResolutionUid, regionList);
        }
    }

}
void AgoraSdk::adjustVideo9Layout(unsigned int maxResolutionUid, agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    bool flag = false;
    CM_LOG_DIR(m_logdir.c_str(), INFO, "adjustVideo9Layou tegion 0 uid: %u, x: %f, y: %f, width: %f, height: %f, zOrder: %d, alpha: %f", regionList[0].uid, regionList[0].x, regionList[0].y, regionList[0].width, regionList[0].height, regionList[0].zOrder, regionList[0].alpha);

    float canvasWidth = static_cast<float>(m_mixRes.m_width);
    float canvasHeight = static_cast<float>(m_mixRes.m_height);

    float viewWidth = 0.235f;
    float viewHEdge = 0.012f;
    float viewHeight = viewWidth * (canvasWidth/canvasHeight);
    float viewVEdge = viewHEdge * (canvasWidth/canvasHeight);
    int number = 0;

    size_t i=0;
    for (; i<m_peers.size(); i++) {
        if(maxResolutionUid == m_peers[i]){
            CM_LOG_DIR(m_logdir.c_str(), INFO, "adjustVideo9Layout equal with configured user uid:%u", maxResolutionUid);
            flag = true;
            setMaxResolutionUid(number,  maxResolutionUid, regionList,9.f/5);
            number++;
            continue;
        }
        regionList[number].uid = m_peers[i];
        float yIndex = flag?static_cast<float>(number-1 % 8):static_cast<float>(number % 8);
        regionList[number].x = 8.f/9;
        regionList[number].y = (1.f/8) * yIndex;
        regionList[number].width = 1.f/9 ;
        regionList[number].height = 1.f/8;
        regionList[number].zOrder = 0;
        regionList[number].alpha = static_cast<double>(number);
        regionList[number].renderMode = 2;
        number++;
        if(i == 8 && !flag){
            changeToVideo17Layout(maxResolutionUid, regionList);
        }
    }
}

void AgoraSdk::adjustVideo17Layout(unsigned int maxResolutionUid, agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    bool flag = false;
    float canvasWidth = static_cast<float>(m_mixRes.m_width);
    float canvasHeight = static_cast<float>(m_mixRes.m_height);

    float viewWidth = 0.235f;
    float viewHEdge = 0.012f;
    float viewHeight = viewWidth * (canvasWidth/canvasHeight);
    float viewVEdge = viewHEdge * (canvasWidth/canvasHeight);
    int number = 0;
    CM_LOG_DIR(m_logdir.c_str(), INFO, "adjustVideo17Layoutenter m_peers size is:%d, maxResolutionUid:%ld",m_peers.size(), maxResolutionUid);
    for (size_t i=0; i<m_peers.size(); i++) {
        if(maxResolutionUid == m_peers[i]){
            CM_LOG_DIR(m_logdir.c_str(), INFO, "adjustVideo17Layoutenter equal with maxResolutionUid:%ld", maxResolutionUid);
            flag = true;
            setMaxResolutionUid(number,  maxResolutionUid, regionList,0.8);
            number++;
            continue;
        }
        if(!flag && i == 16) {
            CM_LOG_DIR(m_logdir.c_str(), INFO, "Not the configured uid, and small regions is sixteen, so ignore this user:%d",m_peers[i]);
            break;
        }
        regionList[number].uid = m_peers[i];
        //float xIndex = 0.833f;
        float yIndex = flag?static_cast<float>((number-1) % 8):static_cast<float>(number % 8);
        regionList[number].x = ((flag && i>8) || (!flag && i >=8)) ? (9.f/10):(8.f/10);
        regionList[number].y = (1.f/8) * yIndex;
        regionList[number].width =  1.f/10 ;
        regionList[number].height = 1.f/8;
        regionList[number].zOrder = 0;
        regionList[number].alpha = static_cast<double>(number);
        regionList[number].renderMode = 2;
        number++;
    }
}

void AgoraSdk::adjustVerticalPresentationLayout(unsigned int maxResolutionUid, agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    CM_LOG_DIR(m_logdir.c_str(), INFO, "begin adjust vertical presentation layout,peers size:%d, maxResolutionUid:%ld",m_peers.size(), maxResolutionUid);
    if(m_peers.size() <= 5) {
        adjustVideo5Layout(maxResolutionUid, regionList);
    }else if(m_peers.size() <= 7) {
        adjustVideo7Layout(maxResolutionUid, regionList);
    }else if(m_peers.size() <= 9) {
        adjustVideo9Layout(maxResolutionUid, regionList);
    }else {
        adjustVideo17Layout(maxResolutionUid, regionList);
    }
}

void AgoraSdk::adjustBestFitLayout_2(agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    float canvasWidth = static_cast<float>(m_mixRes.m_width);
    float canvasHeight = static_cast<float>(m_mixRes.m_height);
    float viewWidth = 0.235f;
    float viewHEdge = 0.012f;
    float viewHeight = viewWidth * (canvasWidth/canvasHeight);
    float viewVEdge = viewHEdge * (canvasWidth/canvasHeight);
    size_t peersCount = m_peers.size();
    for (size_t i=0; i < peersCount; i++) {
        regionList[i].uid = m_peers[i];
        regionList[i].x = ((i+1)%2)?0:0.5;
        regionList[i].y =  0.f;
        regionList[i].width = 0.5f;
        regionList[i].height = 1.f;
        regionList[i].zOrder = 0;
        regionList[i].alpha = static_cast<double>(i+1);
        regionList[i].renderMode = 2;
    }
}
void AgoraSdk::adjustBestFitLayout_Square(agora::linuxsdk::VideoMixingLayout::Region * regionList, int nSquare) {
    float canvasWidth = static_cast<float>(m_mixRes.m_width);
    float canvasHeight = static_cast<float>(m_mixRes.m_height);
    float viewWidth = static_cast<float>(1.f * 1.0/nSquare);
    float viewHEdge = static_cast<float>(1.f * 1.0/nSquare);
    float viewHeight = viewWidth * (canvasWidth/canvasHeight);
    float viewVEdge = viewHEdge * (canvasWidth/canvasHeight);
    size_t peersCount = m_peers.size();
    for (size_t i=0; i < peersCount; i++) {
        float xIndex =static_cast<float>(i%nSquare);
        float yIndex = static_cast<float>(i/nSquare);
        regionList[i].uid = m_peers[i];
        regionList[i].x = 1.f * 1.0/nSquare * xIndex;
        regionList[i].y = 1.f * 1.0/nSquare * yIndex;
        regionList[i].width = viewWidth;
        regionList[i].height = viewHEdge;
        regionList[i].zOrder = 0;
        regionList[i].alpha = static_cast<double>(i+1);
        regionList[i].renderMode = 2;
    }
}
void AgoraSdk::adjustBestFitLayout_17(agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    float canvasWidth = static_cast<float>(m_mixRes.m_width);
    float canvasHeight = static_cast<float>(m_mixRes.m_height);
    int n = 5;
    float viewWidth = static_cast<float>(1.f * 1.0/n);
    float viewHEdge = static_cast<float>(1.f * 1.0/n);
    float totalWidth = static_cast<float>(1.f - viewWidth);
    float viewHeight = viewWidth * (canvasWidth/canvasHeight);
    float viewVEdge = viewHEdge * (canvasWidth/canvasHeight);
    size_t peersCount = m_peers.size();
    for (size_t i=0; i < peersCount; i++) {
        float xIndex = static_cast<float>(i%(n-1));
        float yIndex = static_cast<float>(i/(n-1));
        regionList[i].uid = m_peers[i];
        regionList[i].width = viewWidth;
        regionList[i].height = viewHEdge;
        regionList[i].zOrder = 0;
        regionList[i].alpha = static_cast<double>(i+1);
        regionList[i].renderMode = 2;
        if(i == 16) {
            regionList[i].x = (1-viewWidth)*(1.f/2) * 1.f;
            CM_LOG_DIR(m_logdir.c_str(), INFO, "special layout for 17 x is:",regionList[i].x);
        }else {
            regionList[i].x = 0.5f * viewWidth +  viewWidth * xIndex;
        }
        regionList[i].y =  (1.0/n) * yIndex;
    }
}

void AgoraSdk::adjustBestFitVideoLayout(agora::linuxsdk::VideoMixingLayout::Region * regionList) {
    if(m_peers.size() == 1) {
        adjustBestFitLayout_Square(regionList,1);
    }else if(m_peers.size() == 2) {
        adjustBestFitLayout_2(regionList);
    }else if( 2 < m_peers.size() && m_peers.size() <=4) {
        adjustBestFitLayout_Square(regionList,2);
    }else if(5<=m_peers.size() && m_peers.size() <=9) {
        adjustBestFitLayout_Square(regionList,3);
    }else if(10<=m_peers.size() && m_peers.size() <=16) {
        adjustBestFitLayout_Square(regionList,4);
    }else if(m_peers.size() ==17) {
        adjustBestFitLayout_17(regionList);
    }else {
        CM_LOG_DIR(m_logdir.c_str(), INFO, "adjustBestFitVideoLayout is more than 17 users");
    }
}
}

