//
// Created by Deer on 2022/6/27.
//

#ifndef CPP_PYTHON_SHARE_MEMORY_H
#define CPP_PYTHON_SHARE_MEMORY_H

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <cstdlib>
#include <sys/shm.h>
#include <iostream>
#include <vector>
#include <string>
#include <csignal>
#include <thread>
#include <future>
#include <queue>

using namespace std;

#define SHARE_MEMORY_SIZE (640 * 640 * 3 * 16)  // 共享内存最大容量

#define READY 2         // 准备好进行新一轮的数据传输
#define WORKING 3       // 正在处理数据中
#define JOB_DONE 4    // 结果在内存中

#define WORK_IT_OUT SIGUSR1 // 通知从进程开始计算

/// <summary>
/// 用于提前分配线程，支持任意任务提交，任意返回值类型，处理那些一定有返回的任务
/// 执行任务不能为死循环
/// </summary>
#define THREADPOOL_MAX_NUM 4

class ThreadPool
{
protected:
    using Task = function<void()>;	//定义类型
    vector<thread> _pool;		    //线程池
    queue<Task> _tasks;             //任务队列
    mutex _lock;                    //同步
    condition_variable _task_cv;    //条件阻塞
    atomic<bool> _run{ true };      //线程池是否执行
    atomic<int>  _idlThrNum{ 0 };   //空闲线程数量

public:

    inline ThreadPool(unsigned short size = 4) {
        addThread(size);
    }

    inline ~ThreadPool()
    {
        _run = false;
        _task_cv.notify_all(); // 唤醒所有线程执行
        for (thread& thread : _pool) {
            //thread.detach(); // 让线程“自生自灭”
            if (thread.joinable())
                thread.join(); // 等待任务结束， 前提：线程一定会执行完
        }
    }

public:
    // 提交一个任务
    // 调用.get()获取返回值会等待任务执行完,获取返回值
    // 有两种方法可以实现调用类成员，
    // 一种是使用   bind： .commit(std::bind(&Dog::sayHello, &dog));
    // 一种是用   mem_fn： .commit(std::mem_fn(&Dog::sayHello), this)
    template<class F, class... Args>
    auto commit(F&& f, Args&&... args) ->future<decltype(f(args...))>
    {
        if (!_run)    // stoped ??
            throw runtime_error("commit on ThreadPool is stopped.");

        using RetType = decltype(f(args...)); // typename std::result_of<F(Args...)>::type, 函数 f 的返回值类型
        auto task = make_shared<packaged_task<RetType()>>(
                bind(forward<F>(f), forward<Args>(args)...)
        ); // 把函数入口及参数,打包(绑定)
        future<RetType> future = task->get_future();
        {    // 添加任务到队列
            lock_guard<mutex> lock{ _lock };//对当前块的语句加锁  lock_guard 是 mutex 的 stack 封装类，构造的时候 lock()，析构的时候 unlock()
            _tasks.emplace([task]() { // push(Task{...}) 放到队列后面
                (*task)();
            });
        }
#ifdef THREADPOOL_AUTO_GROW
        if (_idlThrNum < 1 && _pool.size() < THREADPOOL_MAX_NUM)
			addThread(1);
#endif // !THREADPOOL_AUTO_GROW
        _task_cv.notify_one(); // 唤醒一个线程执行

        return future;
    }

    //空闲线程数量
    int idlCount() { return _idlThrNum; }
    //线程数量
    int thrCount() { return _pool.size(); }
#ifndef THREADPOOL_AUTO_GROW
private:
#endif // !THREADPOOL_AUTO_GROW
    //添加指定数量的线程
    void addThread(unsigned short size)
    {
        for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0; --size)
        {   //增加线程数量,但不超过 预定义数量 THREADPOOL_MAX_NUM
            _pool.emplace_back([this] { //工作线程函数
                while (_run)
                {
                    Task task; // 获取一个待执行的 task
                    {
                        // unique_lock 相比 lock_guard 的好处是：可以随时 unlock() 和 lock()
                        unique_lock<mutex> lock{ _lock };
                        _task_cv.wait(lock, [this] {
                            return !_run || !_tasks.empty();
                        }); // wait 直到有 task
                        if (!_run && _tasks.empty())
                            return;
                        task = move(_tasks.front()); // 按先进先出从队列取一个 task
                        _tasks.pop();
                    }
                    _idlThrNum--;
                    task();//执行任务
                    _idlThrNum++;
                }
            });
            _idlThrNum++;
        }
    }
};

namespace ShareMem{

    //共享内存-数据结构
    struct ShareData{
        // head - 一些固定不变的常量
        pid_t host_pid = 0;        // 主进程pid
        pid_t slave_pid = 0;       // 从进程pid
        unsigned long rows = 640;   // 图像高
        unsigned long cols = 640;   // 图像宽

        // body - 需要频繁传输的/大容量的数据
        int status = -1;                          // 标志位
        unsigned long data_size = 0;            // 数据实际大小，用于确定读取量
        char data_body[SHARE_MEMORY_SIZE]{};      // 共享内存里的数据体
        char response[1024]{};                    // 响应数据

        long frame = 0;                         // 帧数
    };

    class ShareMemory{
    private:
        key_t share_memory_key = 0;     //共享内存地址标识
        int share_memory_id = 0;    // 共享内存id
        void *share_memory_address = nullptr;   // 映射共享内存地址  share_memory_address指针记录了起始地址
        ShareData *p_share_data = nullptr;  // 以ShareData结构体类型-访问共享内存

        //未来加速 可以搞一个图像队列 队列大小3  不停存图，然后挨着丢进共享内存，满了就清除。
        //vector<ShareData> share_data_queue;

        ThreadPool pool;
    public:

        /*!
         * @brief 主进程构造类并创建共享内存
         * @param key 共享内存地址标识
         */
        explicit ShareMemory(key_t share_key);

        /*!
         * @brief 构造一个空类
         * @details 由于Python使用C extension打包，无法动态创建
         * 含参数的类，因此提供此构造函数，用于创建空类。Python可在
         * 后续调用CreateShare完成共享内存的创建
         */
        explicit ShareMemory(char PyUseOnly);

        ~ShareMemory();

        /*!
         * @brief 从进程调用写入从进程的pid
         * @param slave_pid 从进程传入自身的pid
         * @return
         */
        int RegisterSlavePid(key_t slave_pid);

        /*!
         * @brief 销毁共享内存
         * @return
         */
        int DestroyShare() const;

        /*!
         * @brief 调用从进程对数据进行计算，返回数据结果
         * @return
         */
        std::string CallSlave();

        /*!
         * @brief 将数据写入共享内存,传入要写入数据的指针，数据大小，偏移量，是否使用多线程。
         * @param data_ptr 要写入的数据的地址
         * @param data_size 要发送的数据大小
         * @param offset 偏移量，便于分块存入数据
         * @param multi_thread 使用多线程对数据进行写入
         * @return 1成功 0失败
         * @details 通过设置偏移量可以决定数据写入的位置，以支持多组数据分块写入。写入支持多
         * 线程并行，通过设置multi_threads决定使用的线程数量。
         */
        int WriteData(u_char *data_ptr, unsigned long data_size, size_t offset = 0, int multi_threads = 1) const;

        /*!
         * @brief 设置共享内存的 head 部分
         * @details head 部分为一些不常修改的数据，比如图像的宽、高等
         * 可以用于包含一些用于告知接收端的"协议性"信息
         * @return
         */
        int SetShareHead(unsigned long rows, unsigned long cols) const;

        /*!
         * @brief 获取数据体的实际大小
         * @return 数据体的实际大小，失败则返回0
         */
        unsigned long GetShareBodySize() const;

        /*!
         * @brief 获取共享内存数据的指针
         * @details 这个函数仅获取共享内存中data_body的指针，不会改变flag的值。
         * @return 共享内存数据的指针
         */
        u_char * GetShareBodyAddress() const;

        /*!
         * @brief 向内存写入结果
         */
        int WriteResult(u_char *result_ptr) const;

        /*!
         * @brief 设置共享内存的读写状态
         * @details CAN_READ （1）可读；CAN_WRITE 可写（0）；
         * @param value 0 为可写，1为可读
         * @return
         */
        int SetStatus(int value) const;

        /*!
         * @brief 获取共享内存的指针
         * @details 该函数可以直接获得共享内存地址，越权操作共享内存里的内容
         * 通常来说不建议使用，因为可能导致内存溢出等灾难性的问题
         * @return
         */
        ShareData *ShareMemoryPtr() const;

        /*!
         * @brief 创建共享内存
         * @param share_key 共享内存地址标识
         * @return
         */
        int CreateShare(key_t share_key);

    public:
        void copy_data(char *start_ptr, unsigned long data_size, u_char *data_ptr);
    };

}



#endif //CPP_PYTHON_SHARE_MEMORY_H
