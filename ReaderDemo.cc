#include <fstream>
#include "VideoReader.h"
#include <ostream>

void TestVideoReader(std::string url, std::string outUrl, int count) {
    std::cout << "video:" << url << std::endl;

    VideoReader video;
    // 需要输入原视频尺寸
    video.InputOriginSize(1920,1080);

    auto ret = video.Open(url);
    if (ret < 0) return;

    cv::Mat frame;
    int seq = 0;
    double timestamp = .0;

    while (seq++ < count) {
        std::cout << "reading " << seq << "th loop." << std::endl;

        auto ret = video.Read(frame, timestamp);
        if (ret < 0) break;

        std::string filename = outUrl + "/" + std::to_string(seq) + ".jpg";
        cv::imwrite(filename, frame);
    }

    std::cout << "video read over" << std::endl;
}


int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::string inputUrl(argv[1]);
    std::string outputUrl(argv[2]);

    std::cout << "read video:" << inputUrl << std::endl;

    TestVideoReader(inputUrl, outputUrl, 50);

    return 0;
}
