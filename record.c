#include "record.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

void initialize_recorder(recorder_context *rc, enum AVCodecID codec_id, int64_t bit_rate, AVRational framerate, int width, int height, enum AVPixelFormat pix_fmt, const char *filename) {
    const AVOutputFormat *fmt = av_guess_format(NULL, filename, NULL);
    if (!fmt)
        fmt = av_guess_format("mpeg", NULL, NULL);

    const AVCodec *codec = avcodec_find_encoder(codec_id);
    rc->c = avcodec_alloc_context3(codec);
    rc->frame = av_frame_alloc();
    rc->packet = av_packet_alloc();

    rc->c->bit_rate = bit_rate;
    rc->c->framerate = framerate;
    rc->c->time_base = (AVRational){ framerate.den, framerate.num};
    rc->c->width = width;
    rc->c->height = height;
    rc->c->pix_fmt = pix_fmt;

    avcodec_open2(rc->c, codec, NULL);

    rc->frame->format = pix_fmt;
    rc->frame->width = width;
    rc->frame->height = height;
    rc->pts = 0;

    av_frame_get_buffer(rc->frame, 0);
    
    rc->fc = avformat_alloc_context();
    rc->fc->oformat = fmt;

    rc->st = avformat_new_stream(rc->fc, NULL);
    rc->st->time_base = rc->c->time_base;
    avcodec_parameters_from_context(rc->st->codecpar, rc->c);

    avio_open(&rc->fc->pb, filename, AVIO_FLAG_WRITE);
    int ret = avformat_write_header(rc->fc, NULL);
}

void encode_frame(recorder_context *rc, const unsigned char *image) {
    av_frame_make_writable(rc->frame);
    for (int y = 0; y < rc->frame->height; ++y) {
        for (int x = 0; x < rc->frame->width; ++x) {
            int Y = image[3 * (y * rc->frame->width + x) + 0] *  .299000 + image[3 * (y * rc->frame->width + x) + 1] *  .587000 + image[3 * (y * rc->frame->width + x) + 2] *  .114000;
            int U = image[3 * (y * rc->frame->width + x) + 0] * -.168736 + image[3 * (y * rc->frame->width + x) + 1] * -.331264 + image[3 * (y * rc->frame->width + x) + 2] *  .500000 + 128;
            int V = image[3 * (y * rc->frame->width + x) + 0] *  .500000 + image[3 * (y * rc->frame->width + x) + 1] * -.418688 + image[3 * (y * rc->frame->width + x) + 2] * -.081312 + 128;

            rc->frame->data[0][(rc->frame->height - 1 - y) * rc->frame->linesize[0] + x] = Y;
            rc->frame->data[1][(rc->frame->height - 1 - y)/2 * rc->frame->linesize[1] + x/2] = U;
            rc->frame->data[2][(rc->frame->height - 1 - y)/2 * rc->frame->linesize[2] + x/2] = V;
        }
    }
    rc->frame->pts = rc->pts++;

    avcodec_send_frame(rc->c, rc->frame);
    int ret;
    while ((ret = avcodec_receive_packet(rc->c, rc->packet)) != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        rc->packet->stream_index = rc->st->index;
        rc->packet->duration = 1;
        av_packet_rescale_ts(rc->packet, rc->c->time_base, rc->st->time_base);
        av_interleaved_write_frame(rc->fc, rc->packet);
        av_packet_unref(rc->packet);
    }
}

void finalize_recorder(recorder_context *rc) {
    avcodec_send_frame(rc->c, NULL);
    int ret;
    while ((ret = avcodec_receive_packet(rc->c, rc->packet)) != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        rc->packet->stream_index = rc->st->index;
        rc->packet->duration = 1;
        av_packet_rescale_ts(rc->packet, rc->c->time_base, rc->st->time_base);
        av_interleaved_write_frame(rc->fc, rc->packet);
        av_packet_unref(rc->packet);
    }
    
    av_write_trailer(rc->fc);
    avio_close(rc->fc->pb);
    avformat_free_context(rc->fc);
    avcodec_free_context(&rc->c);
    av_frame_free(&rc->frame);
    av_packet_free(&rc->packet);
}
