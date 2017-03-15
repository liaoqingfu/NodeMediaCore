//
// Created by Mingliang Chen on 15/11/25.
//

#ifndef NM_NODEMEDIACLIENT_COMMON_H
#define NM_NODEMEDIACLIENT_COMMON_H

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdarg.h>

#include "libavutil/avutil.h"
#include "libavutil/base64.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libavutil/imgutils.h"
#include "libavutil/audio_fifo.h"
#include "libyuv.h"
#include "speex/speex_preprocess.h"
#include "nm_av_codec.h"

#define freep(X) if(X){free(X);X=NULL;}

typedef struct NMPicture {
    uint8_t *data[4];    ///< pointers to the image data planes
    int linesize[4];     ///< number of bytes per line
} NMPicture;

typedef void (*OnEventCallback)(int env,const char* msg);

typedef void (*OnVideoInfoCallback)(int width, int height,int buffer_size);

typedef void (*OnAudioInfoCallback)(int sample_rate, int channels, int buffer_size);

typedef void (*OnVideoDataCallback)(uint8_t *data, int size);

typedef void (*OnAudioDataCallback)(uint8_t *data, int size);

typedef void (*OnVideoOverCallback)();

typedef void (*OnAudioOverCallback)();

typedef void (*OnTsCallback)(uint8_t *data, int size);

typedef void (*OnLogCallback)(int level, const char *fmt, ...);


typedef struct Buffer {
    uint8_t *data;
    int type;
    uint32_t data_size;
    uint32_t time_stamp;
    struct Buffer *prev;
    struct Buffer *next;
} Buffer;


typedef struct BufferQueue {
    Buffer *first_buffer, *last_buffer;
    int nb_packets;
    int max_buffer_length;
    int buffer_length;
    int abort_request;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} BufferQueue;

Buffer *buffer_alloc(uint8_t* data, uint32_t data_size, uint32_t time_stamp);

void buffer_free(Buffer* buffer);

void buffer_queue_init(BufferQueue* q);

int buffer_queue_put(BufferQueue* q, Buffer* buffer);

int buffer_queue_get(BufferQueue* q, Buffer** buffer, bool block);

void buffer_queue_flush(BufferQueue* q);

void buffer_queue_abort(BufferQueue* q);

void buffer_queue_destroy(BufferQueue* q);



int AVCDecoderConfigurationRecord(uint8_t* srcData, uint32_t srcDataSize,  uint8_t** sps, uint32_t* spsLen, uint8_t** pps, uint32_t *ppsLen);

typedef struct BitReader{
    const uint8_t *data;
    uint32_t size;
    uint32_t reservoir;
    uint32_t leftbits;
}BitReader;


int8_t bitreader_init(BitReader *reader, const uint8_t *data, uint32_t size);

/*
 this function return range n value of bits
 uint8_t n: this parameter range is 0 <= n <= 32.
 */
uint32_t get_bits(BitReader *reader, uint8_t n);

/*
 this function skip bits from current position.
 */
void skip_bits(BitReader *reader, uint32_t n);

/*
 this function return num of bits that BitReader contains
 */
uint32_t bitreader_size(BitReader *reader);

/*
 this function return current position pointer of data (carefully: pointer is per bytes)
 */
const uint8_t* bitreader_data(BitReader *reader);

void dumpBuffer(uint8_t* buffer, uint32_t bufferLength);

int64_t gettime();

int nm_picture_fill(NMPicture *picture, const uint8_t *ptr, enum AVPixelFormat pix_fmt, int width, int height);

int nm_picture_layout(const NMPicture* src, enum AVPixelFormat pix_fmt, int width, int height, unsigned char *dest, int dest_size);

int nm_picture_get_size(enum AVPixelFormat pix_fmt, int width, int height);

int nm_picture_alloc(NMPicture *picture, enum AVPixelFormat pix_fmt, int width, int height);

void nm_picture_free(NMPicture *picture);

void nm_picture_copy(NMPicture *dst, const NMPicture *src, enum AVPixelFormat pix_fmt, int width, int height);



//bitstream filter
typedef struct H264BSFContext {
    int32_t  sps_offset;
    int32_t  pps_offset;
    uint8_t  length_size;
    uint8_t  new_idr;
    uint8_t  idr_sps_seen;
    uint8_t  idr_pps_seen;
    int      extradata_parsed;
    
    /* When private_spspps is zero then spspps_buf points to global extradata
     and bsf does replace a global extradata to own-allocated version (default
     behaviour).
     When private_spspps is non-zero the bsf uses a private version of spspps buf.
     This mode necessary when bsf uses in decoder, else bsf has issues after
     decoder re-initialization. Use the "private_spspps_buf" argument to
     activate this mode.
     */
    int      private_spspps;
    uint8_t *spspps_buf;
    uint32_t spspps_size;
} H264BSFContext;

int nm_h264_mp4toannexb_filter(H264BSFContext* ctx, uint8_t **poutbuf, int *poutbuf_size,const uint8_t *buf, int buf_size);

#endif //NM_NODEMEDIACLIENT_COMMON_H
