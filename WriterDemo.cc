#include "VideoReader.h"
#include "VideoWriter.h"
#include <ostream>
#include <opencv2/opencv.hpp>

void TestVideoReadWrite(std::string url, std::string outUrl, int count) {
    std::cout << "video:" << url << std::endl;

    int width = 1920, height = 1080;
    VideoReader reader;
    reader.InputOriginSize(width, height);
    auto ret = reader.Open(url);
    if (ret < 0) return;

    VideoWriter writer;
    writer.SetSize(width, height);
    writer.SetFramerate(reader.Framerate());
    ret = writer.Open(outUrl);
    if (ret < 0) return;

    cv::Mat frame;
    int seq = 0;
    double timestamp = .0;
    while (seq++ < count) {
        auto ret = reader.Read(frame, timestamp);
        if (ret < 0) break;
        // std::string filename = "./bin/"  + std::to_string(seq) + ".jpg";
        // cv::imwrite(filename, frame);
        writer.Write(frame, timestamp);
    }

    std::cout << "video read write test exit" << std::endl;
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::string inputUrl(argv[1]);
    std::string outputUrl(argv[2]);

    TestVideoReadWrite(inputUrl, outputUrl, 100);

    return 0;
}