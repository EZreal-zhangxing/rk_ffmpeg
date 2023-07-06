#include<iostream>
#include<thread>
#include<opencv2/opencv.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include<opencv2/highgui/highgui.hpp>

#include "ffmpeg_head.h"

#include<SDL2/SDL.h>
#include<SDL2/SDL_thread.h>
#include<SDL2/SDL_error.h>

#include "command_push.h"
// #include "Remote_Server/RemoteKeyboardMouse_Server.h"

using namespace std;
using namespace cv;

extern int width,height;
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
        if(flag == "-cpn"){
            res.set_capture_name(argv[i+1]);
        }
        if(flag == "-w"){
            res.set_width(argv[i+1]);
        }
        if(flag == "-h"){
            res.set_height(argv[i+1]);
        }
    }
    res.print_command_str();
    return res;
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



// void start_remote(){
//     thread remote(control);    
//     remote.detach();
// }

/**
 * 直接发送截屏采样的包
*/
extern AVFormatContext *inputFmtCtx;
extern AVFrame * reviceFrame;
extern AVFrame* frame; //YUV420P target frame
extern AVPacket *packet;
extern SwsContext *swsCtx;
extern int inputVideoStream;
extern AVCodecContext *inputCodeCtx;

extern AVFormatContext *outputFmtCtx; 
extern AVCodecContext *outputCodecCtx; // 解码器上下文
extern AVStream * stream; // 输出流


/**
 * 输出头信息 缓存区
*/
long extra_data_size = 10000000;
uint8_t* cExtradata = NULL;

void destroy_(){
    cout << "free sdl resource......" << endl;
	// fclose(wf);
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


void encode_frame(AVFrame *frame,COMMAND_PUSH_OBJ obj){
    // int64_t start_time=av_gettime();

	int res = 0;
    // 给帧打上时间戳
	frame->pts =(framecount) * av_q2d(outputCodecCtx->time_base);

	//一行（宽）数据的字节数 列数x3
    // cout << frame.cols << "x" << frame.elemSize() << "=" << frame.cols* frame.elemSize() << endl;
    // cout << frameYUV->linesize[0] << endl;
	// 将frame的数据转换成 YUV420格式
    //将帧发送给编码器进行编码
    cout << "encoder frame to " << frame->format << endl;
	res = avcodec_send_frame(outputCodecCtx,frame);
	if(res!=0){
		// AVERROR(ERANGE)
		cout << "send frame to avcodec failed ! code :" << res << endl;
        return;
	}
    // 从编码器中获取packet数据
	res = avcodec_receive_packet(outputCodecCtx,packet);
	if(res != 0){
		cout << "fail receive encode packet! code :" << res << endl;
        return;
	}

    // 对包数据的时间进行设置   
    // 展示时间戳设置 以time_base为单位
	// packet->pts = framecount * av_q2d(avCodecCtx->time_base) * av_rescale_q_rnd(packet->pts, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    packet->pts = av_rescale_q_rnd(framecount, inputCodeCtx->time_base, outputCodecCtx->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    // cout << " packet-dts " << packet->dts;
    // 解码时间
	// packet->dts = framecount * av_q2d(avCodecCtx->time_base) * av_rescale_q_rnd(packet->dts, avCodecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    packet->dts = av_rescale_q_rnd(framecount, inputCodeCtx->time_base, outputCodecCtx->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    // cout << " packet-pts " << packet->pts << endl;
    // 持续时间
	packet->duration = av_rescale_q_rnd(packet->duration, inputCodeCtx->time_base, outputCodecCtx->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    
    if(!strcmp(obj.get_protocol(),"rtsp") && !(packet->flags & AV_PKT_FLAG_KEY)){
        // 在每帧非关键帧前面添加PPS SPS头信息
        memset(cExtradata,0,extra_data_size*sizeof(uint8_t));
        memcpy(cExtradata, outputCodecCtx->extradata, outputCodecCtx->extradata_size);
        memcpy(cExtradata + outputCodecCtx->extradata_size, packet->data, packet->size);
        packet->size += outputCodecCtx->extradata_size;
        packet->data = cExtradata;
    }
    // 通过创建输出流的format 输出数据包
    framecount++;
	res = av_interleaved_write_frame(outputFmtCtx, packet);
	if (res < 0){
		cout << "send packet error! code:" << res << endl;
	}
}

void send_packet(){
    framecount ++ ;
    packet->pts = av_rescale_q_rnd(framecount, inputCodeCtx->time_base, outputCodecCtx->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    packet->dts = av_rescale_q_rnd(framecount, inputCodeCtx->time_base, outputCodecCtx->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
	packet->duration = av_rescale_q_rnd(packet->duration, inputCodeCtx->time_base, outputCodecCtx->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    int res = av_interleaved_write_frame(outputFmtCtx, packet);
	if (res < 0){
		cout << "send packet error! code:" << res << endl;
	}
}

int flush_sdl(){
    // 设置SDL更新矩阵 起始点(x,y)，宽高(w,h)
    rect.x = 0;
    rect.y = 0;
    rect.w = width;
    rect.h = height;
    SDL_UpdateYUVTexture(texture,&rect,
        frame->data[0],frame->linesize[0],
        frame->data[1],frame->linesize[1],
        frame->data[2],frame->linesize[2]);

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
        return -1;
    }
    return 0;
}

int ImageFromFfmpeg(COMMAND_PUSH_OBJ &obj){
    if(!strcmp(obj.get_protocol(),"rtsp")){
        // 初始化包空间，进行PPS SPS包头填充
        cExtradata = (uint8_t *)malloc((extra_data_size) * sizeof(uint8_t));
    }
    int res = 0;
    while (av_read_frame(inputFmtCtx, packet) >= 0)
	{
		if (packet->stream_index == inputVideoStream)
		{
			res = avcodec_send_packet(inputCodeCtx, packet);
			if (res < 0){
				printf("Decode error. %d \n",res);
				return -1;
			}
            res = avcodec_receive_frame(inputCodeCtx, reviceFrame);
            // BGRA -> YUV420p
            sws_scale(swsCtx,(const uint8_t* const*)reviceFrame->data,reviceFrame->linesize,
                    0,height,frame->data,frame->linesize);
			if (res < 0){
                cout << "decode failed! " << res << endl;
            }
            if(flush_sdl() < 0){
                break;
            }
            encode_frame(frame,obj);
        }
        av_packet_unref(packet);
    }
    destroy_();
    destroy_ffmpeg();
    return 0;
}

int main(int argc,char * argv[]){
    COMMAND_PUSH_OBJ obj = process_command(argc,argv);
    // start_remote();
    int res = 0;
    res = sdl_init_process();
    if(res < 0){
        cout << "init sdl failed!" << endl;
        return res;
    }
    res = init_ffmpeg_capture(obj); //初始化解码器
    if(res < 0){
        cout << "init capture failed!" << endl;
        return res;
    }
    res = init_ffmpeg_push(obj);
    if(res < 0){
        cout << "init push failed!" << endl;
        return res;
    }
    ImageFromFfmpeg(obj);
    return 0;
}