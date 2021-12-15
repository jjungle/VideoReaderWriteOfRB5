#ifndef __VIDEO_READER_H__
#define __VIDEO_READER_H__

#include "opencv2/opencv.hpp"
#include <gst/gst.h>

class VideoReader {
public:
    /**
     * @brief 打开视频, url为视频的文件路径或者网络地址
     */
    int Open(const std::string &url); 

    /**
     * 读取视频帧，顺序读取，时间戳单位秒
     */ 
    int Read(cv::Mat &frame, double &timestamp);

    /**
     * @brief 视频帧率 
     * @param fps = framerate.first/framerate.second
     */
    std::pair<int, int> Framerate() {  
        return framerate_; 
    }
    
	/**
     * @brief 需要输入原视频宽高, 因为通过gst得到的是对齐后宽高，需要是16的整数倍
     */
    void InputOriginSize(const int width, const int height) {
        width_ = width;
        height_ = height;
    }

    ~VideoReader();

private:
    // int NextFrame(AVFrame *frame);
    int RecvDecodedFrame(cv::Mat& frame, double& timestamp);
    GstElement* pipeline_;
    GstElement* source_;
    GstElement* qtdemux_;
    GstElement* queue_;
    GstElement* h264parse_;
    GstElement* omxh264dec_;
    GstElement* sink_;

    std::string srcFmt_;
    int paddedWidth_ = 0;
    int paddedHeight_ = 0;
    int width_ = 0;
    int height_ = 0;
    std::pair<int, int> framerate_;
};

#endif