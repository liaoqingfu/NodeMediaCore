//
// Created by Mingliang Chen on 15/11/25.
//

#include "nm_common.h"

Buffer *buffer_alloc(uint8_t *data, uint32_t data_size, uint32_t time_stamp) {
    Buffer *buffer = (Buffer *) calloc(1, sizeof(Buffer));
    buffer->data = (uint8_t *) malloc(data_size);
    memcpy(buffer->data, data, data_size);
    buffer->data_size = data_size;
    buffer->time_stamp = time_stamp;
    return buffer;
    
}

void buffer_free(Buffer* buffer) {
    if (buffer) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->next = NULL;
        free(buffer);
        buffer = NULL;
    }
}

void buffer_queue_init(BufferQueue *q) {
    memset(q, 0, sizeof(BufferQueue));
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

int buffer_queue_put(BufferQueue *q, Buffer *buffer) {
    int ret;
    pthread_mutex_lock(&q->mutex);
    do {
        if (q->abort_request) {
            ret = -1;
            break;
        }
        if (!q->last_buffer) {
            q->first_buffer = buffer;
        } else {
            q->last_buffer->next = buffer;
        }
        q->last_buffer = buffer;
        q->nb_packets++;
        
        if (q->nb_packets > 1) {
            q->buffer_length = q->last_buffer->time_stamp - q->first_buffer->time_stamp;
        } else {
            q->buffer_length = 0;
        }
        pthread_cond_signal(&q->cond);
    } while (0);
    pthread_mutex_unlock(&q->mutex);
    
    return ret;
}

int buffer_queue_get(BufferQueue *q, Buffer** buffer, bool block) {
    int ret;
    pthread_mutex_lock(&q->mutex);
    while (1) {
        if (q->abort_request) {
            ret = -1;
            break;
        }
        *buffer = q->first_buffer;
        if (*buffer) {
            q->first_buffer = (*buffer)->next;
            if (!q->first_buffer) {
                q->last_buffer = NULL;
            }
            q->nb_packets--;
            if (q->nb_packets > 1) {
                q->buffer_length = q->last_buffer->time_stamp - q->first_buffer->time_stamp;
            } else {
                q->buffer_length = 0;
            }
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            pthread_cond_wait(&q->cond, &q->mutex);
        }
    }
    pthread_mutex_unlock(&q->mutex);
    return ret;
}

void buffer_queue_flush(BufferQueue *q) {
    Buffer *buffer, *buffer1;
    pthread_mutex_lock(&q->mutex);
    for (buffer = q->first_buffer; buffer; buffer = buffer1) {
        buffer1 = buffer->next;
        buffer_free(buffer);
    }
    q->last_buffer = NULL;
    q->first_buffer = NULL;
    q->nb_packets = 0;
    q->buffer_length = 0;
    pthread_mutex_unlock(&q->mutex);
}

void buffer_queue_abort(BufferQueue *q) {
    pthread_mutex_lock(&q->mutex);
    q->abort_request = 1;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

void buffer_queue_destroy(BufferQueue *q) {
    buffer_queue_flush(q);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}


/////////////////////////////////////////////
// bitreader
//
////////////////////////////////////////////
static void fill_reservoir(BitReader *reader)
{
    uint8_t i;
    
    reader->reservoir = 0;
    for(i=0; reader->size > 0 && i < 4; ++i){
        reader->reservoir = (reader->reservoir << 8) | *(reader->data);
        reader->data += 1;
        reader->size -= 1;
    }
    
    reader->leftbits = i * 8;
    reader->reservoir <<= 32 - reader->leftbits;
}



int8_t bitreader_init(BitReader *reader, const uint8_t *data, uint32_t size)
{
    if(!reader || !data || !size)
        return -1;
    
    reader->data = data;
    reader->size = size;
    reader->reservoir = 0;
    reader->leftbits = 0;
    return 0;
}


uint32_t get_bits(BitReader *reader, uint8_t n)
{
    uint32_t result = 0;
    uint8_t m = 0;
    
    while(n > 0){
        if(reader->leftbits == 0)
            fill_reservoir(reader);
        
        m = n;
        if(m > reader->leftbits)
            m = reader->leftbits;
        
        result = (result << m) | (reader->reservoir >> (32 - m));
        reader->reservoir <<= m;
        reader->leftbits -= m;
        n -= m;
    }
    
    return result;
}


void skip_bits(BitReader *reader, uint32_t n)
{
    while(n > 32){
        get_bits(reader, 32);
        n -= 32;
    }
    
    if(n > 0)
        get_bits(reader, n);
}


uint32_t bitreader_size(BitReader *reader)
{
    return reader->size * 8 + reader->leftbits;
}


const uint8_t* bitreader_data(BitReader *reader)
{
    return reader->data - (reader->leftbits / 8);
}

int AVCDecoderConfigurationRecord(uint8_t* srcData, uint32_t srcDataSize, uint8_t** sps, uint32_t* spsLen, uint8_t** pps, uint32_t *ppsLen) {
    BitReader reader = {0};
    bitreader_init(&reader, srcData, srcDataSize);
    uint32_t configurationVersion           = get_bits(&reader, 8);
    uint32_t AVCProfileIndication           = get_bits(&reader, 8);
    uint32_t profile_compatibility          = get_bits(&reader, 8);
    uint32_t AVCLevelIndication             = get_bits(&reader, 8);
    uint32_t reserved0                      = get_bits(&reader, 6);
    uint32_t lengthSizeMinusOne             = get_bits(&reader, 2);
    uint32_t reserved1                      = get_bits(&reader, 3);
    
    //SPS
    uint32_t numOfSequenceParameterSets     = get_bits(&reader, 5);
    if(numOfSequenceParameterSets > 1) {
        return -1;
    }
    uint32_t sequenceParameterSetLength     = get_bits(&reader, 16);
    *sps = malloc(sequenceParameterSetLength);
    for(int i = 0;i < sequenceParameterSetLength; i++) {
        (*sps)[i] = get_bits(&reader, 8) & 0xFF;
    }
    *spsLen =sequenceParameterSetLength;
    
    //PPS
    uint32_t numOfPictureParameterSets      = get_bits(&reader, 8);
    if(numOfPictureParameterSets > 1) {
        freep(*sps);
        return -1;
    }
    uint32_t pictureParameterSetLength      = get_bits(&reader, 16);
    *pps = malloc(pictureParameterSetLength);
    for(int i = 0;i < pictureParameterSetLength; i++) {
        (*pps)[i] = get_bits(&reader, 8) & 0xFF;
    }
    *ppsLen = pictureParameterSetLength;
    return 0;
}

void dumpBuffer(uint8_t* buffer, uint32_t bufferLength) {
    for (int i=0; i<bufferLength; i++) {
        printf("%02x ",buffer[i] & 0xff);
    }
    printf("\n");
}

int64_t gettime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec/1000;
}


int nm_picture_fill(NMPicture *picture, const uint8_t *ptr, enum AVPixelFormat pix_fmt, int width, int height)
{
    return av_image_fill_arrays(picture->data, picture->linesize, ptr, pix_fmt, width, height, 1);
}

int nm_picture_layout(const NMPicture* src, enum AVPixelFormat pix_fmt, int width, int height, unsigned char *dest, int dest_size)
{
    return av_image_copy_to_buffer(dest, dest_size, (const uint8_t * const*)src->data, src->linesize, pix_fmt, width, height, 1);
}

int nm_picture_get_size(enum AVPixelFormat pix_fmt, int width, int height)
{
    return av_image_get_buffer_size(pix_fmt, width, height, 1);
}

int nm_picture_alloc(NMPicture *picture, enum AVPixelFormat pix_fmt, int width, int height)
{
    int ret = av_image_alloc(picture->data, picture->linesize, width, height, pix_fmt, 1);
    if (ret < 0) {
        memset(picture, 0, sizeof(NMPicture));
        return ret;
    }
    return 0;
}

void nm_picture_free(NMPicture *picture)
{
    av_freep(&picture->data[0]);
}

void nm_picture_copy(NMPicture *dst, const NMPicture *src, enum AVPixelFormat pix_fmt, int width, int height)
{
    av_image_copy(dst->data, dst->linesize, (const uint8_t **)src->data, src->linesize, pix_fmt, width, height);
}


//bitstream filter
#define AV_INPUT_BUFFER_PADDING_SIZE 8
#define AV_WB32(p, darg)                \
do {                                    \
unsigned d = (darg);                    \
((uint8_t*)(p))[3] = (d);               \
((uint8_t*)(p))[2] = (d)>>8;            \
((uint8_t*)(p))[1] = (d)>>16;           \
((uint8_t*)(p))[0] = (d)>>24;           \
} while(0)

static int alloc_and_copy(uint8_t **poutbuf, int *poutbuf_size,
                          const uint8_t *sps_pps, uint32_t sps_pps_size,
                          const uint8_t *in, uint32_t in_size)
{
    uint32_t offset         = *poutbuf_size;
    uint8_t nal_header_size = offset ? 3 : 4;
    int err;
    
    *poutbuf_size += sps_pps_size + in_size + nal_header_size;
    if ((err = av_reallocp(poutbuf,*poutbuf_size + AV_INPUT_BUFFER_PADDING_SIZE)) < 0) {
        *poutbuf_size = 0;
        return err;
    }
    if (sps_pps)
        memcpy(*poutbuf + offset, sps_pps, sps_pps_size);
    memcpy(*poutbuf + sps_pps_size + nal_header_size + offset, in, in_size);
    if (!offset) {
        AV_WB32(*poutbuf + sps_pps_size, 1);
    } else {
        (*poutbuf + offset + sps_pps_size)[0] =
        (*poutbuf + offset + sps_pps_size)[1] = 0;
        (*poutbuf + offset + sps_pps_size)[2] = 1;
    }
    
    return 0;
}


int nm_h264_mp4toannexb_filter(H264BSFContext* ctx, uint8_t **poutbuf, int *poutbuf_size,
                            const uint8_t *buf, int buf_size) {
    int i;
    uint8_t unit_type;
    int32_t nal_size;
    uint32_t cumul_size    = 0;
    const uint8_t *buf_end = buf + buf_size;
    int ret = 0;
    
    ctx->length_size      = 4;
    ctx->new_idr          = 1;
    ctx->idr_sps_seen     = 0;
    ctx->idr_pps_seen     = 0;
    ctx->extradata_parsed = 1;
    
    *poutbuf_size = 0;
    *poutbuf      = NULL;
    do {
        ret= AVERROR(EINVAL);
        if (buf + ctx->length_size > buf_end)
            goto fail;
        
        for (nal_size = 0, i = 0; i<ctx->length_size; i++)
            nal_size = (nal_size << 8) | buf[i];
        
        buf      += ctx->length_size;
        unit_type = *buf & 0x1f;
        
        if (nal_size > buf_end - buf || nal_size < 0)
            goto fail;
        
        if (unit_type == 7)
            ctx->idr_sps_seen = ctx->new_idr = 1;
        else if (unit_type == 8) {
            ctx->idr_pps_seen = ctx->new_idr = 1;
            /* if SPS has not been seen yet, prepend the AVCC one to PPS */
            if (!ctx->idr_sps_seen) {
                if (ctx->sps_offset == -1)
                    printf("SPS not present in the stream, nor in AVCC, stream may be unreadable\n");
                else {
                    if ((ret = alloc_and_copy(poutbuf, poutbuf_size,
                                              ctx->spspps_buf + ctx->sps_offset,
                                              ctx->pps_offset != -1 ? ctx->pps_offset : ctx->spspps_size - ctx->sps_offset,
                                              buf, nal_size)) < 0)
                        goto fail;
                    ctx->idr_sps_seen = 1;
                    goto next_nal;
                }
            }
        }
        
        /* if this is a new IDR picture following an IDR picture, reset the idr flag.
         * Just check first_mb_in_slice to be 0 as this is the simplest solution.
         * This could be checking idr_pic_id instead, but would complexify the parsing. */
        if (!ctx->new_idr && unit_type == 5 && (buf[1] & 0x80))
            ctx->new_idr = 1;
        
        /* prepend only to the first type 5 NAL unit of an IDR picture, if no sps/pps are already present */
        if (ctx->new_idr && unit_type == 5 && !ctx->idr_sps_seen && !ctx->idr_pps_seen) {
            if ((ret=alloc_and_copy(poutbuf, poutbuf_size,
                                    ctx->spspps_buf, ctx->spspps_size,
                                    buf, nal_size)) < 0)
                goto fail;
            ctx->new_idr = 0;
            /* if only SPS has been seen, also insert PPS */
        } else if (ctx->new_idr && unit_type == 5 && ctx->idr_sps_seen && !ctx->idr_pps_seen) {
            if (ctx->pps_offset == -1) {
                printf("PPS not present in the stream, nor in AVCC, stream may be unreadable\n");
                if ((ret = alloc_and_copy(poutbuf, poutbuf_size,
                                          NULL, 0, buf, nal_size)) < 0)
                    goto fail;
            } else if ((ret = alloc_and_copy(poutbuf, poutbuf_size,
                                             ctx->spspps_buf + ctx->pps_offset, ctx->spspps_size - ctx->pps_offset,
                                             buf, nal_size)) < 0)
                goto fail;
        } else {
            if ((ret=alloc_and_copy(poutbuf, poutbuf_size,
                                    NULL, 0, buf, nal_size)) < 0)
                goto fail;
            if (!ctx->new_idr && unit_type == 1) {
                ctx->new_idr = 1;
                ctx->idr_sps_seen = 0;
                ctx->idr_pps_seen = 0;
            }
        }
        
    next_nal:
        buf        += nal_size;
        cumul_size += nal_size + ctx->length_size;
    } while (cumul_size < buf_size);
    
    return 1;
    
fail:
    av_freep(poutbuf);
    *poutbuf_size = 0;
    return ret;
}

