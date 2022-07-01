//
// Created by Deer on 2022/6/27.
//

#include "share_memory.h"

namespace ShareMem{

    ShareMemory::ShareMemory(key_t share_key){
        CreateShare(share_key);
        // 开辟一个独立线程等待初始化完成
        std::thread([this](){
            while(true){
                std::cout<<"\b[C]: 等待主从初始化";
                usleep(100);
                if(p_share_data->host_pid != -1 && p_share_data->slave_pid != -1){
                    std::cout<<"\b[C]: 初始化完成"<<std::endl;
                    p_share_data->status = READY;
                    break;
                }
            }
        }).detach();
        std::cout<<"host pid: "<<p_share_data->host_pid<<std::endl;
        std::cout<<"slave pid: "<<p_share_data->slave_pid<<std::endl;
        p_share_data->host_pid = getpid();
    }

    ShareMemory::~ShareMemory() {
        DestroyShare();
    }

    int ShareMemory::SlaveCreateShare(key_t share_key){
        CreateShare(share_key);
        p_share_data->slave_pid = getpid();
    }

    int ShareMemory::CreateShare(key_t share_key) {
        share_memory_key = share_key;
        // 1.创建共享内存
        share_memory_id = shmget(share_memory_key, sizeof(ShareData), 0666 | IPC_CREAT);
        // 2.映射共享内存地址
        share_memory_address = shmat(share_memory_id, (void*)nullptr, 0);//如果创建一个函数每次调用都执行，需要执行完释放一下shmdt
        // 3.将数据结构指向共享内存地址
        p_share_data = (ShareData *)share_memory_address;
        std::cout<<"共享内存地址 ： "<<(int *)(share_memory_address)<<std::endl;

        return 1;
    }

    int ShareMemory::DestroyShare() const {
        shmdt(share_memory_address);    // 断开映射 ，保证下次访问不被占用
        shmctl(share_memory_id, IPC_RMID, nullptr); // 释放共享内存地址
        cout<<"\b[C]: 共享内存已经销毁";
        return 1;
    }

    std::string ShareMemory::CallSlave() {
        // 发送信号通知从进程传输完成，开始执行计算
        kill(p_share_data->slave_pid, WORK_IT_OUT);
        // 等待从进程执行完成
        while(p_share_data->status != READY){
            std::cout<<"[C]: 等待从进程处理数据"<<std::endl;
            if (p_share_data->status == JOB_DONE) {
                memccpy(response, p_share_data->response, '\0', 1024);
            }
        }
        p_share_data->status = READY;
        return response;
    }

    int ShareMemory::WriteData(u_char *data_ptr, unsigned long data_size, size_t offset, int multi_threads) const {
        if(p_share_data->status != READY){
            return 0;
        }

        auto start_address = p_share_data->data_body + offset; // 写入起始地址
        p_share_data->data_size += data_size;

        unsigned long extra_size = data_size % multi_threads; // 剩余的数据大小

        size_t block_size = data_size / multi_threads; // 每个线程写入的数据大小

        // 使用多线程写入数据
        for (int i = 0; i < multi_threads; i++) {
            std::thread([&]() {
                memcpy(start_address + i * block_size, data_ptr + i * block_size, data_size / multi_threads);
            }).detach();
        }
        if (extra_size > 0) {
            std::thread([&]() {
                memcpy(start_address + multi_threads * block_size, data_ptr + multi_threads * block_size, extra_size);
            }).detach();
        }

        return 1;
    }

    u_char *ShareMemory::GetShareBodyAddress() const {
        return (u_char*)p_share_data->data_body;
    }

    int ShareMemory::SetStatus(int value) const {
        p_share_data->status =value;
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

    int ShareMemory::SetShareHead(unsigned long rows, unsigned long cols) const {
        p_share_data->rows = rows;
        p_share_data->cols = cols;
        return 0;
    }

    int ShareMemory::WriteResult(u_char *result_ptr) const {
        memccpy(p_share_data->response, result_ptr, '\0', 1024);
        p_share_data->status = JOB_DONE;
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

int create_share(key_t share_key) {
    useShare.SlaveCreateShare(share_key);
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

}