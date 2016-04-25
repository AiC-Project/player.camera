#include "camera-common.h"

#include "logger.h"
#include "camera-capture-ffmpeg.h"

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <pthread.h>
#include <unistd.h>

#define LOG_TAG "camera-capture-ffmpeg"

typedef struct video_dec
{
    AVFormatContext* fmt_ctx;
    AVCodecContext* video_dec_ctx;
    AVStream* video_stream;
    uint8_t* video_dst_data[4];
    int video_dst_linesize[4];
    int video_dst_bufsize;
    int video_stream_idx;
    AVFrame* frame;
    AVPacket pkt;
    int width;
    int height;
} video_dec_t;

typedef struct
{
    int pixel_format;
    int width;
    int height;
    struct SwsContext* resize;
} resize_ctx;

/*******************************************************************************
 *                     CameraDevice API
 ******************************************************************************/

extern pthread_mutex_t dec_mtx;

static char camera_filename[256] = "zebre.mpg";
static const char default_filename[] = "zebre.mpg";

static int open_codec_context(int* stream_idx, AVFormatContext* fmt_ctx, enum AVMediaType type)
{
    int ret;
    AVStream* st;
    AVCodecContext* dec_ctx = NULL;
    AVCodec* dec = NULL;
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0)
    {
        return ret;
    }
    else
    {
        *stream_idx = ret;
        st = fmt_ctx->streams[*stream_idx];
        /* find decoder for the stream */
        dec_ctx = st->codec;
        dec = avcodec_find_decoder(dec_ctx->codec_id);
        if (!dec)
        {
            return ret;
        }
        if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0)
        {
            return ret;
        }
    }
    return 0;
}
static int next_frame(video_dec_t* ctx)
{
    AVPacket pkt = ctx->pkt;
    int got_frame = 0;
    int ret = 0;
    if (av_read_frame(ctx->fmt_ctx, &(ctx->pkt)) < 0)
    {
        W("read frame fail");
        return -1;
    }
    if (pkt.stream_index == ctx->video_stream_idx)
    {
        /* decode video frame */
        ret = avcodec_decode_video2(ctx->video_dec_ctx, ctx->frame, &got_frame, &(ctx->pkt));
        if (ret < 0)
        {
            W("decode video fail");
            return 0;
        }
        ctx->pkt.data += ret;
        ctx->pkt.size -= ret;
    }
    av_packet_unref(&pkt);

    return got_frame;
}

static int start_video_dec(video_dec_t* ctx, const char* const filename)
{
    puts("start video dec");
    /* register all formats and codecs */
    av_register_all();

    /* open input file, and allocate format context */
    if (avformat_open_input(&(ctx->fmt_ctx), filename, NULL, NULL) < 0)
    {
        C("Could not open source file %s", filename);
        return -1;
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(ctx->fmt_ctx, NULL) < 0)
    {
        C("Could not find stream information");
        return -1;
    }

    if (open_codec_context(&(ctx->video_stream_idx), ctx->fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0)
    {
        ctx->video_stream = ctx->fmt_ctx->streams[ctx->video_stream_idx];
        ctx->video_dec_ctx = ctx->video_stream->codec;
    }

    /* dump input information to stderr */
    av_dump_format(ctx->fmt_ctx, 0, filename, 0);

    ctx->frame = av_frame_alloc();

    if (!ctx->frame)
    {
        C("Could not allocate frame");
        return -1;
    }

    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&(ctx->pkt));
    ctx->pkt.data = NULL;
    ctx->pkt.size = 0;

    return 0;
}

static void stop_video_dec(video_dec_t* ctx)
{
    I("Stopping video decoding");
    if (ctx)
    {
        if (ctx->video_dec_ctx)
            avcodec_close(ctx->video_dec_ctx);

        avformat_close_input(&(ctx->fmt_ctx));
        av_free(ctx->fmt_ctx);

        av_free(ctx->frame);
    }
}

/**
 * Resize video frames to the right size and pixel format
 * Cache the resize context until another one is needed.
 */
static void resize(video_dec_t* dec, int pixel_format, void* framebuffer)
{
    static struct SwsContext* resize = NULL;
    resize =
        sws_getCachedContext(resize, dec->frame->width, dec->frame->height, dec->frame->format,
                             dec->width, dec->height, pixel_format, SWS_BICUBIC, NULL, NULL, NULL);

    AVFrame* frame2 = av_frame_alloc();
    int num_bytes = avpicture_get_size(pixel_format, dec->width, dec->height);
    uint8_t* frame2_buffer = (uint8_t*) av_malloc(num_bytes * sizeof(uint8_t));
    avpicture_fill((AVPicture*) frame2, frame2_buffer, pixel_format, dec->width, dec->height);
    frame2->width = dec->width;
    frame2->height = dec->height;
    sws_scale(resize, (const uint8_t* const*) dec->frame->data, dec->frame->linesize, 0,
              dec->frame->height, frame2->data, frame2->linesize);

    memcpy(framebuffer, frame2->data[0], num_bytes);
    free(frame2_buffer);
    av_free(frame2);
}

CameraDevice* camera_device_open(const char* name, int inp_channel)
{
    I("Opening device");
    CameraDevice* cam = (CameraDevice*) malloc(sizeof(CameraDevice));
    video_dec_t* decoding_context = (video_dec_t*) calloc(1, sizeof(video_dec_t));
    cam->opaque = (void*) decoding_context;
    start_video_dec((video_dec_t*) cam->opaque, camera_filename);
    return cam;
}

int camera_device_start_capturing(CameraDevice* ccd, uint32_t pixel_format, int frame_width,
                                  int frame_height)
{
    I("Start capturing");
    video_dec_t* decoding_context = (video_dec_t*) ccd->opaque;
    decoding_context->width = frame_width;
    decoding_context->height = frame_height;
    return 0;
}

int camera_device_stop_capturing(CameraDevice* ccd)
{
    I("Stop capturing");
    return 0;
}

int camera_device_read_frame(CameraDevice* ccd, ClientFrameBuffer* framebuffers, int fbs_num,
                             float r_scale, float g_scale, float b_scale, float exp_comp)
{
    video_dec_t* dec = (video_dec_t*) ccd->opaque;
    int res = next_frame(dec);
    if (res <= 0)
    {
        stop_video_dec(dec);
        start_video_dec(dec, camera_filename);
        while (res <= 0)
            res = next_frame(dec);
    }
    if (res > 0)
    {
        int init_fb = 0;
        do
        {
            resize(dec, (framebuffers + init_fb)->pixel_format,
                   (framebuffers + init_fb)->framebuffer);
        } while (init_fb + 1 < fbs_num);
    }
    return !res;
}

void camera_device_close(CameraDevice* ccd)
{
    I("Closing device");
    stop_video_dec((video_dec_t*) ccd->opaque);
    free(ccd->opaque);
    ccd->opaque = NULL;
    return;
}

int enumerate_camera_devices(CameraInfo* cis, int max)
{
    cis[0].display_name = "toto";
    cis[0].device_name = "toto";
    char* direction = malloc(6);
    strncpy(direction, "back", 6);
    cis[0].direction = direction;
    CameraFrameDim* fdim = malloc(sizeof(CameraFrameDim));
    fdim->width = 640;
    fdim->height = 480;
    cis[0].frame_sizes = fdim;
    cis[0].frame_sizes_num = 1;
    cis[0].pixel_format = V4L2_PIX_FMT_YUV420;

    return 1;
}

/**
 * Called from AMQP thread
 * changes the filename and resets video state
 * The mutex ensures no other operations are done on video state until anything is complete.
 */
void switch_video_files(CameraDevice* cd, const char* const filename)
{
    pthread_mutex_lock(&dec_mtx);
    if (access(filename, F_OK) != -1)
    {
        strncpy(camera_filename, filename, sizeof(camera_filename));
        if (cd != NULL)
        {
            video_dec_t* ctx = (video_dec_t*) cd->opaque;
            stop_video_dec(ctx);
            start_video_dec(ctx, camera_filename);
            I("Camera file name changed to %s", filename);
        }
    }
    else
    {
        W("File %s not found, resetting to default", filename);
        strncpy(camera_filename, default_filename, sizeof(camera_filename));
    }
    pthread_mutex_unlock(&dec_mtx);
}
