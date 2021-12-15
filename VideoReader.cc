#include "VideoReader.h"
#include <iostream>

static inline void QtdemuxPadAddedCb(GstElement *qtdemux, GstPad *pad, GstElement *queue) {
    gst_element_link_pads(qtdemux, GST_PAD_NAME(pad), queue, nullptr);
}

// 错误或EOS流结束处理函数，因为会阻塞线程，不用它
static inline void ErrHandle(GstElement *pipeline) {
    // Wait until error or EOS
    auto bus = gst_element_get_bus(pipeline);
    auto msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    // Message handling
    if (msg != nullptr) {
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *err = nullptr;
                gchar *debug_info = nullptr;
                gst_message_parse_error(msg, &err, &debug_info);
                std::cerr<< "Error received:" << err->message << std::endl;
                if (debug_info) {
                    std::cerr<< "Debugging information:" << debug_info << std::endl;
                }
                g_clear_error(&err);
                g_free(debug_info);
            }
                break;
            case GST_MESSAGE_EOS:
                std::cout << "End-Of-Stream reached" << std::endl;
                break;
            default:
                std::cout << "Unexpected message received" << std::endl;
                break;
        }
        gst_message_unref(msg);
    }

    // Free resources
    gst_object_unref(bus);
}

// 主要功能：将pipeline中的数据拷贝到cv:Mat
int VideoReader::RecvDecodedFrame(cv::Mat& frame, double& timestamp) {
    GstSample *sample;
    // 使用pull-sample拉取视频帧，并映射到map变量，通过map拷贝出frame数据
    g_signal_emit_by_name(sink_, "pull-sample", &sample);
    if (sample) {
        auto buffer = gst_sample_get_buffer(sample);

        // fetch timestamp
        timestamp = static_cast<double>(GST_BUFFER_PTS(buffer)) / static_cast<double>(GST_SECOND);
        // std::cout << "timestamp:" << timestamp << std::endl;

        // copy buffer data into cv::Mat
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            std::cout << "recv data size:" << map.size << std::endl;
            if (srcFmt_ != "NV12" && srcFmt_ != "I420") {
                std::cout << "unsupported src pixel format" << std::endl;
                return -1;
            }
            static cv::Mat image;
                if (image.empty()) {
                    image.create(cv::Size(width_, height_ * 3 / 2), CV_8UC1);
                }
                // 1. copy into cv::Mat
                if (paddedWidth_ == width_) {
                    memcpy(image.data, map.data, width_ * sizeof(uint8_t) * height_);
                    memcpy(image.data + height_ * width_, map.data + paddedHeight_ * width_, width_ * sizeof(uint8_t) * height_ / 2);
                } else {
                    // copy Y-channel, jump the padding width
                    for (int i = 0; i < height_; ++i) {
                        memcpy(image.data + i * width_, map.data + i * paddedWidth_, width_ * sizeof(uint8_t));
                    }
                    // copy UV-channel, jump the padding width
                    for (int i = 0; i < height_ / 2; ++i) {
                        memcpy(image.data + (height_ + i) * width_, map.data + (paddedHeight_ + i) * paddedWidth_, width_ * sizeof(uint8_t));
                    }
                }
                
                // 2. convert format
                if (srcFmt_ == "NV12") {
                     cv::cvtColor(image, frame, cv::COLOR_YUV2BGR_NV12);
                } else {
                     cv::cvtColor(image, frame, cv::COLOR_YUV2BGR_I420);
                }

            // release buffer mapping
            gst_buffer_unmap(buffer, &map);
        }
        // release sample reference
        gst_sample_unref(sample);
        return 0;
    } else {
      std::cerr << "recv null frame"  << std::endl;
      return -1;
    }
}

int VideoReader::Open(const std::string& url) {
    // create the elements
    source_     = gst_element_factory_make("filesrc", "InputFile");
    qtdemux_    = gst_element_factory_make("qtdemux", "QtDemux");
    queue_      = gst_element_factory_make("queue", "QueueReader");
    h264parse_  = gst_element_factory_make("h264parse", "H264Parse");
    omxh264dec_ = gst_element_factory_make("omxh264dec", "OmxH264Dec");
    sink_       = gst_element_factory_make("appsink", "CustomSink");

    pipeline_ = gst_pipeline_new("decode-pipeline");

    if (!pipeline_ || !source_ || !qtdemux_ || !queue_ || !omxh264dec_ || !h264parse_ || !sink_) {
        std::cerr<< "Not all elements could be created" << std::endl;
        return -1;
    }
    // Modify element properties
    g_object_set(G_OBJECT(source_), "location", url.c_str(), nullptr);
    g_object_set(G_OBJECT(sink_), "emit-signals", TRUE, "max-buffers", 1, nullptr);

    // Build the pipeline
    gst_bin_add_many(GST_BIN(pipeline_), source_, qtdemux_, queue_, h264parse_, omxh264dec_, sink_, nullptr);

    if (gst_element_link(source_, qtdemux_) != TRUE ) {
        std::cerr<< "source and qtdemux could not be linked"  << std::endl;
        // gst_object_unref(pipeline_);
        return -1;
    }
    if (gst_element_link_many(queue_, h264parse_, omxh264dec_, sink_, nullptr) != TRUE ) {
        std::cerr<< "queue, h264parse, omxh264dec, and sink could not be linked"  << std::endl;
        // gst_object_unref(pipeline);
        return -1;
    }
    // dynamic padded connect between demux and queue
    g_signal_connect(qtdemux_, "pad-added", (GCallback)QtdemuxPadAddedCb, queue_);

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr<< "Unable to set the pipeline to the paused state" << std::endl;
        return -1;
    }
    GstSample *sample;
    g_signal_emit_by_name(sink_, "pull-preroll", &sample);
    if (sample) {
        //std::cout << "recv frame" << std::endl;
        auto buffer = gst_sample_get_buffer(sample);
        // fetch video infomation
        if (paddedHeight_ == 0 && paddedWidth_ == 0) {
            GstCaps *caps = gst_sample_get_caps(sample);
            GstStructure* info = gst_caps_get_structure(caps, 0);
            gst_structure_get_int(info, "width", &paddedWidth_);
            gst_structure_get_int(info, "height", &paddedHeight_);
            const char* format = gst_structure_get_string(info, "format");
            gst_structure_get_fraction(info, "framerate", &framerate_.first, &framerate_.second);
            srcFmt_ = format;

            std::cout << "padded width:" << paddedWidth_ << "padded height:" << paddedHeight_ << std::endl;
            std::cout << "format:" << srcFmt_ << std::endl;
            std::cout << "framerate num:" << framerate_.first << "framerate den:" << framerate_.second << std::endl;
        }
        // release sample reference
        gst_sample_unref(sample);
    }

    // set pipeline to playing
    ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr<< "Unable to set the pipeline to the playing state" << std::endl;
        return -1;
    }

    // handle error or EOS. Atention: error handle will block, so don't use it.
    // ErrHandle(pipeline_);
    return 0;
}

int VideoReader::Read(cv::Mat &frame, double &timestamp) {
    return RecvDecodedFrame(frame, timestamp);
}

VideoReader::~VideoReader() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
}