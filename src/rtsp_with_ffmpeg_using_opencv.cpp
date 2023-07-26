/**
 * OpenCv捕获数据，通过FFmpeg的编码器将数据发出
*/
#include "command_mpp.h"
#include "ffmpeg_with_mpp.h"
#include<opencv2/opencv.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include<opencv2/highgui/highgui.hpp>

#include <rockchip/mpp_buffer.h>
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_meta.h>

#define MPP_ALIGN(x, a)         (((x)+(a)-1)&~((a)-1))
using namespace std;
using namespace cv;

unsigned int framecount = 0;
unsigned int width = 0,height = 0;
/**
 * 分别对应水平补齐后的步长和垂直补齐后的步长
 * RK3588 读取图片时是16字节对齐，因此需要对图片补齐方能处理
*/
unsigned int hor_stride = 0,ver_stride = 0; 

unsigned int yuv_width = 0,yuv_height = 0;
unsigned int yuv_hor_stride = 0,yuv_ver_stride = 0; 

/**
 * 1920x1080x3 的RGB图片补齐后
 * hor_stride 1920,ver_stride = 1088
 * 对于YUV图像，图像会下采样成1920x1080x3/2x1(Channel) 
 * yuv_hor_stride 1920,yuv_ver_stride = 1632
 * 这两个图片的大小从
 * 1920x1080x3(rgb) --> 1920x1080x3/2(yuv)
 * wxhxc:
 * 1920x1080x3(rgb) --> 1920x1620x1(yuv)
 * 同时由于补齐作用 YUV原始图像需要变成:
 * 1920x1620 --> 1920x1632
 * YUV图像分量的分布为：wxh的亮度Y，w/2 x h/2的U，w/2 x h/2的V
 * -------------w(1920)----------
 * |                            |
 * |                            |
 * |                            |
 * |             Y              h(1080)
 * |                            |
 * |                            |
 * |                            |
 * ------------------------------
 * |                            |
 * |             gap            | 8
 * ------------------------------
 * |                            |
 * |              U             h/2(540)
 * |                            |
 * ------------------------------
 * |             gap            | 4
 * ------------------------------
 * |                            |
 * |              V             h/2(540)
 * |                            |
 * ------------------------------
 * |             gap            | 4
 * ------------------------------
 * 
*/

unsigned int image_size = 0;

/*********************FFMPEG_START*/
const AVCodec * codec;
AVCodecContext *codecCtx;
AVFormatContext * formatCtx;
AVStream * stream;
AVHWDeviceType type = AV_HWDEVICE_TYPE_DRM;
AVBufferRef *hwdevice;
AVBufferRef *hwframe;
AVHWFramesContext * hwframeCtx;

AVFrame *frame; // 封装DRM的帧
AVPacket * packet; // 发送的包

long extra_data_size = 10000000;
uint8_t* cExtradata = NULL;

AVPixelFormat hd_pix = AV_PIX_FMT_DRM_PRIME;
AVPixelFormat sw_pix = AV_PIX_FMT_YUV420P;
/*********************FFMPEG_END*/
/**********************MPP_START*/
MppBufferGroup group;
MppBufferInfo info;
MppBuffer buffer;
MppBuffer commitBuffer;
MppFrame mppframe;
MppPacket mppPacket;
typedef struct {
    MppFrame frame;
    AVBufferRef *decoder_ref;
} RKMPPFrameContext;
/************************MPP_END*/

void rkmpp_release_frame(void *opaque, uint8_t *data){
    AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *)data;
    AVBufferRef *framecontextref = (AVBufferRef *)opaque;
    RKMPPFrameContext *framecontext = (RKMPPFrameContext *)framecontextref->data;

    mpp_frame_deinit(&framecontext->frame);
    av_buffer_unref(&framecontext->decoder_ref);
    av_buffer_unref(&framecontextref);

    av_free(desc);
}

int init_encoder(Command & obj){
    int res = 0;
    avformat_network_init();

    codec = avcodec_find_encoder_by_name("h264_rkmpp");
    if(!codec){
        print_error(__LINE__,-1,"can not find h264_rkmpp encoder!");
        return -1;
    }
    // 创建编码器上下文
    codecCtx = avcodec_alloc_context3(codec);
    if(!codecCtx){
        print_error(__LINE__,-1,"can not create codec Context of h264_rkmpp!");
        return -1;
    }
    
    res = av_hwdevice_ctx_create(&hwdevice,type,"/dev/dri/card0",0,0);
    if(res < 0){
        print_error(__LINE__,res,"create hdwave device context failed!");
        return res;
    }

    codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    codecCtx->codec_id = codec->id;
    codecCtx->codec = codec;
    codecCtx->bit_rate = 1024*1024*8;
    codecCtx->codec_type = AVMEDIA_TYPE_VIDEO; //解码类型
    codecCtx->width = width;  // 宽
    codecCtx->height = height; // 高
    codecCtx->channels = 0;
    codecCtx->time_base = (AVRational){1,obj.get_fps()}; // 每帧的时间
    codecCtx->framerate = (AVRational){obj.get_fps(),1}; // 帧率
    codecCtx->pix_fmt = hd_pix; //AV_PIX_FMT_DRM_PRIME
    codecCtx->gop_size = 12; // 每组多少帧
    codecCtx->max_b_frames = 0; // b帧最大间隔

    hwframe = av_hwframe_ctx_alloc(hwdevice);
    if(!hwframe){
        print_error(__LINE__,-1,"create hdwave frame context failed!");
        return -1;
    }
    hwframeCtx = (AVHWFramesContext *)(hwframe->data);
    hwframeCtx->format    = hd_pix;
    hwframeCtx->sw_format = sw_pix;
    hwframeCtx->width     = width;
    hwframeCtx->height    = height;
    /**
     *  帧池，会预分配，后面创建与硬件关联的帧时，会从该池后面获取相应的帧
     *  initial_pool_size与pool 至少要有一个不为空
    */
    // hwframeCtx->initial_pool_size = 20;
    hwframeCtx->pool = av_buffer_pool_init(20*sizeof(AVFrame),NULL);
    res = av_hwframe_ctx_init(hwframe);
    if(res < 0){
        print_error(__LINE__,res,"init hd frame context failed!");
        return res;
    }
    codecCtx->hw_frames_ctx = hwframe;
    codecCtx->hw_device_ctx = hwdevice;
    
    if(!strcmp(obj.get_protocol(),"rtsp")){
        // rtsp协议
        res = avformat_alloc_output_context2(&formatCtx,NULL,"rtsp",obj.get_url());
    }else{
        // rtmp协议
        res = avformat_alloc_output_context2(&formatCtx,NULL,"flv",obj.get_url());
    }
    if(res < 0){
        print_error(__LINE__,res,"create output context failed!");
        return res;
    }

    stream = avformat_new_stream(formatCtx,codec);
    if(!stream){
        print_error(__LINE__,res,"create stream failed!");
        return -1;
    }
    stream->time_base = (AVRational){1,obj.get_fps()}; // 设置帧率
    stream->id = formatCtx->nb_streams - 1; // 设置流的索引

    res = avcodec_parameters_from_context(stream->codecpar,codecCtx);
    if(res < 0){
        print_error(__LINE__,res,"copy parameters to stream failed!");
        return -1;
    }

    // 打开输出IO RTSP不需要打开，RTMP需要打开
    if(!strcmp(obj.get_protocol(),"rtmp")){
        res = avio_open2(&formatCtx->pb,obj.get_url(),AVIO_FLAG_WRITE,NULL,NULL);
        if(res < 0){
            print_error(__LINE__,res,"avio open failed !");
            return -1;
        }
    }
    // 写入头信息   
    AVDictionary *opt = NULL;
    if(!strcmp(obj.get_protocol(),"rtsp")){
        av_dict_set(&opt, "rtsp_transport",obj.get_trans_protocol(),0);
        av_dict_set(&opt, "muxdelay", "0.1", 0);
    }
	res = avformat_write_header(formatCtx, &opt);
	if(res < 0){
        print_error(__LINE__,res,"avformat write header failed ! ");
        return -1;
	}
    av_dump_format(formatCtx, 0, obj.get_url(), 1);

    // open codec
	AVDictionary * opencodec = NULL;
	av_dict_set(&opencodec, "preset", obj.get_preset(), 0);
	av_dict_set(&opencodec, "tune", obj.get_tune(), 0);
    av_dict_set(&opencodec,"profile",obj.get_profile(),0);
    // 打开编码器
	res = avcodec_open2(codecCtx,codec,&opencodec);
	if(res < 0){
        print_error(__LINE__,res,"open codec failed ! ");
        return -1;
	}
    return res;
}

MPP_RET init_mpp(){
    MPP_RET res = MPP_OK;
    res = mpp_buffer_group_get_external(&group,MPP_BUFFER_TYPE_DRM);
    return res;
}

int init_data(Command & obj){
    int res = 0;
    // 给packet 分配内存
    packet = av_packet_alloc();

    if(!strcmp(obj.get_protocol(),"rtsp")){
        // 初始化包空间，进行PPS SPS包头填充
        cExtradata = (uint8_t *)malloc((extra_data_size) * sizeof(uint8_t));
    }

    frame = av_frame_alloc();//分配空间
    frame->width = width;
    frame->height = height;
    frame->format = sw_pix;
    res = av_hwframe_get_buffer(codecCtx->hw_frames_ctx,frame,0); //与硬件帧创建关联

    if(!frame->hw_frames_ctx){
        print_error(__LINE__,res," connect frame to hw frames ctx failed!");
    }
    return res;
}

MPP_RET read_frame(cv::Mat & cvframe,void * ptr){
    RK_U32 row = 0;
    RK_U32 read_size = 0;
    RK_U8 *buf_y = (RK_U8 *)ptr;
    RK_U8 *buf_u = buf_y + hor_stride * ver_stride; // NOTE: diff from gen_yuv_image
    RK_U8 *buf_v = buf_u + hor_stride * ver_stride / 4; // NOTE: diff from gen_yuv_image
    // buf_y = cvframe.data;

    for (row = 0; row < height; row++) {
        memcpy(buf_y + row * hor_stride,cvframe.datastart + read_size,width);
        read_size += width;
    }

    for (row = 0; row < height / 2; row++) {
        memcpy(buf_u + row * hor_stride/2,cvframe.datastart + read_size ,width/2);
        read_size += width/2;
    }

    for (row = 0; row < height / 2; row++) {
        memcpy(buf_v + row * hor_stride/2,cvframe.datastart + read_size ,width/2);
        read_size += width/2;
    }
    return MPP_OK;
}
/**
 * 将opencv的帧转换成Drm数据帧
*/
MPP_RET convert_cvframe_to_drm(cv::Mat &cvframe,AVFrame *& avframe){
    MPP_RET res = MPP_OK;
    res = mpp_buffer_get(NULL,&buffer,image_size);
    if(res != MPP_OK){
        return res;
    }
    info.fd = mpp_buffer_get_fd(buffer);
    info.ptr = mpp_buffer_get_ptr(buffer);
    info.index = framecount;
    info.size = image_size;
    info.type = MPP_BUFFER_TYPE_DRM;
    // 将数据读入buffer
    read_frame(cvframe,info.ptr);

    res = mpp_buffer_commit(group,&info);
    if(res != MPP_OK){
        return res;
    }

    res = mpp_buffer_get(group,&commitBuffer,image_size);
    if(res != MPP_OK){
        return res;
    }

    mpp_frame_init(&mppframe);
    mpp_frame_set_width(mppframe,width);
    mpp_frame_set_height(mppframe,height);
    mpp_frame_set_hor_stride(mppframe,yuv_hor_stride);
    mpp_frame_set_ver_stride(mppframe,ver_stride);
    mpp_frame_set_buf_size(mppframe,image_size);
    mpp_frame_set_buffer(mppframe,commitBuffer);
    /**
     * 使用mpp可以使用 YUV格式的数据外 还能使用RGB格式的数据
     * 但是使用ffmpeg的h264_rkmpp系列编码器只能使用YUV格式
    */
    mpp_frame_set_fmt(mppframe,MPP_FMT_YUV420SP); // YUV420SP == NV12 
    mpp_frame_set_eos(mppframe,0);

    AVDRMFrameDescriptor* desc = (AVDRMFrameDescriptor*)av_mallocz(sizeof(AVDRMFrameDescriptor));
    if (!desc) {
        return MPP_NOK;
    }
    desc->nb_objects = 1;
    desc->objects[0].fd = mpp_buffer_get_fd(commitBuffer);
    desc->objects[0].size = mpp_buffer_get_size(commitBuffer);

    desc->nb_layers = 1;
    AVDRMLayerDescriptor *layer = &desc->layers[0];
    layer->format = DRM_FORMAT_YUV420;
    layer->nb_planes = 2;

    // Y 分量
    layer->planes[0].object_index = 0;
    layer->planes[0].offset = 0;
    layer->planes[0].pitch = mpp_frame_get_hor_stride(mppframe); // 1920

    // 第二层分量
    layer->planes[1].object_index = 0;
    layer->planes[1].offset = layer->planes[0].pitch * ver_stride; // 1920 * 1088
    layer->planes[1].pitch = layer->planes[0].pitch;

    avframe->reordered_opaque = avframe->pts;

    avframe->color_range      = (AVColorRange)mpp_frame_get_color_range(mppframe);
    avframe->color_primaries  = (AVColorPrimaries)mpp_frame_get_color_primaries(mppframe);
    avframe->color_trc        = (AVColorTransferCharacteristic)mpp_frame_get_color_trc(mppframe);
    avframe->colorspace       = (AVColorSpace)mpp_frame_get_colorspace(mppframe);

    auto mode = mpp_frame_get_mode(mppframe);
    avframe->interlaced_frame = ((mode & MPP_FRAME_FLAG_FIELD_ORDER_MASK) == MPP_FRAME_FLAG_DEINTERLACED);
    avframe->top_field_first  = ((mode & MPP_FRAME_FLAG_FIELD_ORDER_MASK) == MPP_FRAME_FLAG_TOP_FIRST);

    AVBufferRef * framecontextref = (AVBufferRef *)av_buffer_allocz(sizeof(AVBufferRef));
    if (!framecontextref) {
        return MPP_NOK;
    }

    // MPP decoder needs to be closed only when all frames have been released.
    RKMPPFrameContext * framecontext = (RKMPPFrameContext *)framecontextref->data;
    framecontext->frame = mppframe;
    
    avframe->data[0]  = (uint8_t *)desc;
    avframe->buf[0]   = av_buffer_create((uint8_t *)desc, sizeof(*desc), rkmpp_release_frame,
                                       framecontextref, AV_BUFFER_FLAG_READONLY);

    return res;
}

int transfer_frame(cv::Mat &cvframe,Command &obj){
    int packetFinish= 0 ,frameFinish = 0;
    int res = 0;
    // 给帧打上时间戳
	frame->pts =(framecount) * av_q2d(codecCtx->time_base);
	//一行（宽）数据的字节数 列数x3
    
    convert_cvframe_to_drm(cvframe,frame);

    if(res < 0){
        print_error(__LINE__,res,"transfer data to hdwave failed !");
        return -1;
    }
    res = avcodec_send_frame(codecCtx,frame);
    if(res!=0){
        print_error(__LINE__,res,"send frame to avcodec failed !");
        return -1;
    }

    res = avcodec_receive_packet(codecCtx,packet);
    if(res != 0){
        print_error(__LINE__,res,"fail receive encode packet!");
        return -1;
    }
    packet->pts = av_rescale_q_rnd(framecount, codecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    packet->dts = av_rescale_q_rnd(framecount, codecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    packet->duration = av_rescale_q_rnd(packet->duration, codecCtx->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
    
    if(!strcmp(obj.get_protocol(),"rtsp") && !(packet->flags & AV_PKT_FLAG_KEY)){
        // 在每帧非关键帧前面添加PPS SPS头信息
        memset(cExtradata,0,extra_data_size*sizeof(uint8_t));
        memcpy(cExtradata, codecCtx->extradata, codecCtx->extradata_size);
        memcpy(cExtradata + codecCtx->extradata_size, packet->data, packet->size);
        packet->size += codecCtx->extradata_size;
        packet->data = cExtradata;
    }
    // 通过创建输出流的format 输出数据包
    framecount++;
    res = av_interleaved_write_frame(formatCtx, packet);
    if (res < 0){
        print_error(__LINE__,res,"send packet error!");
        return -1;
    }
    
    if(buffer != NULL){
        mpp_buffer_put(buffer); // 清空buffer
        buffer = NULL;
    }
    if(commitBuffer != NULL){
        mpp_buffer_put(commitBuffer); // 清空buffer
        commitBuffer = NULL;
    }
    mpp_buffer_group_clear(group);
    mpp_frame_deinit(&mppframe);
    return 0;
}

void destory_(){
    cout << "释放回收资源：" << endl;
    mpp_buffer_group_put(group);
	// fclose(wf);
	if(formatCtx){
        // avformat_close_input(&avFormatCtx);
        avio_close(formatCtx->pb);
        avformat_free_context(formatCtx);
        formatCtx = 0;
        cout << "avformat_free_context(formatCtx)" << endl;
    }
    if(packet){
        av_packet_unref(packet);
        packet = NULL;
        cout << "av_free_packet(packet)" << endl;
    }

    if(frame){
        av_frame_free(&frame);
        frame = 0;
        cout << "av_frame_free(frame)" << endl;
    }
   
    if(codecCtx->hw_device_ctx){
        av_buffer_unref(&codecCtx->hw_device_ctx);
        cout << "av_buffer_unref(&codecCtx->hw_device_ctx)" << endl;
    }

    if(codecCtx){
        avcodec_close(codecCtx);
        codecCtx = 0;
        cout << "avcodec_close(codecCtx);" << endl;
    }
    if(cExtradata){
        free(cExtradata);
        cout << "free cExtradata " << endl;
    }
    if(hwdevice){
        av_buffer_unref(&hwdevice);
        cout << "av_buffer_unref hwdevice " << endl;
    }
    if(hwframe){
        av_buffer_unref(&hwframe);
        cout << "av_buffer_unref encodeHwBufRef " << endl;
    }
}

int main(int argc, char * argv[]){
    Command obj = process_command(argc,argv);
    int res = 0;
    VideoCapture videoCap;
    videoCap.set(CAP_PROP_FRAME_WIDTH, 1920);//宽度
    videoCap.set(CAP_PROP_FRAME_HEIGHT, 1080);//高度
    videoCap.set(CAP_PROP_FPS, 30);//帧率 帧/秒
    videoCap.set(CAP_PROP_FOURCC,VideoWriter::fourcc('M','J','P','G')); // 捕获格式
    // videoCap.set(CAP_PROP_FOURCC,VideoWriter::fourcc('I','4','2','0')); // 捕获格式
    // videoCap.set(CAP_PROP_CONVERT_RGB,0);
    videoCap.open(40);
    if(!videoCap.isOpened()){
        cout << "camera not open !" << endl;
        return -1;
    }
	int is_init_encoder = 0;
    Mat cvframe,yuvframe;
	while (videoCap.read(cvframe))
	{
		imshow("video_show", cvframe);
        if((waitKey(1) & 0xff) == 27){
            break;
        }
        if(!cvframe.data){
            continue;
        }
        cvtColor(cvframe,yuvframe,COLOR_RGB2YUV_YV12);
		if(!is_init_encoder){
            Size size_yuv = yuvframe.size();
            yuv_width = size_yuv.width;
            yuv_height = size_yuv.height;
            
            yuv_hor_stride = MPP_ALIGN(yuv_width, 16); // 1920
            yuv_ver_stride = MPP_ALIGN(yuv_height, 16); // 1632
            
            Size size = cvframe.size();
            width = size.width;
            height = size.height;
            
            hor_stride = MPP_ALIGN(width, 16); // 1920
            ver_stride = MPP_ALIGN(height, 16); // 1088

            image_size = sizeof(unsigned char) * yuv_hor_stride *  yuv_hor_stride;
            cout << " width " << width << endl;
            cout << " height " << height << endl;
            cout << " hor_stride " << hor_stride << endl;
            cout << " ver_stride " << ver_stride << endl;
            // cout << format(yuvframe,Formatter::FMT_C) << endl;
            cout << " yuv frame info " << endl;
            cout << " yuv frame cols " << yuvframe.cols << endl;
            cout << " yuv frame rows " << yuvframe.rows << endl;
            cout << " yuv frame elesize " << yuvframe.elemSize() << endl;
            cout << " yuv frame channel " << yuvframe.channels() << endl;
			res = init_encoder(obj); //初始化解码器
            if(res < 0){
                print_error(__LINE__,res,"init encoder failed!");
                break;
            }
			res = init_data(obj);
            if(res < 0){
                print_error(__LINE__,res,"init data failed!");
                break;
            }
            res = init_mpp();
            if(res < 0){
                print_error(__LINE__,res,"init mpp failed!");
                break;
            }
			is_init_encoder = 1;
		}
		transfer_frame(yuvframe,obj);
		av_packet_unref(packet);
	}
FAIL:
    videoCap.release();
    destory_();
    return 0;
}