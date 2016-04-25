#ifndef CAMERA_CAPTURE_FFMPEG_H

#define CAMERA_CAPTURE_FFMPEG_H

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavformat/avformat.h>
#include "camera-common.h"

void switch_video_files(CameraDevice* cd, const char* const filename);
#endif
