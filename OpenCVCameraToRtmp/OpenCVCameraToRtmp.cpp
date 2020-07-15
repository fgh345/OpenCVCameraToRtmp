// OpenCVCameraToRtmp.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

using namespace cv;
using namespace std;

#pragma comment(lib,"opencv_world440.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")

int main()
{

	char file_out_path[] = "../out/result_file_camera_to_h264.flv";

	VideoCapture cap;
	namedWindow("preview");
	int ret = cap.open(0, CAP_DSHOW);

	int width = cap.get(CAP_PROP_FRAME_WIDTH);
	int heigth = cap.get(CAP_PROP_FRAME_HEIGHT);
	int fps = cap.get(CAP_PROP_FPS);
	if (fps == 0)
	{
		fps = 25;
	}


	SwsContext* swsContext = sws_getCachedContext(NULL, width, heigth, AV_PIX_FMT_BGR24,
		width, heigth, AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, 0, 0, 0);

	AVFrame* avFrame = av_frame_alloc();
	avFrame->format = AV_PIX_FMT_YUV420P;
	avFrame->width = width;
	avFrame->height = heigth;
	avFrame->pts = 0;

	ret = av_frame_get_buffer(avFrame, 32);

	if (ret != 0) {
		char buff[1024] = { 0 };
		av_strerror(ret, buff, sizeof(buff) - 1);
		cout << buff << endl;
		return -1;
	}

	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);

	AVCodecContext* aVCodecContext = avcodec_alloc_context3(codec);
	aVCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//全局参数
	aVCodecContext->codec_id = codec->id;
	aVCodecContext->thread_count = 2;//设置编码线程
	aVCodecContext->bit_rate = 50 * 1024 * 8;//单位bit
	aVCodecContext->width = width;
	aVCodecContext->height = heigth;
	aVCodecContext->time_base = { 1 , fps };
	aVCodecContext->framerate = { fps ,1 };
	aVCodecContext->gop_size = 50;
	aVCodecContext->max_b_frames = 0;
	aVCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;


	ret = avcodec_open2(aVCodecContext, NULL, NULL);


	AVFormatContext* ctx_out = NULL;
	avformat_alloc_output_context2(&ctx_out, NULL, "flv", file_out_path);
	AVStream* stream = avformat_new_stream(ctx_out, NULL);

	stream->codecpar->codec_tag = 0;
	avcodec_parameters_from_context(stream->codecpar, aVCodecContext);

	av_dump_format(ctx_out, 0, file_out_path, 1);

	//Open output URL
	if (avio_open(&ctx_out->pb, file_out_path, AVIO_FLAG_READ_WRITE) < 0) {
		printf("Failed to open output file! \n");
		return -1;
	}

	//写入封装头
	ret = avformat_write_header(ctx_out, NULL);

	AVPacket* pack = av_packet_alloc();

	while (1)
	{
		Mat frame;//定义一个变量把视频源一帧一帧显示
		if (!cap.grab())
		{
			continue;
		}

		if (!cap.retrieve(frame)) {
			continue;
		}

		imshow("preview", frame);
		waitKey(30);

		//输入的数据结构
		uint8_t* inData[AV_NUM_DATA_POINTERS] = { 0 };

		inData[0] = frame.data;

		int inSize[AV_NUM_DATA_POINTERS] = { 0 };
		inSize[0] = frame.cols * frame.elemSize();

		int h = sws_scale(swsContext, inData, inSize, 0, frame.rows,
			avFrame->data, avFrame->linesize);

		if (h <= 0)
		{
			continue;
		}

		avFrame->pts = avFrame->pts + 1;

		if (avcodec_send_frame(aVCodecContext, avFrame))
		{
			if (avcodec_receive_packet(aVCodecContext, pack) == 0)
			{
				if (pack->size > 0) {

					pack->pts = av_rescale_q(pack->pts, aVCodecContext->time_base, stream->time_base);// 前面是原始的 ,后面是要转换成为的
					pack->dts = av_rescale_q(pack->dts, aVCodecContext->time_base, stream->time_base);
					pack->duration = av_rescale_q(pack->duration, aVCodecContext->time_base, stream->time_base);

					//aVCodecContext->time_base 设置时间戳 
					//stream->time_base 输出时间戳 一定要转换

					av_interleaved_write_frame(ctx_out, pack);
				}
			}
		}

		cout << h << " " << flush;

	}

	//写入封装尾
	av_write_trailer(ctx_out);


	cap.release();
	sws_freeContext(swsContext);
	swsContext = NULL;
	av_frame_free(&avFrame);
	avcodec_free_context(&aVCodecContext);
	av_packet_unref(pack);
}