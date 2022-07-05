//
// Created by Deer on 2022/6/27.
//

#include "share_memory.h"

namespace ShareMem{

    ShareMemory::ShareMemory(key_t share_key){
        CreateShare(share_key);
        pool.init();    // 初始化线程池
        p_share_data->host_pid = getpid();
        p_share_data->slave_pid = getpid();
        p_share_data->data_size = 0;

        // 开辟一个独立线程等待初始化完成
        std::thread([this](){
            std::cout<<"[C]: 等待微服务初始化..."<<std::endl;
            while(true){
                usleep(100);
                if(p_share_data->host_pid != p_share_data->slave_pid){
                    std::cout<<"[C]: 初始化完成 | host_pid: "<<p_share_data->host_pid<<" | slave_pid: "<<p_share_data->slave_pid<<std::endl;
                    p_share_data->status = READY;
                    break;
                }
            }
        }).detach();
    }

    ShareMemory::~ShareMemory() {
        DestroyShare();
    }

    int ShareMemory::RegisterSlavePid(key_t slave_pid){
        p_share_data->slave_pid = slave_pid;
    }

    int ShareMemory::CreateShare(key_t share_key) {
        share_memory_key = share_key;
        // 1.创建共享内存
        share_memory_id = shmget(share_memory_key, sizeof(ShareData), 0666 | IPC_CREAT);
        // 2.映射共享内存地址
        share_memory_address = shmat(share_memory_id, (void*)nullptr, 0);//如果创建一个函数每次调用都执行，需要执行完释放一下shmdt
        // 3.将数据结构指向共享内存地址
        p_share_data = (ShareData *)share_memory_address;
        std::cout<<"[C]: 共享内存地址 - "<<(int *)(share_memory_address)<<std::endl;

        return 1;
    }

    int ShareMemory::DestroyShare(){
        shmdt(share_memory_address);    // 断开映射 ，保证下次访问不被占用
        shmctl(share_memory_id, IPC_RMID, nullptr); // 释放共享内存地址
        std::cout<<"[C]: 共享内存已经销毁"<<std::endl;
        pool.shutdown(); // 关闭线程池
        return 1;
    }

    std::string ShareMemory::CallSlave() {
        char response[1024];
        // 发送信号通知从进程传输完成，开始执行计算
        kill(p_share_data->slave_pid, WORK_IT_OUT);
        // 等待从进程执行完成
        std::cout<<"[C]: 已通知微服务开始处理数据，等待微服务返回结果..."<<std::endl;
        while(true){
            this_thread::sleep_for(chrono::microseconds(10));
            if (p_share_data->status == JOB_DONE) {
                std::cout<<"[C]: 监测到数据处理完成，开始回收结果..."<<std::endl;
                memccpy(response, p_share_data->response, '\0', 1024);
                break;
            }
        }
        p_share_data->data_size = 0;
        p_share_data->status = READY;
        return response;
    }

    int ShareMemory::WriteData(u_char *data_ptr, unsigned long data_size, size_t offset, int multi_threads){
        if(p_share_data->status != READY){
            return 0;
        }
        if (multi_threads > 4) multi_threads = 4;

        auto start_address = p_share_data->data_body + offset; // 写入起始地址

        p_share_data->data_size += data_size;

        unsigned long extra_size = data_size % multi_threads; // 剩余的数据大小

        size_t block_size = data_size / multi_threads; // 每个线程写入的数据大小

        //memcpy(p_share_data->data_body, data_ptr, data_size);

        vector<std::future<int>> res;
        for (int i = 0; i < multi_threads; i++) {
            auto re = pool.submit(ShareMemory::copy_data,
                                  start_address + i * block_size,
                                  block_size,
                                  data_ptr + i * block_size);
            res.push_back(std::move(re));
        }
        // 等待传输任务完成
        res.back().get();
//        // 使用多线程写入数据
//        for (int i = 0; i < multi_threads; i++) {
//            std::thread([&]() {
//                memcpy(start_address + i * block_size, data_ptr + i * block_size, block_size);
//            });
//        }
//        if (extra_size > 0) {
//            std::thread([&]() {
//                memcpy(start_address + multi_threads * block_size, data_ptr + multi_threads * block_size, extra_size);
//            });
//        }

        return 1;
    }

    u_char *ShareMemory::GetShareBodyAddress() const {
        return (u_char*)p_share_data->data_body;
    }

    int ShareMemory::SetStatus(int value) const {
        p_share_data->status = value;
    }

    ShareData *ShareMemory::ShareMemoryPtr() const {
        return p_share_data;
    }

    ShareMemory::ShareMemory(char PyUseOnly) {
        std::cout<<"[C]: 成功加载共享内存动态库"<<PyUseOnly<<std::endl;
    }

    unsigned long ShareMemory::GetShareBodySize() const {
        return p_share_data->data_size;
    }

    int ShareMemory::WriteResult(u_char *result_ptr) const {
        memccpy(p_share_data->response, result_ptr, '\0', 1024);
        return 1;
    }

    int ShareMemory::copy_data(char *start_ptr, size_t data_size, u_char *data_ptr) {
        memcpy(start_ptr, data_ptr, data_size);
        return 1;
    }


}

/*!
 * @brief 按照C语言格式重新打包-python调用
 * @details 按照C语言格式重新打包-python调用
 * 如果调用端开启了线程  这部份无法直接访问到 需要在线程里面重新创建这个类
 * 如果是单纯的C++调用，不需要这个样子封装
 */
extern "C" {

ShareMem::ShareMemory useShare('.');

int create_share(key_t share_key, pid_t slave_pid) {
    useShare.CreateShare(share_key);
    useShare.RegisterSlavePid(slave_pid);
}

int destroy_share(){
    useShare.DestroyShare();
}

u_char* get_share_body_address(){
    return useShare.GetShareBodyAddress();
}

int write_result(u_char *result_ptr){
    return useShare.WriteResult(result_ptr);
}

unsigned long get_share_body_size(){
    return useShare.GetShareBodySize();
}

int set_status_done() {
    useShare.SetStatus(JOB_DONE);
}

int set_status_working() {
    useShare.SetStatus(WORKING);
}

unsigned long get_img_rows() {
    return useShare.ShareMemoryPtr()->rows;
}

unsigned long get_img_cols() {
    return useShare.ShareMemoryPtr()->cols;
}

unsigned long get_img_channels() {
    return useShare.ShareMemoryPtr()->channels;
}

}