#ifndef RECORD_H
#define RECORD_H

#include <stdlib.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

typedef struct {
    AVFormatContext *fc;

    AVCodecContext *c;
    AVFrame *frame;
    unsigned int pts;
    AVPacket *packet;
    AVStream *st;
} recorder_context;

void initialize_recorder(recorder_context *rc, enum AVCodecID codec_id, int64_t bit_rate, AVRational framerate, int width, int height, enum AVPixelFormat pix_fmt, const char *filename);

void encode_frame(recorder_context *rc, const unsigned char *image);

void finalize_recorder(recorder_context *rc);

#endif /* RECORD_H */
