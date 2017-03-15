//
//  nm_vdc_avc.c
//  NodeMediaClient
//
//  Created by Mingliang Chen on 15/11/25.
//  Copyright © 2015年 Mingliang Chen. All rights reserved.
//

#include "nm_common.h"
#include "libavutil/imgutils.h"
#include <wels/codec_api.h>
#include <wels/codec_ver.h>

#define MAX_PARSE_SIZE 1048576
typedef struct {
    ISVCDecoder*    pSvcDecoder;
    SDecodingParam  sDecParam;
    SBufferInfo     sDstBufInfo;
    SParserBsInfo   sDstParseInfo;
} hnd_vdc_avc_t;


static int open_codec_avc(nm_av_opt* opt) {
    hnd_vdc_avc_t* h = calloc(1, sizeof(hnd_vdc_avc_t));
    opt->handle = h;
    
    memset(&h->sDecParam, 0, sizeof(SDecodingParam));
    memset(&h->sDstBufInfo, 0, sizeof(SBufferInfo));
    memset(&h->sDstParseInfo, 0, sizeof(SParserBsInfo));
    h->sDstParseInfo.pDstBuff = malloc(MAX_PARSE_SIZE);
    
    
//    h->sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_SVC;
    WelsCreateDecoder(&h->pSvcDecoder);
    (*h->pSvcDecoder)->Initialize(h->pSvcDecoder, &h->sDecParam);
    
    
    return 0;
}


static int decode_frame_avc(nm_av_opt* opt, uint8_t* inData, uint32_t inDataSize, uint8_t** outData) {
    hnd_vdc_avc_t* h = (hnd_vdc_avc_t*)opt->handle;
    uint8_t* pData[3] = {NULL, NULL, NULL};
    SBufferInfo pDstInfo;
    (*h->pSvcDecoder)->DecodeFrameNoDelay(h->pSvcDecoder,inData,(int)inDataSize, pData, &pDstInfo);
    if(pDstInfo.iBufferStatus == 1) {
        if (opt->picture_size == 0) {
            opt->width = pDstInfo.UsrData.sSystemBuffer.iWidth;
            opt->height = pDstInfo.UsrData.sSystemBuffer.iHeight;
            opt->picture_size = opt->width * opt->height * 3 / 2;
            opt->linesize[0] = pDstInfo.UsrData.sSystemBuffer.iStride[0];
            opt->linesize[1] = pDstInfo.UsrData.sSystemBuffer.iStride[1];
            opt->linesize[2] = pDstInfo.UsrData.sSystemBuffer.iStride[1];
        }
        *outData = malloc(opt->picture_size);
        av_image_copy_to_buffer(*outData, opt->picture_size, (const uint8_t * const *)pData, opt->linesize, AV_PIX_FMT_YUV420P, opt->width, opt->height, 1);
        
        
        return 0;
    } else {
        return -1;
    }
    
}

static int close_codec_avc(nm_av_opt* opt) {
    hnd_vdc_avc_t* h = (hnd_vdc_avc_t*)opt->handle;
    if(h->pSvcDecoder) {
        WelsDestroyDecoder(h->pSvcDecoder);
    }
    freep(h->sDstParseInfo.pDstBuff);
    freep(opt->extradata);
    freep(opt->handle);
    return 0;
}


const nm_av_codec_t vdc_avc = {
    .codec_id = 7,
    .open_codec = open_codec_avc,
    .decode_frame = decode_frame_avc,
    .encode_frame = NULL,
    .close_codec = close_codec_avc
};