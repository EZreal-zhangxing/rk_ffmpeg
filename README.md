# rk_ffmpeg
[2023/07/06] 在rockchip3588上实现用ffmpeg进行推拉流,本项目支持如下特性
- [x] 读取摄像头并推流
- [x] 支持推流协议RTMP,RTSP
- [x] 拉流使用硬件加速解码
- [x] 从网络拉取数据流
- [x] 从本地拉取视频文件
[2023/07/26] 实现使用硬件进行编码然后推流
- [x] 推流也使用硬件加速
## 1.安装FFMPEG
官方ffmpeg 并没有对rockchip的硬编解码做适配，所以我选择了一个魔改版本的[FFMPEG](https://github.com/jjm2473/ffmpeg-rk/tree/enc)

同时需要安装如下依赖：
- [x] libx264
- [x] [mpp](https://github.com/HermanChen/mpp)，注意：使用develop分支
- [x] librga
- [x] libdrm
- [x] SDL 
  
  
编译安装方式如下：
```shell
./configure --prefix=/usr/local/ --enable-shared --enable-version3 --enable-rkmpp --enable-libx264 --enable-gpl --enable-libdrm --enable-nonfree --enable-hwaccels --enable-gpl
```
其中我为了添加了鼠标捕获操作添加了libxcb依赖包

```
--enable-libxcb --enable-libxcb-shm --enable-libxcb-xfixes --enable-libxcb-shape 
```
## 2.安装OpenCV[可选]
读取摄像头需要用到OpenCV，如果用不到读取摄像头的操作可以选装。

## 3.编译
在根目录下
```shell
cd build
cmake .. && make -j4
```

## 4.参数说明
### 1. remote_stream
从摄像头设备或者HDMI中读取数据并发送,摄像头采样参数默认w=640,h=480,fps=30，注意修改设备驱动/dev/video?
```shell
-nd 默认网络路径
-n <url> 网络路径
-cn <name> 编码器名称 libx264,h264,h264_rkmpp等
-hd 使用硬件编解码，功能有待更新（废除）
-fps <fps> 采样帧率
-preset <> ffmpeg编码参数
-profile <> ffmpeg编码参数
-tune <> ffmpeg编码参数 
rtsp/rtmp 使用RTSP/RTMP协议，default=rtsp
```
```shell
./remote_stream -n rtsp://xxxx/xxxx -cn libx264 rtsp
./remote_stream -n rtmp://xxxx/xxxx -cn libx264 rtmp
```

### 2. push_camera_rtmp
功能：使用ffmpeg调用x11grab进行录屏 并发送
```shell
-nd 默认网络路径
-n <url> 网络路径
-cn <name> 编码器名称 libx264,h264,h264_rkmpp等
-hd 使用硬件编解码，功能有待更新（废除）
-fps <fps> 采样帧率
-preset <> ffmpeg编码参数
-profile <> ffmpeg编码参数
-tune <> ffmpeg编码参数 
rtsp/rtmp 使用RTSP/RTMP协议，default=rtsp
-cpn <name> 视频输入设备:x11grab等
-w 宽 default = 1440
-h 高 default = 900
```
```shell
./push_camera_rtmp -n rtsp://xxxx/xxxx -cn libx264 rtsp
./push_camera_rtmp -n rtmp://xxxx/xxxx -cn libx264 rtmp
```

### 3. pullVideo
读取网络流或者本地文件
```shell
-l <url> 本地文件路径
-n <url> 网络路径
-hd 使用硬件编解码
-s <wxh> 读取YUV视频时需要指定宽和高
udp/tcp 使用udp/tcp协议，default=tcp
```

```shell
./pullVideo -n rtsp://xxxx/xxxx -hd
./pullVideo -l video.mp4 -hd
```

### 4. rtsp_send_opencv_ffmpeg
通过opencv捕获摄像头或者HDMI数据，将BGR数据转换为YUV，封装成AVFrame 送入由FFmpeg初始化的硬件编码器h264_rkmpp进行编码并推流
```shell
-nd 默认网络路径
-n <url> 网络路径
-cn <name> 编码器名称 libx264,h264,h264_rkmpp等 （废除，仅使用h264_rkmpp）
-hd 使用硬件编解码
-fps <fps> 采样帧率
-preset <> ffmpeg编码参数
-profile <> ffmpeg编码参数
-tune <> ffmpeg编码参数 
rtsp/rtmp 使用RTSP/RTMP协议，default=rtsp
tcp/udp 传输协议
```
```shell
./rtsp_send_opencv_ffmpeg -nd -hd rtsp
./rtsp_send_opencv_ffmpeg -n rtsp://xxxx/xxxx -hd rtsp
```

## 5. rtsp_send_opencv_mpp
通过opencv捕获摄像头或者HDMI数据，将BGR数据转换为YUV，送入由自行初始化的硬件编码器进行编码并推流
并封装成AvPacket交由FFmpeg 进行发送
```shell
-nd 默认网络路径
-n <url> 网络路径
-cn <name> 编码器名称 libx264,h264,h264_rkmpp等 （废除，仅使用h264_rkmpp）
-hd 使用硬件编解码
-fps <fps> 采样帧率
-preset <> ffmpeg编码参数
-profile <> ffmpeg编码参数
-tune <> ffmpeg编码参数 
rtsp/rtmp 使用RTSP/RTMP协议，default=rtsp
tcp/udp 传输协议
```
```shell
./rtsp_send_opencv_mpp -nd -hd rtsp
./rtsp_send_opencv_mpp -n rtsp://xxxx/xxxx -hd rtsp
```

## 5. rtsp_send_opencv_mpp_rgb
通过opencv捕获摄像头或者HDMI数据，将BGR数据直接送入由自行初始化的硬件编码器进行编码并推流
并封装成AvPacket交由FFmpeg 进行发送。与rtsp_send_opencv_mpp相比直接将BGR数据送入编码器
```shell
-nd 默认网络路径
-n <url> 网络路径
-cn <name> 编码器名称 libx264,h264,h264_rkmpp等 （废除，仅使用h264_rkmpp）
-hd 使用硬件编解码
-fps <fps> 采样帧率
-preset <> ffmpeg编码参数
-profile <> ffmpeg编码参数
-tune <> ffmpeg编码参数 
rtsp/rtmp 使用RTSP/RTMP协议，default=rtsp
tcp/udp 传输协议
```
```shell
./rtsp_send_opencv_mpp_rgb -nd -hd rtsp
./rtsp_send_opencv_mpp_rgb -n rtsp://xxxx/xxxx -hd rtsp
```

## 6.说明
### 6.1 编码相关
目前最新的MPP目前只支持YUV/BGR的数据编解码，因此需要保证图片格式的真确性我分别实现了这两种格式的数据编码。
可以参见调用硬件编码的代码：在5.4/5.5/5.6小节
整体流程如下：
1. 使用MPP硬件编码
将获取到的帧数据，送入mpp进行编解码，输出mppPacket 然后将mppPacket封装成AvPacket，整体流程如下：
```
----------                     -------               ----------     --------
| opencv | -->frame(BGR/YUV)-->| mpp | -->AvPacket-->| ffmpeg | --> | send |
----------                     -------               ----------     --------
```
2. 交由FFmpeg完成硬件编码
FFmpeg由于是魔改版本，他封装的硬件解码器只支持输入Drm帧的格式，因此需要将帧数据封装成AVFrame。并送入AvCodec，然后得到AvPacket,最后发送。整体流程如下：
```
----------                     ---------                   ----------                   --------
| opencv | -->frame(BGR/YUV)-->| model | -->AvFrame(drm)-->| ffmpeg | -->encode(mpp)--> | send |
----------                     ---------                   ----------                   --------
```
3. 不管是h264_rkmpp编码器还是纯mpi解码器而言，整个编码流程的关键点在于如何将数据中帧封装成硬件支持的DRM帧，得到MppFrame或者AvFrame，这里也是重点
整个帧的处理流程如下:
```
-----------    -----------------             -----------------           ----------     ------------
| rgb/yuv | -->| mppBufferInfo | -->commit-->| mppbufferPool | -->get--> | buffer | --> | mppFrame |
-----------    -----------------             -----------------           ----------     ------------
```

这里我们创建一个bufferinfo，并让info关联一块buffer，这块buffer通过fd关联至硬件中，然后将数据按照官方给的指定格式拷贝至这块区域中，我最后提交给bufferPool，然后通过get获取与其相关联的内存区域，最后封装成mppFrame 这里这就是drm帧，前提是bufferPool类型是交由Drm进行内存管理的

### 6.2 数据格式相关

#### 6.2.1 YUV数据格式的拷贝
这里以$1920 \times 1080$ 的图片来进行说明
官方手册说明，数据读取按照16位对齐，1920x1080x3 的RGB图片补齐后
hor_stride 1920,ver_stride = 1088
对于YUV图像，图像会下采样成1920x1080x3/2x1(Channel) 
yuv_hor_stride 1920,yuv_ver_stride = 1632
这两个图片的大小从
1920x1080x3(rgb) --> 1920x1080x3/2(yuv)
wxhxc:
1920x1080x3(rgb) --> 1920x1620x1(yuv)
同时由于补齐作用 YUV原始图像需要变成:
1920x1620 --> 1920x1632
其中YUV图像分量的分布为：wxh的亮度Y，w/2 x h/2的U，w/2 x h/2的V
所以只需要按照如下格式拷贝到buffer中即可
```
-------------w(1920)----------
|                            |
|                            |
|                            |
|             Y              h(1080)
|                            |
|                            |
|                            |
------------------------------
|                            |
|             gap            | 8
------------------------------
|                            |
|              U             h/4(270)
|                            |
------------------------------
|             gap            | 2
------------------------------
|                            |
|              V             h/4(270)
|                            |
------------------------------
|             gap            | 2
------------------------------
```
#### 6.2.2 RGB数据格式的拷贝
RGB格式则保持原样送入即可，不需要做通道提取。只不过要注意hor_stride = 3 * w,ver_stride = align(h,16)
```
-------------w(1920)-----------------------w(1920)-----------------------w(1920)----------
|r|g|b|r|g|b|                |                              |                            |
|                            |                              |                            |
|                            |                              |                            |
|             Y              h(1080)                        |                            |
|                            |                              |                            |
|                            |                              |                            |
|                            |                              |                            |
-------------------------------------------w(1920)-----------------------w(1920)----------
|                            |                              |                            |
|             gap            | 8                            |                            |
-------------------------------------------w(1920)-----------------------w(1920)----------
```


解码则只需要送入包即可，通过内置的bufferPool进行扭转，就能得到相关的MppFrame