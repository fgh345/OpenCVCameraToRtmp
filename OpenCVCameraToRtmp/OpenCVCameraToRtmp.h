#pragma once

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "libavdevice/avdevice.h"
#include "libavutil/time.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/imgutils.h"
}

#pragma comment(lib,"opencv_world440.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avdevice.lib")
#pragma comment(lib,"avfilter.lib")


class TransmitBean
{
public:
	AVFrame* avFrame = NULL;
	SwsContext* swsContext = NULL;
	AVCodecContext* codecContext_output = NULL;

	AVFormatContext* formatContext_input = NULL;
	AVCodecContext* codecContext_input = NULL;
	AVFilterContext* buffer_src_ctx = NULL;
	AVFilterContext* buffer_sink_ctx = NULL;
};

int initFFmpegFormat();
TransmitBean initCameraByOpencv();
TransmitBean initMicrophone();
int preFFmpegFormat();
void startVideo(TransmitBean tb);
void startAudio(TransmitBean tb);

void releaseALL();

