//
//  nm_vec_avc.c
//  NodeMediaClient
//
//  Created by Mingliang Chen on 15/11/29.
//  Copyright © 2015年 NodeMedia. All rights reserved.
//


#include "nm_common.h"

#include <wels/codec_api.h>
#include <wels/codec_ver.h>

typedef struct {
    ISVCEncoder* pSvcEncoder;
    SFrameBSInfo info;
    SSourcePicture pic;
    int       tmp_size;
    NMPicture tmp_picture;
    NMPicture dst_picture;
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;
    bool     sendSPSPPS;
} hnd_vec_avc_t;


static int open_codec_avc(nm_av_opt* opt) {
    hnd_vec_avc_t* h = calloc(1, sizeof(hnd_vec_avc_t));
    opt->handle = h;
    
    WelsCreateSVCEncoder(&h->pSvcEncoder);
    SEncParamExt param;
    memset (&param, 0, sizeof (SEncParamBase));
    (*h->pSvcEncoder)->GetDefaultParams(h->pSvcEncoder, &param);
    
    param.iUsageType                 = CAMERA_VIDEO_REAL_TIME;
    param.fMaxFrameRate              = opt->fps;
    param.iPicWidth                  = opt->width;
    param.iPicHeight                 = opt->height;
    param.iTargetBitrate             = opt->video_bitrate;
    param.iMaxBitrate                = opt->video_bitrate;
    param.iRCMode                    = RC_BITRATE_MODE;
    param.iTemporalLayerNum          = 1;
    param.iSpatialLayerNum           = 1;
    param.bEnableDenoise             = 0;
    param.bEnableBackgroundDetection = 1;
    param.bEnableAdaptiveQuant       = 1;
    param.bEnableFrameSkip           = 0;
    param.bEnableLongTermReference   = 0;
    param.iLtrMarkPeriod             = 30;
    param.uiIntraPeriod              = opt->fps;
    param.eSpsPpsIdStrategy          = CONSTANT_ID;
    param.bPrefixNalAddingCtrl       = 0;
    if(opt->video_profile == 0) {
        param.iEntropyCodingModeFlag     = 0;
    }else {
        param.iEntropyCodingModeFlag     = 1;
    }
    
    param.sSpatialLayers[0].iVideoWidth         = param.iPicWidth;
    param.sSpatialLayers[0].iVideoHeight        = param.iPicHeight;
    param.sSpatialLayers[0].fFrameRate          = param.fMaxFrameRate;
    param.sSpatialLayers[0].iSpatialBitrate     = param.iTargetBitrate;
    param.sSpatialLayers[0].iMaxSpatialBitrate  = param.iMaxBitrate;
    
    int ret = (*h->pSvcEncoder)->InitializeExt(h->pSvcEncoder,&param);
    if(ret != 0) {
        printf("AVC encoder init error, invalid param\n");
        return -1;
    }
    int videoFormat = videoFormatI420;
    (*h->pSvcEncoder)->SetOption(h->pSvcEncoder,ENCODER_OPTION_DATAFORMAT, &videoFormat);
    
    SFrameBSInfo fbi = { 0 };
    int i, size = 0;
    (*h->pSvcEncoder)->EncodeParameterSets(h->pSvcEncoder, &fbi);
    for (i = 0; i < fbi.sLayerInfo[0].iNalCount; i++) {
        size += fbi.sLayerInfo[0].pNalLengthInByte[i];
    }
    opt->extradata = malloc(size);
    opt->extradata_size = size;
    memcpy(opt->extradata, fbi.sLayerInfo[0].pBsBuf, size);
    
    memset(&h->info, 0, sizeof(SFrameBSInfo));
    memset(&h->pic, 0, sizeof(SSourcePicture));
    h->pic.iPicWidth = opt->width;
    h->pic.iPicHeight = opt->height;
    h->pic.iColorFormat = videoFormatI420;
    
    h->tmp_size = nm_picture_get_size(AV_PIX_FMT_YUV420P, opt->tmp_width, opt->tmp_height);
    nm_picture_alloc(&h->tmp_picture, AV_PIX_FMT_YUV420P, opt->tmp_width, opt->tmp_height);
    nm_picture_alloc(&h->dst_picture, AV_PIX_FMT_YUV420P, opt->width, opt->height);
    h->crop_width = opt->src_width;
    h->crop_height = opt->src_height;
    if(opt->src_width == 640) {
        h->crop_y = 60;
        h->crop_height = 360;
    }
    
    
    
    //printf("AVC encoder init success.\n");
    return 0;
}


static int encode_frame_avc(nm_av_opt* opt, uint8_t* inData, uint8_t** outData, uint32_t* outDataSize) {
    hnd_vec_avc_t* h = (hnd_vec_avc_t*)opt->handle;
    
    int ret = ConvertToI420((const uint8*) inData, h->tmp_size,
                            h->tmp_picture.data[0], h->tmp_picture.linesize[0],
                            h->tmp_picture.data[1], h->tmp_picture.linesize[1],
                            h->tmp_picture.data[2], h->tmp_picture.linesize[2],
                            h->crop_x, h->crop_y,
                            opt->src_width, opt->src_height,
                            h->crop_width, h->crop_height,
                            opt->videoRotation, FOURCC_NV12);
    if (ret != 0) {
        printf( "yuv convert error.\n");
        return -1;
    }
    ret = I420Scale(h->tmp_picture.data[0], h->tmp_picture.linesize[0],
                    h->tmp_picture.data[1], h->tmp_picture.linesize[1],
                    h->tmp_picture.data[2], h->tmp_picture.linesize[2],
                    opt->tmp_width,opt->tmp_height,
                    h->dst_picture.data[0], h->dst_picture.linesize[0],
                    h->dst_picture.data[1], h->dst_picture.linesize[1],
                    h->dst_picture.data[2], h->dst_picture.linesize[2],
                    opt->width, opt->height, 0);
    
    if (ret != 0) {
        printf("yuv scale error.\n");
        return -1;;
    }
    h->pic.pData[0] = h->dst_picture.data[0];
    h->pic.pData[1] = h->dst_picture.data[1];
    h->pic.pData[2] = h->dst_picture.data[2];
    
    h->pic.iStride[0] = h->dst_picture.linesize[0];
    h->pic.iStride[1] = h->dst_picture.linesize[1];
    h->pic.iStride[2] = h->dst_picture.linesize[2];
    
    SFrameBSInfo info = {0};
    (*h->pSvcEncoder)->EncodeFrame(h->pSvcEncoder, &h->pic, & info);
    if(info.eFrameType != videoFrameTypeSkip ) {
        int first_layer = 0;
        int size = 0;
//        printf("h->sendSPSPPS :%d\n",h->sendSPSPPS);
        first_layer = info.iLayerNum -1;   
        for(int i=first_layer;i<info.iLayerNum;i++) {
            for (int j = 0; j < info.sLayerInfo[i].iNalCount; j++) {
                size += info.sLayerInfo[i].pNalLengthInByte[j];
            }
        }
        *outData = malloc(size);
        *outDataSize = size;
        int pos = 0;
        for(int i=first_layer;i<info.iLayerNum;i++) {
            int nsize = 0;
            for (int j = 0; j < info.sLayerInfo[i].iNalCount; j++) {
                nsize += info.sLayerInfo[i].pNalLengthInByte[j];
            }
            memcpy((*outData)+pos, info.sLayerInfo[i].pBsBuf, nsize);
            pos += nsize;
        }
    }
    return 0;
}

static int close_codec_avc(nm_av_opt* opt) {
    hnd_vec_avc_t* h = (hnd_vec_avc_t*)opt->handle;
    nm_picture_free(&h->tmp_picture);
    nm_picture_free(&h->dst_picture);
    WelsDestroySVCEncoder(h->pSvcEncoder);
    freep(opt->handle);
    freep(opt->extradata);
    return 0;
}


const nm_av_codec_t vec_avc = {
    .codec_id = 7,
    .open_codec = open_codec_avc,
    .decode_frame = NULL,
    .encode_frame = encode_frame_avc,
    .close_codec = close_codec_avc
};