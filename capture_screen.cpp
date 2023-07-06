#include "ffmpeg_head.h"

#include<iostream>
#include<string>

const AVPixelFormat target_pix_fmt = AV_PIX_FMT_YUV420P; // AV_PIX_FMT_YUV420P AV_PIX_FMT_NV12
const AVPixelFormat input_pix_fmt = AV_PIX_FMT_BGRA; // AV_PIX_FMT_NV12 AV_PIX_FMT_BGRA
extern int width = 1440,height = 900;



/**
 * 捕获流相关对象
*/
extern const AVInputFormat *inputFmt = 0;
extern AVFormatContext *inputFmtCtx = 0;
extern AVCodecParameters *inputCodecPar = 0;
extern const AVCodec *inputCodec = 0;
extern AVCodecContext *inputCodeCtx = 0;

extern SwsContext *swsCtx = 0;
extern AVFrame * reviceFrame = 0;//从x11grab中拿到的帧
extern AVFrame* frame = 0; //转换成YUV420P
extern AVPacket *packet = 0;
extern int inputVideoStream = -1;

/**
 * 输出流相关对象
*/
extern AVFormatContext *outputFmtCtx = 0; 
extern const AVCodec *outputCodec = 0; // 解码器
extern AVCodecContext *outputCodecCtx = 0; // 解码器上下文
extern AVStream * stream = 0; // 输出流


using namespace std;

string get_pix_str(){
    char width_temp[7] = "";
    char height_temp[7] = "";
    sprintf(width_temp,"%d",width);
    sprintf(height_temp,"%d",height);
    return (string(width_temp) + "x" + string(height_temp));
}

/**
 * 初始化输入编码器的硬解码
*/
void hwwave_init(AVCodecContext *& codeCtx){
	AVHWDeviceType type = AV_HWDEVICE_TYPE_DRM;
	AVBufferRef *buff = av_hwdevice_ctx_alloc(type);

	int res = av_hwdevice_ctx_init(buff);
	if(res){
		cout << "init hw encoder failed ! code :" << res << endl;
	}
	codeCtx->hw_device_ctx = buff;
	cout << "init hardwave encoder success ! "<< buff << endl;
}

int init_ffmpeg_capture(COMMAND_PUSH_OBJ &obj){
    int res = 0;
    avdevice_register_all();
    inputFmt = av_find_input_format(obj.get_capture_name());
    if (!inputFmt){
		printf("can't find input device.\n");
		return -1;
	}
 
	AVDictionary *options = NULL;
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options,"video_size",get_pix_str().c_str(),0); 
    av_dict_set(&options, "draw_mouse", "1", 0);
    inputFmtCtx = avformat_alloc_context();
    res = avformat_open_input(&inputFmtCtx,":0.0", inputFmt, &options);
	if (res != 0){
		printf("can't open input stream. code:%d\n",res);
		return -1;
	}

    res = avformat_find_stream_info(inputFmtCtx, NULL);
	if (res < 0){
		printf("can't find stream information.\n");
		return -1;
	}
    
    inputVideoStream = av_find_best_stream(inputFmtCtx,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
    if (inputVideoStream == -1){
		printf("can't find a video stream.\n");
		return -1;
	}
    cout << "[" << __LINE__ << "] input video stream index " << inputVideoStream << endl;

    inputCodecPar = inputFmtCtx->streams[inputVideoStream]->codecpar;
    //找到对应的解码器
    if(obj.get_use_hw()){
        inputCodec = avcodec_find_decoder_by_name("h264_rkmpp");
    }else{
        inputCodec = avcodec_find_decoder_by_name("h264");
    }
    inputCodec = avcodec_find_decoder(inputCodecPar->codec_id);
    if(inputCodec == nullptr){
        printf("can not find decoder!");
        return -1;
    }
    cout << "[" << __LINE__ << "] find codec name " << inputCodec->name << " id is "<< inputCodec->id << endl;
    //通过解码器获取解码器的上下文
    inputCodeCtx = avcodec_alloc_context3(inputCodec);
    if(inputCodeCtx == 0){
        cout <<  "[" << __LINE__ << "] do not find codec context !"<< inputCodeCtx << endl;
        return -1;
    }
    cout << "[" << __LINE__ << "] pix_fmt " << inputCodeCtx->pix_fmt << endl;
    if(obj.get_use_hw()){
        hwwave_init(inputCodeCtx);
    }
    // 拷贝解码器上下文信息 如果不拷贝会出现pCodeCtx中缺失部分信息
	if (avcodec_parameters_to_context(inputCodeCtx, inputCodecPar) < 0) {
		printf("Failed to copy in_stream codecpar to codec context\n");
        return -1;
	}

    AVDictionary *avDict_open = NULL; // 键值对
    //打开解码器
    res = avcodec_open2(inputCodeCtx,inputCodec,&avDict_open);
    if(res != 0){
        cout << "open decoder failed!" << endl;
        return -1;
    }
    reviceFrame = av_frame_alloc();
    av_image_alloc(reviceFrame->data, reviceFrame->linesize, width, height,target_pix_fmt, 1);

    frame = av_frame_alloc();//分配空间
    frame->width = width;
    frame->height = height;
    frame->format = target_pix_fmt;
    av_image_alloc(frame->data, frame->linesize, width, height,target_pix_fmt, 1);

    packet = (AVPacket *)av_malloc(sizeof(AVPacket)); //数据包

    // 转换器
    swsCtx = sws_getContext(width,height,input_pix_fmt,
        width,height,target_pix_fmt,SWS_BICUBIC,0,0,0);
    cout << "init ffmpeg caputre finished!" << endl;
    return 0;
}

int init_ffmpeg_push(COMMAND_PUSH_OBJ &obj){
    int res = 0;
    // 初始化网络模块
    avformat_network_init();
    // 根据编码名找到编码器
    outputCodec = avcodec_find_encoder_by_name(obj.get_codec_name());
    // cout << "support pixformat is " << endl;
    // const enum AVPixelFormat * temp = outputCodec->pix_fmts;
    // while(temp != NULL && *temp != -1){
    //     cout << *temp << endl;
    //     temp++;
    // }
    cout << " check is encoder : " << av_codec_is_encoder(outputCodec) << endl;
    if(!outputCodec){
        cout << "can not find encoder by name " << obj.get_codec_name() << endl;
        return -1;
    }
    // 根据编码器创建上下文
    outputCodecCtx = avcodec_alloc_context3(outputCodec);
    if(!outputCodecCtx){
        cout << "can not create context of encoder !" << endl;
        return -1;
    }
    // 设置上下文参数
    outputCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; // 全局参数
    outputCodecCtx->codec_id = outputCodec->id;
    outputCodecCtx->codec = outputCodec;
    outputCodecCtx->bit_rate = 1024*1024*8;
    outputCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO; //解码类型
    outputCodecCtx->width = width;  // 宽
    outputCodecCtx->height = height; // 高
    outputCodecCtx->channels = 3;
    outputCodecCtx->time_base = (AVRational){1,obj.get_fps()}; // 每帧的时间
    outputCodecCtx->framerate = (AVRational){obj.get_fps(),1}; // 帧率
    outputCodecCtx->pix_fmt = target_pix_fmt;
    outputCodecCtx->gop_size = 10; // 每组多少帧
    outputCodecCtx->max_b_frames = 1; // b帧最大间隔

    if(obj.get_use_hw()){
        hwwave_init(outputCodecCtx);
    }
    // 创建Format上下文
    if(!strcmp(obj.get_protocol(),"rtsp")){
        // rtsp协议
        res = avformat_alloc_output_context2(&outputFmtCtx,NULL,"rtsp",obj.get_url());
    }else{
        // rtmp协议
        res = avformat_alloc_output_context2(&outputFmtCtx,NULL,"flv",obj.get_url());
    }
    
    if(res < 0){
        cout << "create output format failed ! code :" << res << endl;
        return -1;
    }
    // 创建输出流
    stream = avformat_new_stream(outputFmtCtx,outputCodec);
    if(!stream){
        cout << "create stream failed !" << endl;
        return -1;
    }
    // stream->codecpar->extradata = 
    // avFormatCtx->max_interleave_delta = 1000000;
    // stream->time_base = (AVRational){1,obj.get_fps()}; // 设置流的帧率
    // stream->id = outputFmtCtx->nb_streams - 1; 
    // stream->codecpar->codec_tag = 0;

    // 将编码器上下文的参数 复制到流对象中
    res = avcodec_parameters_from_context(stream->codecpar,outputCodecCtx);
    if(res < 0){
        cout << "copy parameters to stream failed! code :" << res << endl;
        return -1;
    }

    // 打开输出IO RTSP不需要打开，RTMP需要打开
    if(!strcmp(obj.get_protocol(),"rtmp")){
        // res = avio_open(&avFormatCtx->pb, obj.get_url(), AVIO_FLAG_WRITE);
        cout << "avio opened !" << endl;
        res = avio_open2(&outputFmtCtx->pb,obj.get_url(),AVIO_FLAG_WRITE,NULL,NULL);
        if(res < 0){
        	// AVERROR(EAGAIN);
        	cout << "avio open failed ! code: "<< res << endl;
            return -1;
        }
    }
    
    // 写入头信息   
    AVDictionary *opt = NULL;
    if(!strcmp(obj.get_protocol(),"rtsp")){
        av_dict_set(&opt, "rtsp_transport","tcp",0);
        av_dict_set(&opt, "muxdelay", "0.1", 0);
    }
    // av_dict_set(&opt, "rtsp_transport","tcp",0);
    // av_dict_set(&opt, "muxdelay", "0.1", 0);
	res = avformat_write_header(outputFmtCtx, &opt);
	if(res < 0){
		cout << "avformat write header failed ! code: "<< res << endl;
	}
    av_dump_format(outputFmtCtx, 0, obj.get_url(), 1);

    // open codec
	AVDictionary * opencodec = NULL;
	av_dict_set(&opencodec, "preset", obj.get_preset(), 0);
	av_dict_set(&opencodec, "tune", obj.get_tune(), 0);
    av_dict_set(&opencodec,"profile",obj.get_profile(),0);
    // 打开编码器
	res = avcodec_open2(outputCodecCtx,outputCodec,&opencodec);
	if(res < 0){
		cout << "open codec failed ! " << res <<endl; 
	}
    // 给packet 分配内存
    packet = av_packet_alloc();

    av_frame_get_buffer(frame, 0);
    // 让其可填充数据
    res = av_frame_make_writable(frame);
    if(res < 0){
		cout << "make frameYUV writeable failed ! code:" << res << endl;
	}
	cout << "init codec finished ! [w:" << width << " h:" << height << "]" << endl;
    return 0;
}


void destroy_ffmpeg(){
    cout << "free ffmpeg resource ...." << endl;
	// fclose(wf);
	if(inputFmtCtx){
        avformat_close_input(&inputFmtCtx);
        avformat_free_context(inputFmtCtx);
        inputFmtCtx = 0;
        cout << "avformat_free_context(inputFmtCtx)" << endl;
    }

    if(outputFmtCtx){
        avio_close(outputFmtCtx->pb);
        avformat_free_context(outputFmtCtx);
        outputFmtCtx = 0;
        cout << "avformat_free_context(outputFmtCtx)" << endl;
    }
    
    if(packet){
        av_packet_unref(packet);
        packet = NULL;
        cout << "av_free_packet(pAVPacket)" << endl;
    }
    if(swsCtx){
        sws_freeContext(swsCtx);
        swsCtx = 0;
        cout << "sws_freeContext(swsCtx)" << endl;;
    }
    
    if(frame){
        av_frame_free(&frame);
        frame = 0;
        cout << "av_frame_free(frame)" << endl;
    }
    
    if(inputCodeCtx->hw_device_ctx){
        av_buffer_unref(&inputCodeCtx->hw_device_ctx);
        cout << "av_buffer_unref(&inputCodeCtx->hw_device_ctx)" << endl;
    }
    if(inputCodeCtx){
        avcodec_close(inputCodeCtx);
        inputCodeCtx = 0;
        cout << "avcodec_close(inputCodeCtx);" << endl;
    }
    if(outputCodec){
        avcodec_close(inputCodeCtx);
        inputCodeCtx = 0;
        cout << "avcodec_close(inputCodeCtx);" << endl;
    }

    
}