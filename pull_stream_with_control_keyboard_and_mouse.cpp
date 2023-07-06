#include<iostream>
#include<thread>
#include<string.h>

#include"ffmpeg_head.h"

#include<SDL2/SDL.h>
#include<SDL2/SDL_thread.h>
#include<SDL2/SDL_error.h>
#include<SDL2/SDL_ttf.h>
#include "command.h"
// #include "Remote_Client/RemoteKeyboardMouse_Client.h"
using namespace std;

COMMAND_STR process_command(int argc,char * argv[]){
    COMMAND_STR res = COMMAND_STR(-1);
    if(argc >= 2){
        for(int i=1;i<argc;i++){
            std::string flag = argv[i];
            if(flag == "-l"){
                // 读取本地文件
                res.setType(0);
                res.setUrl(argv[i+1]);
            }
            if(flag == "-n"){
                //读取网路流
                // return COMMAND_STR(1,argv[2]);
                res.setType(1);
                res.setUrl(argv[i+1]);
                // res.setUrl("rtsp://192.168.1.222:8554/media");
            }
            if(flag == "-hd"){
                res.setUseHdwave(1);
            }
            if(flag == "udp" || flag == "tcp"){
                res.setTransport(flag.c_str());
            }
            if(flag == "-s"){
                res.setVideo_size(argv[i+1]);
            }
        }
    }
    return res;
}

int width = 0;
int height = 0;
SDL_Window *win = NULL; //SDL窗口
SDL_Renderer *ren = NULL; // 渲染器
SDL_Texture *texture = NULL; // 纹理
SDL_Surface * surface = NULL; //表面
SDL_Rect rect; // 活动矩形
SDL_Event event; // 事件

TTF_Font * font = NULL;
SDL_Color White = {0, 0, 0};

static enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_DRM_PRIME; //硬件支持的像素格式
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }
    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

void hardwave_init(AVCodecContext *codecCtx,const AVCodec * codec){
    //设置硬解码器
    AVHWDeviceType deviceType = AV_HWDEVICE_TYPE_DRM; // 根据架构设置硬解码类型
    // AVHWDeviceType deviceType = av_hwdevice_find_type_by_name("drm");
    // cout << "drm find device type " << deviceType << endl;
    // if (deviceType == AV_HWDEVICE_TYPE_NONE) {
    //     fprintf(stderr, "Device type drm is not supported.\n");
    //     fprintf(stderr, "Available device types:");
    //     while((deviceType = av_hwdevice_iterate_types(deviceType)) != AV_HWDEVICE_TYPE_NONE)
    //         fprintf(stderr, " %s", av_hwdevice_get_type_name(deviceType));
    //     fprintf(stderr, "\n");
    //     return;
    // }
    // AVHWDeviceType type = av_hwdevice_find_type_by_name("opencl");
    // cout << type << endl;
    // for (int i = 0;; i++) {
    //     const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
    //     if (!config) {
    //         fprintf(stderr, "Decoder %s does not support device type %s.\n",
    //                 codec->name, av_hwdevice_get_type_name(deviceType));
    //         return;
    //     }
    //     if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
    //         config->device_type == deviceType) {
    //         hw_pix_fmt = config->pix_fmt;
    //         cout << "hw_Pix_fmt is " << config->pix_fmt << endl;
    //         break;
    //     }
    // }
    // codecCtx->get_format = get_hw_format;
    
    // 根据硬解码类型创建硬解码器
    // AVBufferRef * bufferRef = NULL;
    AVBufferRef * bufferRef = av_hwdevice_ctx_alloc(deviceType);
    cout << "alloc hwdevice success address : " << bufferRef << endl;
    // 初始化硬解码器
    // int ret = av_hwdevice_ctx_create(&bufferRef,deviceType,NULL,NULL,0);
    int ret = av_hwdevice_ctx_init(bufferRef);
    if(ret != 0){
        cout << "init hardwave device context failed!" << endl;
    }
    // 将硬解码器关联到解码器上
    codecCtx->hw_device_ctx = av_buffer_ref(bufferRef);
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
				std::cout << "key down q, ready to exit" << std::endl;
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

int main(int argc,char *argv[]){
    COMMAND_STR  com = process_command(argc,argv);
    com.print_command_str();
    if(com.getType() < 0){
        cout << "command process failed!" << endl;
        return -1;
    }
    
    AVFormatContext *avFormatCtx = NULL; // 编解码上下文
    AVDictionary *avDict = NULL; // 键值对
    int ret = 0; 
    if(com.getType() == 1){
        avformat_network_init();
        av_dict_set(&avDict,"rtsp_transport",com.getTransport(),0);
        // av_dict_set(&avDict, "buffer_size", "1024000", 0); //设置缓存大小,1080p可将值跳到最大
        // av_dict_set(&avDict, "rtsp_transport", "tcp", 0); //以tcp的方式打开,
        // av_dict_set(&avDict, "stimeout", "5000000", 0); //设置超时断开链接时间，单位us
        // av_dict_set(&avDict, "max_delay", "500000", 0); //设置最大时延
        // av_dict_set(&avDict,"max_delay","550",0);
        av_dict_set(&avDict,"fflags","nobuffer",0);
    }
    // 打开文件
    if(strcmp(com.getVideo_size(),"0x0")){
        av_dict_set(&avDict,"video_size",com.getVideo_size(),0); //如果是yuv文件需要提前设置文件尺寸，否则会报错-22
    }
    ret = avformat_open_input(&avFormatCtx,com.getUrl(),NULL,&avDict);
    if(ret != 0){
        cout << "open input file or network failed ! code:"<< ret << endl;
        return -1;
    }

    avFormatCtx->probesize = 100 * 1024;
	avFormatCtx->max_analyze_duration = 5*AV_TIME_BASE;
    // 获取流信息
    ret = avformat_find_stream_info(avFormatCtx,avDict == NULL?NULL:&avDict);
    if(ret < 0){
        cout << "get stream info failed ! code :"<< ret << endl;
        return -1;
    }

    auto all_seconds = avFormatCtx->duration/1000000;
    printf("the video time [%d:%d]\n",(int)all_seconds/60,(int)all_seconds%60);

    // 查找需要的流索引
    int videoStreamIndex = -1,audioStreamIndex = -1;
    for(int i=0;i<avFormatCtx->nb_streams;i++){
        if(avFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            videoStreamIndex = i;
        }
        if(avFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audioStreamIndex = i;
        }
    }
    if(videoStreamIndex == -1){
        cout << "do not found video strem !" << endl;
    }
    if(audioStreamIndex == -1){
        cout << "do not found audio strem !" << endl;
    }
    cout << "found video strem index "<< videoStreamIndex << endl;
    cout << "found audio strem index "<< audioStreamIndex << endl;
    //查找流的编码器
    AVCodecParameters *codecPar = avFormatCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec *codec = NULL;

    // const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if(com.getUseHdwave()){
        cout << "find decoder_by_name h264_rkmpp!" << endl;
        codec = avcodec_find_decoder_by_name("h264_rkmpp");
    }else{
        cout << "codec_id is " << codecPar->codec_id << endl;
        codec = avcodec_find_decoder_by_name("h264");
        // codec = avcodec_find_decoder(codecPar->codec_id);
    }
    cout << "find decoder by name address :" <<  codec << endl;
    if(!codec){
        cout << "do not find decoder of the stream !" << endl;
        return -1;
    }
    // 根据编码器找到上下文
    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if(!codecCtx){
        cout << "do not find codec Context !" << endl;
        return -1;
    }

    //拷贝解码器的参数到解码器
    ret = avcodec_parameters_to_context(codecCtx,codecPar);
    if(ret < 0){
        cout << "write parameters into context failed !" << endl;
        return -1;
    }
    // width = codecPar->width;
    // height = codecPar->height;
    width = codecCtx->width;
    height = codecCtx->height;
    cout << "the codec id ["<< codec->id <<"] name [" << codecCtx->codec->name << "],[w:"<<width << ",h:" << height << "]"<< endl;
    cout << "the pic format " << codecCtx->pix_fmt << endl;
    
    if(com.getUseHdwave()){
        hardwave_init(codecCtx,codec);
        cout << "init hardwave device context! address : "<< codecCtx->hw_device_ctx << endl;
    }

    // avcodec_get_hw_config(codec,);
    AVDictionary *avDict_open = NULL; // 键值对
    // av_dict_set(&avDict_open,"hwaccle","drm",0);
    // av_dict_set(&avDict_open,"hwaccle_device","/dev/dri/card0",0);
    //打开解码器
    ret = avcodec_open2(codecCtx,codec,&avDict_open);
    if(ret != 0){
        cout << "open decoder failed!" << endl;
        return -1;
    }
    
    /**
     * 分别创建 原始帧，转换帧，以及硬解码帧
     * 使用流程是将原始帧拷贝到硬解码帧中，解码器解码
     * 然后将硬解码器的拷出，并进行格式转换
    */
    AVFrame *frame,*frameYUV,*frameHW;
    // 分配内存空间，此处并不会分配buffer的空间
    frame = av_frame_alloc();
    frameYUV = av_frame_alloc();
    frameHW = av_frame_alloc();
    // 分配buffer内存空间
    
    int framesize = av_image_alloc(frame->data,frame->linesize,width,height,
        com.getUseHdwave() ? AV_PIX_FMT_NV12:AV_PIX_FMT_YUV420P,1);
    int frameHWsize = av_image_alloc(frameHW->data,frameHW->linesize,width,height,
        com.getUseHdwave() ? AV_PIX_FMT_NV12:AV_PIX_FMT_YUV420P,1);
    //此处我将原始帧转换成YUV格式的帧来显示，因此分配的内存空间根据YUV格式来计算每帧的大小，并且无缩放
    int frameYUVsize = av_image_alloc(frameYUV->data,frameYUV->linesize,width,height,AV_PIX_FMT_YUV420P,1);

    printf("alloc origin frame size: [%d]\n",framesize);
    printf("alloc hardwave frame size: [%d]\n",frameHWsize);
    printf("alloc YUV frame size: [%d]\n",frameYUVsize);

    // 创建转换器 将帧转换成我们想要的格式 此处是YUV，同时可以添加滤镜等Filter
    SwsContext * swsCtx = sws_getContext(width,height,
        com.getUseHdwave() ? AV_PIX_FMT_NV12:AV_PIX_FMT_YUV420P,
        width,height,
        AV_PIX_FMT_YUV420P,
        SWS_BICUBIC,NULL,NULL,NULL);

    ret = sdl_init_process(); //初始化显示窗口
    if(ret<0){
        return -1;
    }
    cout << "the win address :"<< win << endl;
    cout << "the render address :"<< ren << endl;
    cout << "the texture address :"<< texture << endl;
    
    // start_remote();

    // 创建数据包 用于接受文件或者网络的数据
    AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    // 帧和包的读取返回标记
    int frameFinish = -1,packetFinish = -1;
    clock_t start,end;
    SDL_Rect sdl_rect1 = { 0, 0, 400, 40 }; // 字幕坐标

    int get_key_frame = 0;
    // 读取帧数据送入packet
    while (start = clock(),av_read_frame(avFormatCtx,packet) >= 0){
        // cout << "packet_stream_index" << packet->stream_index <<endl;
        // if(get_key_frame == 0 && !(packet->flags & AV_PKT_FLAG_KEY)){
        //     cout << " drop packet without key frame!"<< endl;
        //     av_packet_unref(packet);
        //     continue;
        // }
        // get_key_frame = 1;
        if(packet->stream_index == videoStreamIndex){
            // 将包数据送入编解码器
            packetFinish = avcodec_send_packet(codecCtx,packet);
            if(packetFinish < 0){
                continue;
            }
            // printf("%x,%x,%x,%x,%x\n",packet->data[0],packet->data[1],packet->data[2],packet->data[3],packet->data[4]);
            // cout << packet->data[22] << endl;
            // goto END;
            // 从编解码器 读取解码后的帧
            frameFinish = avcodec_receive_frame(codecCtx,frame);
            if(frameFinish < 0){
                continue;
            }
            cout << "frame format :" << frame->format <<endl;
            if(hw_pix_fmt == frame->format){
                // 该数据格式可以被硬解码，将数据从硬件内存拷贝至内存
                // cout << "transfer data from hdwave to mem" << endl;
                av_hwframe_transfer_data(frameHW, frame, 0);
                sws_scale(swsCtx,(const uint8_t* const*)frameHW->data,frameHW->linesize,
                    0,height,frameYUV->data,frameYUV->linesize);
            }else{
                // 进行图片格式转换
                sws_scale(swsCtx,(const uint8_t* const*)frame->data,frame->linesize,
                    0,height,frameYUV->data,frameYUV->linesize);
            }
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
            // 释放包数据
            av_packet_unref(packet);
        }        
        // SDL事件处理
        SDL_PollEvent(&event);
        
        int ext = event_process(&event);
        if(ext){
            break;
        }
        end = clock();
        // double use_time = round(double(end-start)/CLOCKS_PER_SEC * 1000) /100 *100;
        // string timetemp = "time = " + to_string(use_time) + "ms ,fps :" + to_string((int)round(1000/use_time));
        // cout<< timetemp <<endl;
        // surface = TTF_RenderText_Blended(font, "Hello, World!", White);
        // surface = TTF_RenderText_Solid(font,timetemp.c_str(), White); 
        // cout << surface. << endl;
        // SDL_Texture *tx = SDL_CreateTextureFromSurface(ren, surface);
        // fprintf(stderr, "%s\n", SDL_GetError());
        // SDL_RenderCopy(ren, texture, NULL, NULL);
        // SDL_RenderCopy(ren, tx,NULL, &sdl_rect1);
        // SDL_FreeSurface(surface);
        // SDL_DestroyTexture(tx);
        // SDL_RenderPresent(ren);
    }
END:
    cout << "释放回收资源：" << endl;
    if(packet){
        av_packet_unref(packet);
        packet = NULL;
        cout << "av_free_packet(pAVPacket)" << endl;
    }
    if(swsCtx)
    {
        sws_freeContext(swsCtx);
        swsCtx = 0;
        cout << "sws_freeContext(swsCtx)" << endl;;
    }
    if(frame){
        av_frame_free(&frame);
        frame = 0;
        cout << "av_frame_free(frame)" << endl;
    }
    if(frameHW){
        av_frame_free(&frameHW);
        frameHW = 0;
        cout << "av_frame_free(frameHW)" << endl;
    }
    
    if(codecCtx->hw_device_ctx){
        av_buffer_unref(&codecCtx->hw_device_ctx);
        cout << "av_buffer_unref(&bufferRef)" << endl;
    }
    if(codecCtx)
    {
        avcodec_close(codecCtx);
        codecCtx = 0;
        cout << "avcodec_close(codecCtx);" << endl;
    }
    if(avFormatCtx)
    {
        avformat_close_input(&avFormatCtx);
        avformat_free_context(avFormatCtx);
        avFormatCtx = 0;
        cout << "avformat_free_context(avFormatCtx)" << endl;
    }
    if(texture){
        SDL_DestroyTexture(texture);
        cout << "SDL_DestroyTexture(texture)" << endl;
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
    return 0;

}
