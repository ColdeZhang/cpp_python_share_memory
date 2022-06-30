#include <iostream>
#include "share_memory.h"
#include <opencv2/core/core.hpp>
#include <ctime>
#include <chrono>

using namespace chrono;

inline double GetSpan(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end){
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return double(duration.count()) * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den;
}

int main( int argc, char** argv ){

    int img_rows = atoi(argv[1]);
    int img_cols = atoi(argv[2]);
    int img_nums = atoi(argv[3]);
    std::cout << "图片尺寸: " << img_rows << "*" << img_cols << std::endl;
    std::cout << "测试数量: " << img_nums << std::endl;

    std::cout << "=========开始传输数据=========" << std::endl;
    //cv::Mat Img = cv::imread(argv[2],cv::IMREAD_COLOR);
    cv::Mat Img = cv::Mat(img_rows, img_cols, CV_8UC3, cv::Scalar(0, 255, 0));
    std::cout << "图片大小: " << Img.cols * Img.rows * Img.channels() << std::endl;

    ShareMem::ShareMemory ShareImpl(12331);
    ShareImpl.SetStatus(CAN_WRITE);
    ShareImpl.SetShareHead(Img.rows, Img.cols);
    ShareImpl.ShareMemoryPtr()->frame = 0;

    auto time_start =  system_clock::now();   // 开始计时
    auto time_end = system_clock::now();    // 结束计时
    double time_send_total = 0;   // 发送总时间

    while(true){
        if (ShareImpl.FlagStatus() == CAN_WRITE){
            if (ShareImpl.ShareMemoryPtr()->frame == 2){
                time_start = system_clock::now();
                time_send_total = 0;
            }
            auto time_start_cpy = system_clock::now();

            ShareImpl.PutShareBody((u_char *)Img.data, Img.cols * Img.rows * Img.channels());
            ShareImpl.ShareMemoryPtr()->frame++;

            auto time_end_cpy = system_clock::now();
            time_send_total += GetSpan(time_start_cpy, time_end_cpy);

        }

        if (ShareImpl.ShareMemoryPtr()->frame >= img_nums && ShareImpl.ShareMemoryPtr()->flag == CAN_WRITE){
            time_end = system_clock::now();    // 结束计时
            ShareImpl.ShareMemoryPtr()->flag = 3;
            break;
        }
    }

    std::cout<< "平均写入共享内存耗时: " << time_send_total *1000/(img_nums - 2)<<"ms"<<std::endl;
    std::cout<< "平均每次发送接收耗时: " << GetSpan(time_start, time_end)*1000/(img_nums - 2)<<"ms"<<std::endl;



    //销毁
    ShareImpl.DestroyShare(); //销毁共享内存  类释放时候会自动销毁，这里做个提醒

    return 0;
}
