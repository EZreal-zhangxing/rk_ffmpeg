#ifndef COMMAND
#define COMMAND
#include<string>
class COMMAND_STR
{
private:
    int type; // 0 本地文件 1 网络文件
    std::string url;
    int use_hdwave; // 0 不使用，1 使用
    std::string transport; // 0 TCP, 1 UDP
    std::string video_size = "0x0";
    /* data */
public:
    COMMAND_STR() = default;
    COMMAND_STR(const int type) : type(type),use_hdwave(0),transport("tcp"){};
    COMMAND_STR(const int type,const char * url): type(type),url(std::string(url)),use_hdwave(0),transport("TCP"){};
    const char * getUrl();
    void setUrl(const char * str);
    int getType();
    void setType(int t);
    void setVideo_size(const char * videoSize);
    const char * getVideo_size();
    int getUseHdwave();
    void setUseHdwave(int hdwave);
    void setTransport(const char * transportStr);
    const char * getTransport();

    void print_command_str(){
        std::cout << " url: " << url << std::endl;
        std::cout << " type: " << type << std::endl;
        std::cout << " use hw: " << use_hdwave << std::endl;
        std::cout << " transport: " << transport ;
        std::cout << " video_size: " << video_size << std::endl;
    }
};

const char * COMMAND_STR::getUrl(){
    return url.c_str();
}

void COMMAND_STR::setUrl(const char * str){
    url = std::string(str);
}

int COMMAND_STR::getType(){
    return type;
}

void COMMAND_STR::setType(int t){
    type = t;
}

int COMMAND_STR::getUseHdwave(){
    return use_hdwave;
}

void COMMAND_STR::setUseHdwave(int hdwave){
    use_hdwave = hdwave;
}

const char * COMMAND_STR::getTransport(){
    return transport.c_str();
}
void COMMAND_STR::setTransport(const char * transportStr){
    transport = std::string(transportStr);
}

const char * COMMAND_STR::getVideo_size(){
    return video_size.c_str();
}

void COMMAND_STR::setVideo_size(const char * videoSize){
    video_size = videoSize;
}
#endif
