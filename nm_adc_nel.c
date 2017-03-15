//
//  nm_adc_nel.c
//  NodeMediaClient
//
//  Created by Mingliang Chen on 16/1/6.
//  Copyright © 2016年 NodeMedia. All rights reserved.
//

#include "nm_av_codec.h"
#include "nelly.h"

static int open_codec_nel(nm_av_opt* opt) {
    nelly_handle* s = (nelly_handle*)calloc(1, sizeof(nelly_handle));
    opt->handle = s;
    return 0;
}


static int decode_frame_nel(nm_av_opt* opt, uint8_t* inData, uint32_t inDataSize, uint8_t** outData) {
    nelly_handle* s = (nelly_handle*)opt->handle;
    if(NELLY_BLOCK_LEN <= inDataSize) {
        float audio[256];
        nelly_decode_block(s,inData,audio);
        *outData = malloc(512);
        nelly_util_floats2shorts(audio,(short*) *outData);
        return NELLY_BLOCK_LEN;
    } else {
        return -1;
    }
}

static int close_codec_nel(nm_av_opt* opt) {
    nelly_handle* s = (nelly_handle*)opt->handle;
    free(s);
    return 0;
}


const nm_av_codec_t adc_nel = {
    .codec_id   = 6,
    .open_codec = open_codec_nel,
    .decode_frame = decode_frame_nel,
    .encode_frame = NULL,
    .close_codec = close_codec_nel
};