// OpenCVCameraToRtmp.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。

#include "OpenCVCameraToRtmp.h"

using namespace cv;
using namespace std;


int width_output = 1920;
int heigth_output = 1080;

int out_sample_rate = 44100;//音频采样
int out_audio_bit_rate = 64000;

char device_in_url[] = "audio=virtual-audio-capturer";//麦克风设备
char file_out_path[] = "../out/result_file.flv";

int main()
{

	

	

	//std::thread thread_camera(initCamera);

	std::thread thread_Microphone(initMicrophone);

	getchar();

	//namedWindow("preview");



	//if (ret != 0) {
	//	char buff[1024] = { 0 };
	//	av_strerror(ret, buff, sizeof(buff) - 1);
	//	cout << buff << endl;
	//	return -1;
	//}


	

	//写入封装尾
	//av_write_trailer(ctx_out);



}

int initCamera() {

	int ret = 0;

	VideoCapture cap;
	try
	{
		ret = cap.open(0);
	}
	catch (const std::exception&)
	{
	}
	if (ret==0)
	{
		cout << "打开摄像头失败!" << endl;
	}
	

	int width_input = cap.get(CAP_PROP_FRAME_WIDTH);
	int heigth_input = cap.get(CAP_PROP_FRAME_HEIGHT);
	int fps_input = cap.get(CAP_PROP_FPS);
	if (fps_input == 0)
	{
		fps_input = 30;
	}


	SwsContext* swsContext = sws_getCachedContext(NULL, width_input, heigth_input, AV_PIX_FMT_BGR24,
		width_output, heigth_output, AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, 0, 0, 0);

	AVFrame* avFrame = av_frame_alloc();
	avFrame->format = AV_PIX_FMT_YUV420P;
	avFrame->width = width_output;
	avFrame->height = heigth_output;
	avFrame->pts = 0;

	ret = av_frame_get_buffer(avFrame, 0);
	if (ret)
	{
		cout << "av_frame_get_buffer error!" << endl;
	}

	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);

	AVCodecContext* aVCodecContext = avcodec_alloc_context3(codec);
	aVCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//全局参数
	aVCodecContext->thread_count = 2;//设置编码线程
	aVCodecContext->bit_rate = 50 * 1024 * 8;//单位bit
	aVCodecContext->width = width_output;
	aVCodecContext->height = heigth_output;
	aVCodecContext->time_base = { 1 , fps_input };
	aVCodecContext->framerate = { fps_input ,1 };
	aVCodecContext->gop_size = 50;
	aVCodecContext->max_b_frames = 0;
	aVCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;

	ret = avcodec_open2(aVCodecContext, NULL, NULL);
	if (ret)
	{
		cout << "avcodec_open2 error!" << endl;
	}

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

		//cout << h << " " << flush;

	}


	cap.release();
	sws_freeContext(swsContext);
	swsContext = NULL;
	av_frame_free(&avFrame);
	avcodec_free_context(&aVCodecContext);
	av_packet_unref(pack);

	return 0;
}

int initMicrophone() {
	avdevice_register_all();

	AVFormatContext* formatContext_input = NULL;

	AVInputFormat* fmt = av_find_input_format("dshow");

	AVDictionary* options = NULL;
	av_dict_set_int(&options, "audio_buffer_size", 20, 0);

	avformat_open_input(&formatContext_input, device_in_url, fmt, &options);

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
	av_dump_format(formatContext_input, 0, device_in_url, 0);

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

	AVFormatContext* formatContext_output;
	avformat_alloc_output_context2(&formatContext_output, NULL, NULL, file_out_path);

	AVStream* stream_output = avformat_new_stream(formatContext_output, encoder);

	avcodec_parameters_from_context(stream_output->codecpar, codecContext_output);

	//Open output URL
	if (avio_open2(&formatContext_output->pb, file_out_path, AVIO_FLAG_READ_WRITE, NULL, NULL) < 0) {
		printf("Failed to open output file! \n");
		return -1;
	}

	//打印输出流信息
	av_dump_format(formatContext_output, 0, file_out_path, 1);


	//写入文件头
	avformat_write_header(formatContext_output, NULL);

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

	int pst_p = 0;

	AVPacket* avpkt_in = av_packet_alloc();
	AVPacket* avpkt_out = av_packet_alloc();

	AVFrame* frameOriginal = av_frame_alloc();
	AVFrame* frameAAC = av_frame_alloc();

	for (;;) {
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
					ret = av_buffersrc_add_frame_flags(buffer_src_ctx, frameOriginal, AV_BUFFERSRC_FLAG_PUSH);
					if (ret < 0)
					{
						//XError(ret);
					}

					ret = av_buffersink_get_frame_flags(buffer_sink_ctx, frameAAC, AV_BUFFERSINK_FLAG_NO_REQUEST);
					if (ret < 0)
					{
						continue;
					}

					//pts 计算
					// second = nb_samples/sample_rate   一帧音频的秒数
					// pts = second/timebase 

					//Encode
					//AVRational itime = stream_input->time_base;
					//AVRational otime = stream_output->time_base;



					frameAAC->pts = pst_p;

					pst_p += av_rescale_q(frameAAC->nb_samples, codecContext_output->time_base, stream_output->time_base);

					if (avcodec_send_frame(codecContext_output, frameAAC) == 0) {
						if (avcodec_receive_packet(codecContext_output, avpkt_out) == 0)
						{
							if (avpkt_out->size > 0) {

								//avpkt_out->pts = av_rescale_q_rnd(avpkt_in->pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));// 前面是原始的 ,后面是要转换成为的
								//avpkt_out->dts = av_rescale_q_rnd(avpkt_in->dts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
								//avpkt_out->duration = av_rescale_q_rnd(avpkt_in->duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

								avpkt_out->pts = av_rescale_q(avpkt_out->pts, codecContext_output->time_base, stream_output->time_base);// 前面是原始的 ,后面是要转换成为的
								avpkt_out->dts = av_rescale_q(avpkt_out->dts, codecContext_output->time_base, stream_output->time_base);
								avpkt_out->duration = av_rescale_q(avpkt_out->duration, codecContext_output->time_base, stream_output->time_base);
								avpkt_out->pos = -1;

								printf("pst_p:%d,pts:%d,dts:%d,size:%d,duration:%d\n", pst_p, avpkt_out->pts, avpkt_out->dts, avpkt_out->size, avpkt_out->duration);

								ret = av_interleaved_write_frame(formatContext_output, avpkt_out);
								if (ret != 0)
								{
									//XError(ret);
								}
							}
						}
					}
				}
			av_packet_unref(avpkt_in);

		}
	}


	return 0;
}

void releaseALL() {

}
