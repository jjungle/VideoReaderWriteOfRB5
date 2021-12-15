#ifndef __VIDEO_WRITER_H__
#define __VIDEO_WRITER_H__


#include <string>
#include "opencv2/opencv.hpp"
#include <gst/gst.h>

class VideoWriter {
public:
    ~VideoWriter();
    /**
     * @brief 打开视频, url为视频的文件路径或者网络地址
     */
    int Open(const std::string url);

    /**
     * @brief 设置视频帧率
     * @param fps = framerate.first/framerate.second
     */
    void SetFramerate(std::pair<int, int> framerate) {
        framerate_ = framerate;
    }

    /**
     * @brief 设置视频分辨率
     */
    void SetSize(int width, int height) {
        width_ = width;
        height_ = height;
    }
    
    /**
     * @brief 设置视频码率
     * @param bitrate 单位bit/sec
     */
    void SetBitrate(int bitrate) {
        bitrate_ = bitrate;
    }

    /**
     * @brief 写入视频帧
     * @param timestamp 单位秒
     */
    int Write(const cv::Mat& frame, double timestamp) ;

private:
    int PushData2Pipeline(const cv::Mat& frame, double timestamp);
    GstElement *pipeline_; 
    GstElement *appSrc_; 
    GstElement *queue_; 
    GstElement *videoConvert_; 
    GstElement *encoder_; 
    GstElement *capsFilter_; 
    GstElement *mux_; 
    GstElement *sink_;
    int width_= 0;
    int height_ = 0;
    int bitrate_ = 0;
    std::pair<int, int> framerate_{30, 1};
};

#endif