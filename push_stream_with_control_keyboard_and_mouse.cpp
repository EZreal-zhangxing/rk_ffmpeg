#include<iostream>
#include<thread>
#include<opencv2/opencv.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include<opencv2/highgui/highgui.hpp>

#include "ffmpeg_head.h"

#include<SDL2/SDL.h>
#include<SDL2/SDL_thread.h>
#include<SDL2/SDL_error.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "command_push.h"
// #include "Remote_Server/RemoteKeyboardMouse_Server.h"

using namespace std;
using namespace cv;

int width = 0,height = 0; // 视频宽高
AVFormatContext *avFormatCtx = NULL; 
const AVCodec *avCodec = NULL; // 解码器
AVCodecContext *avCodecCtx = NULL; // 解码器上下文
AVStream * stream = NULL; // 输出流

AVFrame * frameYUV = NULL;
AVPacket * packet = NULL;
long extra_data_size = 10000000;
uint8_t* cExtradata = NULL;
int insize[AV_NUM_DATA_POINTERS] = {0}; // 一行的字节数

SwsContext *swsCtx = NULL;
AVPixelFormat in_pix_fmt = AV_PIX_FMT_RGB32; // 截屏是AV_PIX_FMT_RGB32 opencv Mat 数据格式 如果是摄像头或者HDMI是AV_PIX_FMT_BGR24
AVPixelFormat out_pix_fmt = AV_PIX_FMT_YUV420P; // 转换输出格式

SDL_Window *win = NULL; //SDL窗口
SDL_Renderer *ren = NULL; // 渲染器
SDL_Texture *texture = NULL; // 纹理
SDL_Surface * surface = NULL; //表面
SDL_Rect rect; // 活动矩形
SDL_Event event; // 事件

int framecount =0;

COMMAND_PUSH_OBJ process_command(int argc,char * argv[]){
    COMMAND_PUSH_OBJ res = COMMAND_PUSH_OBJ();
    for(int i=1;i<argc;i++){
        string flag = argv[i];
        if(flag == "-n" && i+1 < argc){
            res.set_url(argv[i+1]);
        }
        if(flag == "-nd"){
            //默认网路流
            res.set_url("rtsp://192.168.1.222:554/media");
        }
        if(flag == "-hd"){
            res.set_use_hw(1);
        }
        if(flag == "-cn" && i+1 < argc){
            res.set_codec_name(argv[i+1]);
        }
        if(flag == "-fps" && i+1 < argc){
            res.set_fps(strtol(argv[i+1],NULL,10));
        }
        if(flag == "-preset" && i+1 < argc){
            res.set_preset(argv[i+1]);
        }
        if(flag == "-profile" && i+1 < argc){
            res.set_profile(argv[i+1]);
        }
        if(flag == "-tune" && i+1 < argc){
            res.set_tune(argv[i+1]);
        }
        if(flag == "rtsp" || flag == "rtmp"){
            res.set_protocol(argv[i]);
        }
    }
    res.print_command_str();
    return res;
}

int ff_Error(int errNum){
	char buf[1024] = { 0 };
	av_strerror(errNum, buf, sizeof(buf));
	cout << buf << endl;
	return -1;
}

int sdl_init_process(){
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)){
        cout << "can not initialize sdl" << SDL_GetError();
        return -1;
    }
    //在(0,SDL_WINDOWPOS_CENTERED)处创建窗口，
    win = SDL_CreateWindow("video",50,SDL_WINDOWPOS_CENTERED,width,height,SDL_WINDOW_SHOWN);
    if(!win){
        cout << "create sdl win failed !" << SDL_GetError() << endl;
        return -1;
    }
    // 创建渲染器
    ren = SDL_CreateRenderer(win,-1,0);
    if(!ren){
        cout << "create sdl renderer failed !" << SDL_GetError() << endl;
        return -1;
    }
    // 创建指定像素格式的纹理
    texture = SDL_CreateTexture(ren,SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING,width,height);
    if(!texture){
        cout << "create sdl texture failed !" << SDL_GetError() << endl;
        return -1;
    }
    // if(!TTF_Init()){
    //     cout << "sdl ttf initial failed !" <<  SDL_GetError() << endl;
    // }
    // font = TTF_OpenFont("/home/firefly/mpp/test_video/SimHei.ttf",30);
    return 0;
}

void hwwave_init(){
	AVHWDeviceType type = AV_HWDEVICE_TYPE_DRM;
	AVBufferRef *buff = av_hwdevice_ctx_alloc(type);

	int res = av_hwdevice_ctx_init(buff);
	if(res){
		cout << "init hw encoder failed ! code :" << res << endl;
	}
	avCodecCtx->hw_device_ctx = buff;
	cout << "init hardwave encoder success ! "<< buff << endl;
}


void pps_sps_init(){
    uint8_t* cExtradata = (uint8_t *)malloc((packet->size + avCodecCtx->extradata_size) * sizeof(uint8_t));
    memcpy(cExtradata, avCodecCtx->extradata, avCodecCtx->extradata_size);
    memcpy(cExtradata + avCodecCtx->extradata_size, packet->data, packet->size);//&enc_packet.data[0]
    packet->size += avCodecCtx->extradata_size;
	packet->data = cExtradata;
}

int encoder_init(COMMAND_PUSH_OBJ obj){
    int res = 0;
    // 初始化网络模块
    avformat_network_init();
    // 根据编码名找到编码器
    avCodec = avcodec_find_encoder_by_name(obj.get_codec_name());
    if(!avCodec){
        cout << "can not find encoder by name " << obj.get_codec_name() << endl;
        return -1;
    }
    // 根据编码器创建上下文
    avCodecCtx = avcodec_alloc_context3(avCodec);
    if(!avCodecCtx){
        cout << "can not create context of encoder !" << endl;
        return -1;
    }
    // 设置上下文参数
    avCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; // 全局参数
    avCodecCtx->codec_id = avCodec->id;
    avCodecCtx->codec = avCodec;
    avCodecCtx->bit_rate = 1024*1024*8;
    avCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO; //解码类型
    avCodecCtx->width = width;  // 宽
    avCodecCtx->height = height; // 高
    avCodecCtx->channels = 3;
    avCodecCtx->time_base = (AVRational){1,obj.get_fps()}; // 每帧的时间
    avCodecCtx->framerate = (AVRational){obj.get_fps(),1}; // 帧率
    avCodecCtx->pix_fmt = out_pix_fmt;
    avCodecCtx->gop_size = 10; // 每组多少帧
    avCodecCtx->max_b_frames = 1; // b帧最大间隔

    if(!strcmp(obj.get_protocol(),"rtsp")){
        // 初始化包空间，进行PPS SPS包头填充
        cExtradata = (uint8_t *)malloc((extra_data_size) * sizeof(uint8_t));
    }
    // 创建Format上下文
    if(!strcmp(obj.get_protocol(),"rtsp")){
        // rtsp协议
        res = avformat_alloc_output_context2(&avFormatCtx,NULL,"rtsp",obj.get_url());
    }else{
        // rtmp协议
        res = avformat_alloc_output_context2(&avFormatCtx,NULL,"flv",obj.get_url());
    }
    
    if(res < 0){
        cout << "create output format failed ! code :" << res << endl;
        return -1;
    }
    // 创建输出流
    stream = avformat_new_stream(avFormatCtx,avCodec);
    if(!stream){
        cout << "create stream failed !" << endl;
        return -1;
    }
    // stream->codecpar->extradata = 
    // avFormatCtx->max_interleave_delta = 1000000;
    stream->time_base = (AVRational){1,obj.get_fps()}; // 设置流的帧率
    stream->id = avFormatCtx->nb_streams - 1; 
    stream->codecpar->codec_tag = 0;

    // 将编码器上下文的参数 复制到流对象中
    res = avcodec_parameters_from_context(stream->codecpar,avCodecCtx);
    if(res < 0){
        cout << "copy parameters to stream failed! code :" << res << endl;
        return -1;
    }

    // 打开输出IO RTSP不需要打开，RTMP需要打开
    if(!strcmp(obj.get_protocol(),"rtmp")){
        // res = avio_open(&avFormatCtx->pb, obj.get_url(), AVIO_FLAG_WRITE);
        cout << "avio opened !" << endl;
        res = avio_open2(&avFormatCtx->pb,obj.get_url(),AVIO_FLAG_WRITE,NULL,NULL);
        if(res < 0){
        	// AVERROR(EAGAIN);
        	cout << "avio open failed ! code: "<< res << " " << ff_Error(res) << endl;
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
	res = avformat_write_header(avFormatCtx, &opt);
	if(res < 0){
		cout << "avformat write header failed ! code: "<< res << endl;
	}
    av_dump_format(avFormatCtx, 0, obj.get_url(), 1);
    // 是否使用硬解码
	if(obj.get_use_hw()){
		hwwave_init();
	}

    // open codec
	AVDictionary * opencodec = NULL;
	av_dict_set(&opencodec, "preset", obj.get_preset(), 0);
	av_dict_set(&opencodec, "tune", obj.get_tune(), 0);
    av_dict_set(&opencodec,"profile",obj.get_profile(),0);
    // 打开编码器
	res = avcodec_open2(avCodecCtx,avCodec,&opencodec);
	if(res < 0){
		cout << "open codec failed ! " << res <<endl; 
	}
    // 给packet 分配内存
    packet = av_packet_alloc();

    // 分配内存
    frameYUV = av_frame_alloc();
    frameYUV->width = width;
    frameYUV->height = height;
    frameYUV->format = out_pix_fmt;
    av_image_alloc(frameYUV->data,frameYUV->linesize,width,height,out_pix_fmt,1);
    av_frame_get_buffer(frameYUV, 0);
    // 让其可填充数据
    res = av_frame_make_writable(frameYUV);
    if(res < 0){
		cout << "make frameYUV writeable failed ! code:" << res << endl;
	}
    // 申明转换器
    swsCtx = sws_getContext(width,height,
        in_pix_fmt,
        width,height,
        out_pix_fmt,
        SWS_BICUBIC,NULL,NULL,NULL);

	cout << "init codec finished ! [w:" << width << " h:" << height << "]" << endl;
    return 0;
}

void encode_frame(cv::Mat frame,COMMAND_PUSH_OBJ obj){
    // int64_t start_time=av_gettime();

	int res = 0;
    // 给帧打上时间戳
	frameYUV->pts =(framecount) * av_q2d(avCodecCtx->time_base);

	//一行（宽）数据的字节数 列数x3
	insize[0] = frame.cols * frame.elemSize();
    // cout << frame.cols << "x" << frame.elemSize() << "=" << frame.cols* frame.elemSize() << endl;
    // cout << frameYUV->linesize[0] << endl;
	// 将frame的数据转换成 YUV420格式
	res = sws_scale(swsCtx,&frame.data,insize,0,frame.rows,frameYUV->data,frameYUV->linesize);
    //将帧发送给编码器进行编码
	res = avcodec_send_frame(avCodecCtx,frameYUV);
	if(res!=0){
		// AVERROR(ERANGE)
		cout << "send frame to avcodec failed ! code :" << res << endl;
        return;
	}
    // 从编码器中获取packet数据
	res = avcodec_receive_packet(avCodecCtx,packet);
	if(res != 0){
		cout << "fail receive encode packet! code :" << res << endl;
        return;
	}
    // //Write PTS
    // AVRational time_base1={1,obj.get_fps()};
    // //Duration between 2 frames (us)
    // int64_t calc_duration=(double)AV_TIME_BASE/av_q2d({obj.get_fps(),1});
    // cout << "cac_duration " << calc_duration << endl;
    // //Parameters
    // packet->pts =(double) framecount * av_q2d(avCodecCtx->time_base);
    // packet->dts = packet->pts;
    // packet->duration = (double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
    // 对包数据的时间进行设置   
    // 展示时间戳设置 以time_base为单位
	// packet->pts = framecount * av_q2d(avCodecCtx->time_base) * av_rescale_q_rnd(packet->pts, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    packet->pts = av_rescale_q_rnd(framecount, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    // cout << " packet-dts " << packet->dts;
    // 解码时间
	// packet->dts = framecount * av_q2d(avCodecCtx->time_base) * av_rescale_q_rnd(packet->dts, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    packet->dts = av_rescale_q_rnd(framecount, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    // cout << " packet-pts " << packet->pts << endl;
    // 持续时间
	packet->duration = av_rescale_q_rnd(packet->duration, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    
    if(!strcmp(obj.get_protocol(),"rtsp") && !(packet->flags & AV_PKT_FLAG_KEY)){
        // 在每帧非关键帧前面添加PPS SPS头信息
        memset(cExtradata,0,extra_data_size*sizeof(uint8_t));
        memcpy(cExtradata, avCodecCtx->extradata, avCodecCtx->extradata_size);
        memcpy(cExtradata + avCodecCtx->extradata_size, packet->data, packet->size);
        packet->size += avCodecCtx->extradata_size;
        packet->data = cExtradata;
    }
	// packet->pos = -1;
    // 找到带I帧的AVPacket
    // if (packet->flags & AV_PKT_FLAG_KEY){
    //     printf("%X %X %X %X %X\n",packet->data[0],
    //         packet->data[1],
    //         packet->data[2],
    //         packet->data[3],
    //         packet->data[4]);
    //     printf("%X %X %X %X %X\n",avCodecCtx->extradata[0],
    //         avCodecCtx->extradata[1],
    //         avCodecCtx->extradata[2],
    //         avCodecCtx->extradata[3],
    //         avCodecCtx->extradata[4]);
    //         uint8_t* cExtradata = (uint8_t *)malloc((packet->size + avCodecCtx->extradata_size) * sizeof(uint8_t));
    //         memcpy(cExtradata, avCodecCtx->extradata, avCodecCtx->extradata_size);
    //         memcpy(cExtradata + avCodecCtx->extradata_size, packet->data, packet->size);//&enc_packet.data[0]
    //         packet->size += avCodecCtx->extradata_size;
    //         packet->data = cExtradata;
    //     // pps_sps_init();
    //     cout << "key frame " << framecount << endl;
    // }
    // 通过创建输出流的format 输出数据包
    framecount++;
	res = av_interleaved_write_frame(avFormatCtx, packet);
	if (res < 0){
		cout << "send packet error! code:" << res << endl;
	}
}

void encode_frame(AVFrame *frame,COMMAND_PUSH_OBJ obj){
    // int64_t start_time=av_gettime();

	int res = 0;
    // 给帧打上时间戳
	frame->pts =(framecount) * av_q2d(avCodecCtx->time_base);

	//一行（宽）数据的字节数 列数x3
    // cout << frame.cols << "x" << frame.elemSize() << "=" << frame.cols* frame.elemSize() << endl;
    // cout << frameYUV->linesize[0] << endl;
	// 将frame的数据转换成 YUV420格式
    //将帧发送给编码器进行编码
	res = avcodec_send_frame(avCodecCtx,frame);
	if(res!=0){
		// AVERROR(ERANGE)
		cout << "send frame to avcodec failed ! code :" << res << endl;
        return;
	}
    // 从编码器中获取packet数据
	res = avcodec_receive_packet(avCodecCtx,packet);
	if(res != 0){
		cout << "fail receive encode packet! code :" << res << endl;
        return;
	}
    // 对包数据的时间进行设置   
    // 展示时间戳设置 以time_base为单位
	// packet->pts = framecount * av_q2d(avCodecCtx->time_base) * av_rescale_q_rnd(packet->pts, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    packet->pts = av_rescale_q_rnd(framecount, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    // cout << " packet-dts " << packet->dts;
    // 解码时间
	// packet->dts = framecount * av_q2d(avCodecCtx->time_base) * av_rescale_q_rnd(packet->dts, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    packet->dts = av_rescale_q_rnd(framecount, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    // cout << " packet-pts " << packet->pts << endl;
    // 持续时间
	packet->duration = av_rescale_q_rnd(packet->duration, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    
    if(!strcmp(obj.get_protocol(),"rtsp") && !(packet->flags & AV_PKT_FLAG_KEY)){
        // 在每帧非关键帧前面添加PPS SPS头信息
        memset(cExtradata,0,extra_data_size*sizeof(uint8_t));
        memcpy(cExtradata, avCodecCtx->extradata, avCodecCtx->extradata_size);
        memcpy(cExtradata + avCodecCtx->extradata_size, packet->data, packet->size);
        packet->size += avCodecCtx->extradata_size;
        packet->data = cExtradata;
    }
    // 通过创建输出流的format 输出数据包
    framecount++;
	res = av_interleaved_write_frame(avFormatCtx, packet);
	if (res < 0){
		cout << "send packet error! code:" << res << endl;
	}
}

int event_process(SDL_Event *event){
    int is_exit = 0;
    switch (event->type)
		{
		case SDL_KEYDOWN:			//键盘事件
			switch (event->key.keysym.sym)
			{
			case SDLK_q:
				// std::cout << "key down q, ready to exit" << std::endl;
                is_exit = 1;
				break;
			default:
				// printf("key down 0x%x\n", event->key.keysym.sym);
				break;
			}
			break;
        }
    return is_exit;
}

void destory_(){
    cout << "释放回收资源：" << endl;
	// fclose(wf);
	if(avFormatCtx){
        // avformat_close_input(&avFormatCtx);
        avio_close(avFormatCtx->pb);
        avformat_free_context(avFormatCtx);
        avFormatCtx = 0;
        cout << "avformat_free_context(avFormatCtx)" << endl;
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
    
    if(frameYUV){
        av_frame_free(&frameYUV);
        frameYUV = 0;
        cout << "av_frame_free(frameYUV)" << endl;
    }
    
    if(avCodecCtx->hw_device_ctx){
        av_buffer_unref(&avCodecCtx->hw_device_ctx);
        cout << "av_buffer_unref(&avCodecCtx->hw_device_ctx)" << endl;
    }
    if(avCodecCtx){
        avcodec_close(avCodecCtx);
        avCodecCtx = 0;
        cout << "avcodec_close(avCodecCtx);" << endl;
    }
    if(texture){
        SDL_DestroyTexture(texture);
        cout << "SDL_DestroyTexture(texture)" << endl;
    }
    if(cExtradata){
        free(cExtradata);
        cout << "free cExtradata " << endl;
    }
    // SDL_free(&rect);
    // cout << "SDL_free(&rect)" << endl;
    // SDL_free(&event);
    // cout << "SDL_free(&event);" << endl;
    if(ren){
        SDL_DestroyRenderer(ren);
        cout << "SDL_DestroyRenderer(ren)" << endl;
    }
    if(win){
        SDL_DestroyWindow(win);
        cout << "SDL_DestroyWindow(win)" << endl;
    }
    SDL_Quit();
}

void cv_test(){
    VideoCapture videoCap;
    videoCap.set(CAP_PROP_FRAME_WIDTH, 640);//宽度
    videoCap.set(CAP_PROP_FRAME_HEIGHT, 480);//高度
    videoCap.set(CAP_PROP_FPS, 30);//帧率 帧/秒
    videoCap.set(CAP_PROP_FOURCC,VideoWriter::fourcc('M','J','P','G')); // 捕获格式
    videoCap.open(0);
    if(!videoCap.isOpened()){
        cout << "camera not open !" << endl;
        return;
    }
    Mat frame;
	while (videoCap.read(frame))
	{
        Size size  = frame.size();
        width = size.width;
        height = size.height;
        // cout << width << " " << height << endl;
		imshow("读取视频", frame);
        if((waitKey(1) & 0xff) == 27){
            break;
        }
    }
    videoCap.release();
    destroyAllWindows();
}

// void start_remote(){
//     thread remote(control);    
//     remote.detach();
// }

/**
 * X11捕获当前屏幕
*/
void ImageFromDisplay(std::vector<uint8_t>& Pixels, int& Width, int& Height, int& BitsPerPixel){
    Display* display = XOpenDisplay(nullptr);
    Window root = DefaultRootWindow(display);

    XWindowAttributes attributes = {0};
    XGetWindowAttributes(display, root, &attributes);

    Width = attributes.width;
    Height = attributes.height;

    XImage* img = XGetImage(display, root, 0, 0 , Width, Height, AllPlanes, ZPixmap);
    BitsPerPixel = img->bits_per_pixel;
    Pixels.resize(Width * Height * 4);

    memcpy(&Pixels[0], img->data, Pixels.size());

    XDestroyImage(img);
    XCloseDisplay(display);
}

/**
 * 从外设中捕获图像并发送
*/
int ImageFromOther(COMMAND_PUSH_OBJ &obj){
    int res = 0;
    VideoCapture videoCap;
    videoCap.set(CAP_PROP_FRAME_WIDTH, 640);//宽度
    videoCap.set(CAP_PROP_FRAME_HEIGHT, 480);//高度
    videoCap.set(CAP_PROP_FPS, 30);//帧率 帧/秒
    videoCap.set(CAP_PROP_FOURCC,VideoWriter::fourcc('M','J','P','G')); // 捕获格式
    videoCap.open(41); // v4l2-ctl --list-device 查看驱动 41 摄像头 40从hdmi输入
    if(!videoCap.isOpened()){
        cout << "camera not open !" << endl;
        return -1;
    }
    // start_remote();
	int is_init_encoder = 0;
    Mat frame;
	while (videoCap.read(frame))
	{
		// imshow("读取视频", frame);
        // if((waitKey(1) & 0xff) == 27){
        //     break;
        // }
		if(!is_init_encoder){
            Size size = frame.size();
            width = size.width;
            height = size.height;
			res = encoder_init(obj); //初始化解码器
            if(res < 0){
                cout << "encoder init failed !" << endl;
                return -1;
            }
			res = sdl_init_process(); //初始化显示窗口
			if(res<0){
				return -1;
			}
			cout << "the win address :"<< win << endl;
			cout << "the render address :"<< ren << endl;
			cout << "the texture address :"<< texture << endl;
			is_init_encoder = 1;
		}
		encode_frame(frame,obj);
		// 设置SDL更新矩阵 起始点(x,y)，宽高(w,h)
		rect.x = 0;
		rect.y = 0;
		rect.w = width;
		rect.h = height;
		SDL_UpdateYUVTexture(texture,&rect,
			frameYUV->data[0],frameYUV->linesize[0],
			frameYUV->data[1],frameYUV->linesize[1],
			frameYUV->data[2],frameYUV->linesize[2]);

		// 清空渲染器
		SDL_RenderClear(ren);
		// 将纹理拷贝至渲染器
		SDL_RenderCopy(ren, texture,NULL, &rect);
		SDL_RenderPresent(ren);

		// SDL事件处理
        SDL_PollEvent(&event);
        // packet清空
		av_packet_unref(packet);
        int ext = event_process(&event);
        if(ext){
            break;
        }
	}
    videoCap.release();
    destory_();
}

int ImageFromX11(COMMAND_PUSH_OBJ &obj){
    int Width = 0;
    int Height = 0;
    int Bpp = 0;
    int res = 0;
    std::vector<std::uint8_t> Pixels;

    int is_init_encoder = 0;
    Mat frame;
    while (1){
        ImageFromDisplay(Pixels, Width, Height, Bpp);
        frame = Mat(Height, Width, Bpp > 24 ? CV_8UC4 : CV_8UC3, &Pixels[0]); //Mat(Size(Height, Width), Bpp > 24 ? CV_8UC4 : CV_8UC3, &Pixels[0]); 
        if(!is_init_encoder){
            Size size = frame.size();
            width = size.width;
            height = size.height;
			res = encoder_init(obj); //初始化解码器
            if(res < 0){
                cout << "encoder init failed !" << endl;
                return -1;
            }
			is_init_encoder = 1;
		}
		encode_frame(frame,obj);
        
        // packet清空
        if(packet){
            av_packet_unref(packet);
        }
        imshow("test",frame);
        if((waitKey(1) & 0xff) == 27){
            break;
        }
	}
    destory_();
}

int main(int argc,char * argv[]){
    COMMAND_PUSH_OBJ obj = process_command(argc,argv);
    // start_remote();
    // ImageFromOther(obj);
    ImageFromX11(obj);
    return 0;
}