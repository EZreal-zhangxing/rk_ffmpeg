#ifndef FFMPEG_HEAD
#define FFMPEG_HEAD

#include "command_push.h"

extern "C"{
#include<libavcodec/avcodec.h>
#include<libavdevice/avdevice.h>
#include<libavformat/avformat.h>
#include<libavutil/imgutils.h>
#include<libswscale/swscale.h>
#include<libavutil/pixdesc.h>
#include<libavutil/hwcontext.h>
#include<libavutil/time.h>
}

int init_ffmpeg_capture(COMMAND_PUSH_OBJ &obj);

int init_ffmpeg_push(COMMAND_PUSH_OBJ &obj);

void destroy_ffmpeg();
#endif
