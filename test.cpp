#include <iostream>
#include "cpp_src/share_memory.h"
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <ctime>
#include <chrono>

using namespace std::chrono;

inline double GetSpan(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end){
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return double(duration.count()) * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den;
}

int main( int argc, char** argv ){

    int img_rows = atoi(argv[1]);
    int img_cols = atoi(argv[2]);
    int img_nums = atoi(argv[3]);
    int threads_ = atoi(argv[4]);
    std::cout << "图片尺寸: " << img_rows << "*" << img_cols << std::endl;
    std::cout << "测试数量: " << img_nums << std::endl;

    //cv::Mat Img = cv::imread("../01.png",cv::IMREAD_COLOR);
    cv::Mat Img = cv::Mat(img_rows, img_cols, CV_8UC3, cv::Scalar(0, 255, 0));
    auto image_size = Img.cols * Img.rows * Img.channels();
    std::cout << "图片大小: " << image_size << std::endl;

    ShareMem::ShareMemory ShareImpl(2615);
    ShareImpl.SetShareHead(Img.rows, Img.cols, Img.channels());

    auto time_start =  system_clock::now();   // 开始计时
    auto time_end = system_clock::now();    // 结束计时
    double time_send_total = 0;   // 发送总时间

    while (ShareImpl.ShareMemoryPtr()->status != READY){
    }

    std::cout << "=========开始传输数据=========" << std::endl;

    for (int i = 0; i < img_nums; i++) {
        // 开始计时
        if (i == 2){
            time_start = system_clock::now();
            time_send_total = 0;
        }
        auto time_start_cpy = system_clock::now();
        ShareImpl.WriteData((u_char*)Img.data, image_size, 0, threads_);
        auto time_end_cpy = system_clock::now();
        time_send_total += GetSpan(time_start_cpy, time_end_cpy);

        std::cout<<"[main-结果]: "<<ShareImpl.CallSlave()<<std::endl;
    }
    time_end = system_clock::now();    // 结束计时

    std::cout<< "平均每次发送耗时: " << time_send_total *1000/(img_nums - 2)<<"ms"<<std::endl;
    std::cout<< "平均每轮收发耗时: " << GetSpan(time_start, time_end)*1000/(img_nums - 2)<<"ms"<<std::endl;

    //销毁
    ShareImpl.DestroyShare(); //销毁共享内存  类释放时候会自动销毁，这里做个提醒

    return 0;
}
