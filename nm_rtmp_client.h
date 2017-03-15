//
// Created by Mingliang Chen on 15/11/25.
//

#ifndef NM_NODEMEDIACLIENT_RTMP_CLIENT_H
#define NM_NODEMEDIACLIENT_RTMP_CLIENT_H

#include <srs_librtmp.hpp>
#include <stdlib.h>
#include "nm_common.h"

enum STREAM_STATUS{
    STREAM_CLOSE = 0,
    STREAM_CONNECTION,
    STREAM_RECONNECTION,
    STREAM_CONNECTED,
    STREAM_BUFFER_EMPTY,
    STREAM_BUFFER_BUFFERING,
    STREAM_BUFFER_FULL
};

typedef struct{
    //SRS Context
    srs_rtmp_t          rtmp;
    
    //Local callback object
    OnEventCallback     eventCb;
    OnAudioInfoCallback audioInfoCb;
    OnVideoInfoCallback videoInfoCb;
    OnAudioDataCallback audioDataCb;
    OnVideoDataCallback videoDataCb;
    OnAudioOverCallback audioOverCb;
    OnVideoOverCallback videoOverCb;
    OnTsCallback        tsCb;
    OnLogCallback       logCb;
    
    //thread
    pthread_t           openThreadId;
    
    int                 streamStatus;
    bool                abort;
    
    BufferQueue audioInQueue;
    BufferQueue videoInQueue;
    
    BufferQueue audioOuQueue;
    BufferQueue videoOuQueue;
    
    BufferQueue mediaOuQueue;
    
    //本地缓冲时长 毫秒单位
    int bufferTime;
    
    int play_mode;
    
    int64_t sys_clk_sta;
    int64_t pts_clk_sta;
    
    bool                processAudio;
    bool                processVideo;
    bool                haveAudio;
    bool                haveVideo;

    nm_av_codec_t       audio_encoder;
    nm_av_codec_t       video_encoder;
    
    nm_av_opt           audio_opt;
    nm_av_opt           video_opt;
    
    SpeexPreprocessState *preprocess_state;
}nm_rtmp_client_t,NMRtmpClient;

nm_rtmp_client_t* nm_rtmp_client_create(const char* rtmpUrl, const char* pageUrl, const char* swfUrl);
void nm_rtmp_client_destroy(nm_rtmp_client_t* nrc);

int nm_rtmp_client_start_play(nm_rtmp_client_t* nrc);
int nm_rtmp_client_stop_play(nm_rtmp_client_t* nrc);

int nm_rtmp_client_start_publish(nm_rtmp_client_t* nrc);
int nm_rtmp_client_stop_publish(nm_rtmp_client_t* nrc);

int nm_rtmp_client_put_audio(nm_rtmp_client_t* nrc, uint8_t* data, size_t size);
int nm_rtmp_client_put_video(nm_rtmp_client_t* nrc, uint8_t* data, size_t size);


#endif //NM_NODEMEDIACLIENT_RTMP_CLIENT_H
