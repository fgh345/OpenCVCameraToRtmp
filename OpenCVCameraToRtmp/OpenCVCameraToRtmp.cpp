#include "OpenCVCameraToRtmp.h"

using namespace cv;
using namespace std;


int width_output = 1280;
int heigth_output = 720;
int fps_input = 60;//设置fps
int out_video_bit_rate = 128000;//视频比特率

int out_sample_rate = 44100;//音频采样
int out_audio_bit_rate = 64000;//音频比特率

char vdevice_in_url[] = "video=screen-capture-recorder";//Surface Camera Front | USB2.0 Camera | (TC-UB570, Video Capture) | screen-capture-recorder
char adevice_in_url[] = "audio=virtual-audio-capturer";//麦克风设备 virtual-audio-capturer ub570 (TC-UB570, Audio Capture)
//char file_out_path[] = "rtmp://49.232.191.221/live/hkz";//rtmp://49.232.191.221/live/hkz | rtmp://192.168.30.20/live/livestream
char file_out_path[] = "../out/result_file.flv";

//int64_t startTime;

AVFormatContext* formatContext_output;

bool isRun = true;

int main()
{

	initFFmpegFormat();
	TransmitBean tb_video = initCamera();
	//TransmitBean tb_audio = initMicrophone();
	preFFmpegFormat();

	thread thread_video([tb_video]() {
		startVideo(tb_video);
	});

	//thread thread_audio([tb_audio]() {
	//	startAudio(tb_audio);
	//});



	getchar();

}

TransmitBean initCamera() {
	AVDictionary* options = NULL;
	av_dict_set_int(&options, "rtbufsize", 3041280 * 20, 0);

	AVFormatContext* formatContext_input = avformat_alloc_context();

	AVInputFormat* fmt = av_find_input_format("dshow");

	avformat_open_input(&formatContext_input, vdevice_in_url, fmt, &options);

	avformat_find_stream_info(formatContext_input, NULL);

	AVStream* stream_input = formatContext_input->streams[0];//我的相机只有一个输入流 简单写了

	//处理解码器


	AVCodec* codec_input = avcodec_find_decoder(stream_input->codecpar->codec_id);
	AVCodecContext* codecContext_input = avcodec_alloc_context3(codec_input);
	avcodec_parameters_to_context(codecContext_input, stream_input->codecpar);

	if (avcodec_open2(codecContext_input, codec_input, NULL) != 0)
		printf("开启解码器失败!");

	AVPixelFormat pix_fmt_out = AV_PIX_FMT_YUV420P;
	AVFrame* frame420p = av_frame_alloc();

	//计算输出缓存
	int image_buffer_size = av_image_get_buffer_size(
		pix_fmt_out,//out
		width_output,
		heigth_output,
		1);

	//输出缓存
	uint8_t* image_buffer = new uint8_t[image_buffer_size];

	av_image_fill_arrays(
		frame420p->data,//out
		frame420p->linesize,//out
		image_buffer,//out
		pix_fmt_out,//out
		width_output,//out
		heigth_output,//out
		1);

	SwsContext* swsContext = sws_getContext(
		codecContext_input->width,//in
		codecContext_input->height,//in
		codecContext_input->pix_fmt,//in
		width_output,//out
		heigth_output,//out
		pix_fmt_out,//out
		SWS_BICUBIC,
		NULL, NULL, NULL);

	//处理编码器
	AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);

	AVCodecContext* codecContext_output = avcodec_alloc_context3(encoder);
	codecContext_output->pix_fmt = pix_fmt_out;//像素格式  Test_480x272.yuv必须和次类型相等 否则需要转码
	codecContext_output->width = width_output;//视频宽
	codecContext_output->height = heigth_output;//视频高
	codecContext_output->time_base.num = 1;//时间基 分子
	codecContext_output->time_base.den = 1;//时间基 分母 //这里必填 但是 设置又无效.....
	codecContext_output->gop_size = 10;// 关键帧 = 总帧数/gop_size
	codecContext_output->qmin = 4;//决定像素块大小 qmin越大  画面块状越明显
	codecContext_output->qmax = 20;
	codecContext_output->max_b_frames = 4;//设置b帧是p帧的倍数

	AVStream* stream_output = avformat_new_stream(formatContext_output, NULL);

	avcodec_parameters_from_context(stream_output->codecpar, codecContext_output);

	// Set Option
	AVDictionary* param = 0;
	//H.264
	if (codecContext_output->codec_id == AV_CODEC_ID_H264) {
		av_dict_set(&param, "preset", "superfast", 0);//superfast slow
		av_dict_set(&param, "tune", "zerolatency", 0);
	}
	
	if (avcodec_open2(codecContext_output, encoder, &param) < 0)
		printf("开启编码器失败!");


	TransmitBean tb;
	tb.avFrame = frame420p;
	tb.swsContext = swsContext;
	tb.codecContext_output = codecContext_output;

	return tb;

	//while (av_read_frame(formatContext_input, avpkt_in) == 0)
	//{
	//	w_frame_time_start = av_gettime();

	//	if (avcodec_send_packet(codecContext_input, avpkt_in) == 0)
	//		if (avcodec_receive_frame(codecContext_input, frameOriginal) == 0)
	//		{

	//			//转换像素数据格式
	//			sws_scale(swsContext_input,
	//				(const uint8_t* const*)frameOriginal->data,
	//				frameOriginal->linesize,
	//				0,
	//				codecContext_input->height,
	//				frame420p->data,
	//				frame420p->linesize);

	//			frame420p->format = pix_fmt_out;
	//			frame420p->width = width_output;
	//			frame420p->height = heigth_output;
	//			frame420p->pts = pst_p++ * duration;

	//			//Encode
	//			if (avcodec_send_frame(codecContext_output, frame420p) == 0) {
	//				if (avcodec_receive_packet(codecContext_output, avpkt_out) == 0)
	//				{
	//					//printf("Succeed to encode %d frame!\n", pst_p++);

	//					avpkt_out->duration = duration;

	//					//printf("pts:%d--dts:%d--duration:%d\n", avpkt_out->pts, avpkt_out->dts, avpkt_out->duration);

	//					int ret = av_write_frame(formatContext_output, avpkt_out);

	//					if (ret != 0)
	//						printf("write fail %d!\n", ret);

	//					av_packet_unref(avpkt_out);


	//					//控制帧率
	//					int64_t duration_new = av_rescale_q(duration, stream_output->time_base, { 1,AV_TIME_BASE });
	//					int64_t spanTime = duration_new - (av_gettime() - w_frame_time_start);

	//					printf("duration_new:%d --- %d  --- %d\n", duration_new, duration, spanTime);

	//					if (spanTime > 0)
	//						av_usleep(spanTime);


	//					int64_t dv = av_gettime() - w_frame_time_start;//大
	//					int real_frame_rate = stream_output->time_base.den / av_rescale_q(dv, { 1,AV_TIME_BASE }, stream_output->time_base);
	//					printf("实际帧率:%d\n", real_frame_rate);

	//				}
	//			}
	//		}
	//	av_packet_unref(avpkt_in);
	//}

	////Write file trailer
	//av_write_trailer(formatContext_output);

	////Clean
	//if (stream_output) {
	//	avcodec_close(codecContext_output);
	//	av_free(frame420p);
	//}
	//avio_close(formatContext_output->pb);
	//avformat_free_context(formatContext_output);
}

//TransmitBean initCameraByOpencv() {
//
//	int ret = 0;
//
//	VideoCapture cap;
//
//	try
//	{
//		ret = cap.open(0);
//	}
//	catch (const std::exception&)
//	{
//	}
//	if (ret == 0)
//	{
//		cout << "打开摄像头失败!" << endl;
//	}
//
//
//	int width_input = cap.get(CAP_PROP_FRAME_WIDTH);
//	int heigth_input = cap.get(CAP_PROP_FRAME_HEIGHT);
//
//	if (fps_input > cap.get(CAP_PROP_FPS)) {
//		fps_input = cap.get(CAP_PROP_FPS);
//	}
//	printf("实际fps:%d \n", fps_input);
//
//	SwsContext* swsContext = sws_getCachedContext(NULL, width_input, heigth_input, AV_PIX_FMT_BGR24,
//		width_output, heigth_output, AV_PIX_FMT_YUV420P,
//		SWS_BICUBIC, 0, 0, 0);
//
//	AVFrame* avFrame = av_frame_alloc();
//	avFrame->format = AV_PIX_FMT_YUV420P;
//	avFrame->width = width_output;
//	avFrame->height = heigth_output;
//	avFrame->pts = 0;
//
//	ret = av_frame_get_buffer(avFrame, 0);
//	if (ret)
//	{
//		cout << "av_frame_get_buffer error!" << endl;
//	}
//
//	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
//
//	AVCodecContext* codecContext_output = avcodec_alloc_context3(codec);
//	codecContext_output->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//全局参数
//	codecContext_output->thread_count = 8;//设置编码线程
//	codecContext_output->bit_rate = out_video_bit_rate;//单位bit
//	codecContext_output->width = width_output;
//	codecContext_output->height = heigth_output;
//	codecContext_output->time_base = { 1 , fps_input };
//	codecContext_output->framerate = { fps_input ,1 };
//	codecContext_output->gop_size = 10;
//	codecContext_output->max_b_frames = 0; //每一个p帧对应的b帧数
//	codecContext_output->codec_tag = 0;
//	codecContext_output->pix_fmt = AV_PIX_FMT_YUV420P;
//
//	ret = avcodec_open2(codecContext_output, NULL, NULL);
//	if (ret)
//	{
//		cout << "avcodec_open2 error!" << endl;
//	}
//
//	AVStream* stream_output = avformat_new_stream(formatContext_output, NULL);
//
//	avcodec_parameters_from_context(stream_output->codecpar, codecContext_output);
//
//
//	//cap.release();
//	//sws_freeContext(swsContext);
//	//swsContext = NULL;
//	//av_frame_free(&avFrame);
//	//avcodec_free_context(&aVCodecContext);
//	//av_packet_unref(pack);
//
//	TransmitBean tb;
//	tb.avFrame = avFrame;
//	tb.swsContext = swsContext;
//	tb.codecContext_output = codecContext_output;
//
//	return tb;
//}

TransmitBean initMicrophone() {

	AVFormatContext* formatContext_input = NULL;

	AVInputFormat* fmt = av_find_input_format("dshow");

	AVDictionary* options = NULL;
	av_dict_set_int(&options, "audio_buffer_size", 20, 0);
	//av_dict_set(&options, "list_devices", "true", 0);

	avformat_open_input(&formatContext_input, adevice_in_url, fmt, &options);

	av_dict_free(&options);

	avformat_find_stream_info(formatContext_input, NULL);

	AVStream* stream_input = formatContext_input->streams[0];

	//处理解码器
	AVCodec* codec_input = avcodec_find_decoder(stream_input->codecpar->codec_id);

	AVCodecContext* codecContext_input = avcodec_alloc_context3(codec_input);

	avcodec_parameters_to_context(codecContext_input, stream_input->codecpar);


	if (avcodec_open2(codecContext_input, codec_input, NULL) != 0)
		printf("开启解码器失败!");

	//打印输入流信息
	av_dump_format(formatContext_input, 0, adevice_in_url, 0);

	/// <summary>
	/// 输出模块
	/// </summary>

	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_FLTP;
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;

	AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);

	AVCodecContext* codecContext_output = avcodec_alloc_context3(encoder);
	codecContext_output->codec = encoder;
	codecContext_output->sample_fmt = out_sample_fmt;
	codecContext_output->sample_rate = out_sample_rate;
	codecContext_output->channel_layout = out_channel_layout;
	codecContext_output->channels = av_get_channel_layout_nb_channels(out_channel_layout);
	codecContext_output->bit_rate = out_audio_bit_rate;
	codecContext_output->codec_tag = 0;//表示不用封装器做编码
	codecContext_output->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	codecContext_output->thread_count = 8;

	if (avcodec_open2(codecContext_output, encoder, NULL) < 0)
		printf("开启编码器失败!");

	AVStream* stream_output = avformat_new_stream(formatContext_output, encoder);

	avcodec_parameters_from_context(stream_output->codecpar, codecContext_output);

	stream_output->time_base = { 1,AV_TIME_BASE };


	//配置过滤器 重新采用音频
	char args[512];
	const AVFilter* abuffersrc = avfilter_get_by_name("abuffer");
	const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
	AVFilterInOut* outputs = avfilter_inout_alloc();
	AVFilterInOut* inputs = avfilter_inout_alloc();

	AVFilterContext* buffer_src_ctx;
	AVFilterContext* buffer_sink_ctx;
	AVFilterGraph* filter_graph = avfilter_graph_alloc();
	filter_graph->nb_threads = 1;

	sprintf_s(args, sizeof(args),
		"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
		stream_input->time_base.num,
		stream_input->time_base.den,
		stream_input->codecpar->sample_rate,
		av_get_sample_fmt_name((AVSampleFormat)stream_input->codecpar->format),
		av_get_default_channel_layout(codecContext_input->channels));



	int ret = avfilter_graph_create_filter(&buffer_src_ctx, abuffersrc, "in",
		args, NULL, filter_graph);
	if (ret)
	{
		printf("avfilter_graph_create_filter error \n");
	}

	ret = avfilter_graph_create_filter(&buffer_sink_ctx, abuffersink, "out",
		NULL, NULL, filter_graph);
	if (ret)
	{
		printf("avfilter_graph_create_filter error \n");
	}

	static const enum AVSampleFormat out_sample_fmts[] = { AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE };
	static const int64_t out_channel_layouts[] = { stream_output->codecpar->channel_layout, -1 };
	static const int out_sample_rates[] = { stream_output->codecpar->sample_rate , -1 };

	ret = av_opt_set_int_list(buffer_sink_ctx, "sample_fmts", out_sample_fmts, -1,
		AV_OPT_SEARCH_CHILDREN);
	ret = av_opt_set_int_list(buffer_sink_ctx, "channel_layouts", out_channel_layouts, -1,
		AV_OPT_SEARCH_CHILDREN);
	ret = av_opt_set_int_list(buffer_sink_ctx, "sample_rates", out_sample_rates, -1,
		AV_OPT_SEARCH_CHILDREN);

	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffer_src_ctx;;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffer_sink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	ret = avfilter_graph_parse_ptr(filter_graph, "anull", &inputs, &outputs, nullptr);
	if (ret < 0)
	{
		printf("avfilter_graph_parse_ptr error \n");
	}

	ret = avfilter_graph_config(filter_graph, NULL);
	if (ret)
	{
		printf("avfilter_graph_config error \n");
	}

	av_buffersink_set_frame_size(buffer_sink_ctx, 1024);

	TransmitBean tb;
	tb.formatContext_input = formatContext_input;
	tb.codecContext_input = codecContext_input;
	tb.buffer_src_ctx = buffer_src_ctx;
	tb.buffer_sink_ctx = buffer_sink_ctx;
	tb.codecContext_output = codecContext_output;

	return tb;
}

int initFFmpegFormat() {

	avdevice_register_all();

	avformat_alloc_output_context2(&formatContext_output, NULL, "flv", file_out_path);

	//startTime = av_gettime();

	return 0;
}

int preFFmpegFormat() {
	//Open output URL
	if (avio_open2(&formatContext_output->pb, file_out_path, AVIO_FLAG_READ_WRITE, NULL, NULL) < 0) {
		printf("Failed to open output file! \n");
		return -1;
	}

	//打印输出流信息
	av_dump_format(formatContext_output, 0, file_out_path, 1);



	//写入文件头
	avformat_write_header(formatContext_output, NULL);

	for (int i = 0; i < formatContext_output->nb_streams; i++)
	{
		cout << "stream_timebase" << i << ";" << formatContext_output->streams[i]->time_base.den << endl;
	}


	printf("初始化完成!\n");
}

void startVideo(TransmitBean tb) {

	AVFrame* avFrame = tb.avFrame;
	SwsContext* swsContext = tb.swsContext;
	AVCodecContext* codecContext_output = tb.codecContext_output;


	AVFrame* frameOriginal = av_frame_alloc();
	AVPacket* pack = av_packet_alloc();


	long long pts = 0;

	while (isRun)
	{

		//w_frame_time_start = av_gettime();

		if (avcodec_send_packet(codecContext_output, pack) == 0)
			if (avcodec_receive_frame(codecContext_output, frameOriginal) == 0)
			{

				//转换像素数据格式
				sws_scale(swsContext,
					(const uint8_t* const*)frameOriginal->data,
					frameOriginal->linesize,
					0,
					codecContext_output->height,
					avFrame->data,
					avFrame->linesize);

				avFrame->pts = av_rescale_q(pts++, codecContext_output->time_base, formatContext_output->streams[0]->time_base);


				if (avcodec_send_frame(codecContext_output, avFrame))
				{
					if (avcodec_receive_packet(codecContext_output, pack) == 0)
					{
						if (pack->size > 0) {

							pack->pos = -1;
							pack->stream_index = 0;

							//printf("pts:%d,dts:%d,size:%d,duration:%d\n", pack->pts, pack->dts, pack->size, pack->duration);

							av_interleaved_write_frame(formatContext_output, pack);

							cout << "#" << flush;
						}
					}
				}

				
			}

	}
}

void startAudio(TransmitBean tb) {

	AVFormatContext* formatContext_input = tb.formatContext_input;
	AVCodecContext* codecContext_input = tb.codecContext_input;
	AVFilterContext* buffer_src_ctx = tb.buffer_src_ctx;
	AVFilterContext* buffer_sink_ctx = tb.buffer_sink_ctx;
	AVCodecContext* codecContext_output = tb.codecContext_output;


	AVPacket* avpkt_in = av_packet_alloc();
	AVPacket* avpkt_out = av_packet_alloc();

	AVFrame* frameOriginal = av_frame_alloc();
	AVFrame* frameAAC = av_frame_alloc();

	long long pts = 0;

	int stream_index = formatContext_output->nb_streams - 1;

	while (isRun)
	{
		if (av_read_frame(formatContext_input, avpkt_in) == 0)
		{

			if (0 >= avpkt_in->size)
			{
				continue;
			}

			if (avcodec_send_packet(codecContext_input, avpkt_in) == 0)

				if (avcodec_receive_frame(codecContext_input, frameOriginal) == 0)
				{

					//pst_p += av_rescale_q(frameOriginal->nb_samples, codecContext_output->time_base, stream_output->time_base);

					//printf("%d ", pst_p);

					//转换数据格式
					int ret = av_buffersrc_add_frame_flags(buffer_src_ctx, frameOriginal, AV_BUFFERSRC_FLAG_PUSH);
					if (ret < 0)
					{
						//XError(ret);
					}

					ret = av_buffersink_get_frame_flags(buffer_sink_ctx, frameAAC, AV_BUFFERSINK_FLAG_NO_REQUEST);
					if (ret < 0)
					{
						continue;
					}

					frameAAC->pts = av_rescale_q(frameAAC->nb_samples * pts++, codecContext_output->time_base, formatContext_output->streams[stream_index]->time_base);

					if (avcodec_send_frame(codecContext_output, frameAAC) == 0) {
						if (avcodec_receive_packet(codecContext_output, avpkt_out) == 0)
						{
							if (avpkt_out->size > 0) {

								avpkt_out->pos = -1;
								avpkt_out->stream_index = stream_index;

								//printf("pts:%d,dts:%d,size:%d,duration:%d\n", avpkt_out->pts, avpkt_out->dts, avpkt_out->size, avpkt_out->duration);

								ret = av_interleaved_write_frame(formatContext_output, avpkt_out);
								if (ret != 0)
								{
									//XError(ret);
								}
								cout << "@" << flush;
							}
						}
					}
				}
			av_packet_unref(avpkt_in);

		}
	}
}

void releaseALL() {

}
