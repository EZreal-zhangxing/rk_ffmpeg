# rk_ffmpeg
[2023/07/06] 在rockchip3588上实现用ffmpeg进行推拉流,本项目支持如下特性
- [x] 读取摄像头并推流
- [x] 支持推流协议RTMP,RTSP
- [x] 拉流使用硬件加速解码
- [x] 从网络拉取数据流
- [x] 从本地拉取视频文件
- [ ] 推流也使用硬件加速
## 1.安装FFMPEG
官方ffmpeg 并没有对rockchip的硬编解码做适配，所以我选择了一个魔改版本的[ffmpeg](https://github.com/jjm2473/ffmpeg-rk/tree/enc)

同时需要安装如下依赖：
- [x] install libx264
- [x] install librga

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
从摄像头设备中读取数据并发送,摄像头采样参数默认w=640,h=480,fps=30
```shell
-nd 默认网络路径
-n <url> 网络路径
-cn <name> 编码器
-hd 使用硬件编解码，功能有待更新
-fps <fps> 采样帧率
-preset <> ffmpeg编码参数
-profile <> ffmpeg编码参数
-tune <> ffmpeg编码参数 
rtsp/rtmp 使用RTSP/RTMP协议，default=rtsp
```

### 2. push_camera_rtmp
功能：使用ffmpeg进行录屏 并发送
```shell
-nd 默认网络路径
-n <url> 网络路径
-cn <name> 编码器
-hd 使用硬件编解码，功能有待更新
-fps <fps> 采样帧率
-preset <> ffmpeg编码参数
-profile <> ffmpeg编码参数
-tune <> ffmpeg编码参数 
rtsp/rtmp 使用RTSP/RTMP协议，default=rtsp
-cpn <name> 视频输入设备:x11grab等
-w 宽 default = 1440
-h 高 default = 900
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