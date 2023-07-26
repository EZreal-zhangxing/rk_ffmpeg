#ifndef COMMAND_MPP
#define COMMAND_MPP

#include<string>
#include<iostream>

class Command{
private:
    std::string url; // 网络推流URL
    std::string codec_name; // 指定编码器
    int use_hw;// 是否使用硬件编码 0 否，1是
    /*********推流参数***********/
    std::string preset; 
    std::string tune; 
    std::string profile; 
    /*********推流参数***********/
    int fps; // 指定fps
    std::string protocol; // 协议 rtsp/rtmp
    std::string capture_name;// 捕获名字
    std::string trans_protocol; // 传输协议 TCP/UDP

    int width; // 宽
    int height;// 高

public:
    Command():
        use_hw(0),
        fps(30),
        preset("ultrafast"),
        tune("zerolatency"),
        profile("high"),
        protocol("rtsp"),
        capture_name("x11grab"),
        trans_protocol("tcp"),
        width(1440),
        height(900){};
    Command(const std::string url,const std::string codec_name) : \
        url(url),
        codec_name(codec_name),
        use_hw(0),
        fps(30),
        preset("ultrafast"),
        tune("zerolatency"),
        profile("high"),
        protocol("rtsp"),
        capture_name("x11grab"),
        trans_protocol("tcp"),
        width(1440),
        height(900){};
    void set_url(const char * u){url = std::string(u);}
    void set_codec_name(const char * cn){codec_name = std::string(cn);}
    void set_use_hw(int use_hw){this->use_hw = use_hw;};
    void set_preset(const char * ps){preset = std::string(ps);};
    void set_tune(const char * t){tune = std::string(t);};
    void set_profile(const char * pf){profile = std::string(pf);};
    void set_fps(int fps){this->fps = fps;};
    void set_protocol(const char * pl){protocol = std::string(pl);};
    void set_capture_name(const char * cn){capture_name = std::string(cn);};
    void set_trans_protocol(const char * tp){trans_protocol = std::string(tp);};
    void set_width(const char *w){width = atoi(w);};
    void set_height(const char * h){height = atoi(h);};

    const char * get_url(){return url.c_str();}
    const char * get_codec_name(){return codec_name.c_str();}
    int get_use_hw(){return use_hw;};
    const char * get_preset(){return preset.c_str();};
    const char * get_tune(){return tune.c_str();};
    const char * get_profile(){return profile.c_str();};
    int get_fps(){return fps;};
    const char * get_protocol(){return protocol.c_str();};
    const char * get_capture_name(){return capture_name.c_str();};
    const char * get_trans_protocol(){return trans_protocol.c_str();};
    int get_width(){return width;};
    int get_height(){return height;};

    void print_command_str(){
        std::cout << " url: " << url << std::endl;
        std::cout << " protocol: " << protocol;
        std::cout << " trans protocol: " << trans_protocol << std::endl;
        std::cout << " codec_name: " << codec_name << std::endl;
        std::cout << " fps: " << fps << std::endl;
        std::cout << " use hw: " << use_hw << std::endl;
        std::cout << " media from : " << capture_name << std::endl;
        std::cout << " preset: " << preset ;
        std::cout << " tune: " << tune ;
        std::cout << " profile: " << profile << std::endl;
        std::cout << " width: " << width << " height: "<< height << std::endl;        
    }
};

Command process_command(int argc,char * argv[]){
    Command res = Command();
    for(int i=1;i<argc;i++){
        std::string flag = argv[i];
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
        if(flag == "tcp" || flag == "udp"){
            res.set_trans_protocol(argv[i]);
        }
    }
    res.print_command_str();
    return res;
}


#endif