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
#include <functional>

using namespace std;

#define SHARE_MEMORY_SIZE (640*640*3*100)  // 共享内存最大容量
#define RESPONSE_SIZE (1024*20)

#define READY 2         // 准备好进行新一轮的数据传输
#define WORKING 3       // 正在处理数据中
#define JOB_DONE 4    // 结果在内存中

#define WORK_IT_OUT SIGUSR1 // 通知从进程开始计算

namespace ThreadsPool{
    // 安全队列（便于多线程操作）
    template <typename T>
    class SafeQueue{
    private:
        std::queue<T> m_queue; //利用模板函数构造队列
        std::mutex m_mutex; // 访问互斥信号量

    public:
        SafeQueue() = default;
        SafeQueue(SafeQueue &&other)  noexcept {}
        ~SafeQueue() = default;

        // 返回队列是否为空
        bool empty(){
            std::unique_lock<std::mutex> lock(m_mutex); // 互斥信号变量加锁，防止m_queue被改变
            return m_queue.empty();
        }

        int size(){
            std::unique_lock<std::mutex> lock(m_mutex); // 互斥信号变量加锁，防止m_queue被改变
            return m_queue.size();
        }

        // 队列添加元素
        void enqueue(T &t){
            std::unique_lock<std::mutex> lock(m_mutex);
            m_queue.emplace(t);
        }

        // 队列取出元素
        bool dequeue(T &t){
            std::unique_lock<std::mutex> lock(m_mutex); // 队列加锁
            if (m_queue.empty())
                return false;
            t = std::move(m_queue.front()); // 取出队首元素，返回队首元素值，并进行右值引用
            m_queue.pop(); // 弹出入队的第一个元素
            return true;
        }
    };

    class ThreadsPool{
    public:
        // 线程池构造函数
        explicit ThreadsPool(const int n_threads = 4)
                : m_threads(std::vector<std::thread>(n_threads)), m_shutdown(false){
        }
        ThreadsPool(const ThreadsPool &) = delete;
        ThreadsPool(ThreadsPool &&) = delete;
        ThreadsPool &operator=(const ThreadsPool &) = delete;
        ThreadsPool &operator=(ThreadsPool &&) = delete;

        // Inits thread pool
        void init(){
            for (int i = 0; i < m_threads.size(); ++i)
            {
                m_threads.at(i) = std::thread(ThreadWorker(this, i)); // 分配工作线程
            }
        }

        // Waits until threads finish their current task and shutdowns the pool
        void shutdown(){
            m_shutdown = true;
            m_conditional_lock.notify_all(); // 通知，唤醒所有工作线程
            for (auto & m_thread : m_threads){
                // 判断线程是否在等待
                if (m_thread.joinable()){
                    m_thread.join(); // 将线程加入到等待队列
                }
            }
        }

        // Submit a function to be executed asynchronously by the pool
        template <typename F, typename... Args>
        auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))>{
            // Create a function with bounded parameter ready to execute
            // 连接函数和参数定义，特殊函数类型，避免左右值错误
            std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            // Encapsulate it into a shared pointer in order to be able to copy construct
            auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);
            // Warp packaged task into void function
            std::function<void()> warpper_func = [task_ptr](){
                (*task_ptr)();
            };
            // 队列通用安全封包函数，并压入安全队列
            m_queue.enqueue(warpper_func);
            // 唤醒一个等待中的线程
            m_conditional_lock.notify_one();
            // 返回先前注册的任务指针
            return task_ptr->get_future();
        }
    private:
        SafeQueue<std::function<void()>> m_queue; // 安全队列
        std::condition_variable m_conditional_lock; // 条件变量
        bool m_shutdown; // 线程池是否关闭
        std::vector<std::thread> m_threads; // 工作线程队列
        std::mutex m_conditional_mutex; // 线程休眠锁互斥变量

        // 内置线程工作类
        class ThreadWorker{
        private:
            int m_id; // 工作id
            ThreadsPool *m_pool; // 所属线程池
        public:
            // 构造函数
            ThreadWorker(ThreadsPool *pool, const int id) : m_pool(pool), m_id(id){}
            // 重载()操作
            void operator()(){
                std::function<void()> func; // 定义基础函数类func
                bool dequeued; // 是否正在取出队列中元素
                while (!m_pool->m_shutdown){
                    {
                        // 为线程环境加锁，互访问工作线程的休眠和唤醒
                        std::unique_lock<std::mutex> lock(m_pool->m_conditional_mutex);
                        // 如果任务队列为空，阻塞当前线程
                        if (m_pool->m_queue.empty()){
                            m_pool->m_conditional_lock.wait(lock); // 等待条件变量通知，开启线程
                        }
                        // 取出任务队列中的元素
                        dequeued = m_pool->m_queue.dequeue(func);
                    }
                    // 如果成功取出，执行工作函数
                    if (dequeued)
                        func();
                }
            }
        };
    };

}

namespace ShareMem{

    //共享内存-数据结构
    struct ShareData{
        /*! head - 一些固定不变的常量
         * 可以在这里加一些"协议性"的数据
         */
        unsigned long rows = 0;   // 图像高
        unsigned long cols = 0;   // 图像宽
        unsigned long channels = 0; // 图像通道数

        // body - 需要频繁传输的/大容量的数据
        int status = -1;                          // 标志位
        unsigned long data_size = 0;            // 数据实际大小，用于确定读取量
        char data_body[SHARE_MEMORY_SIZE];      // 共享内存里的数据体
        char response[RESPONSE_SIZE];                    // 响应数据

        // pid信息
        pid_t host_pid = 0;        // 主进程pid
        pid_t slave_pid = 0;       // 从进程pid
    };

    class ShareMemory{

    public:
        /*!
         * @brief 设置共享内存的 head 部分
         * @details head 部分为一些不常修改的数据，比如图像的宽、高、通道等
         * 可以改写此函数里的一些信息，用于包含一些用于告知接收端的"协议性"信息
         * @return
         */
        int SetShareHead(unsigned long rows, unsigned long cols, unsigned long channels) const {
            p_share_data->rows = rows;
            p_share_data->cols = cols;
            p_share_data->channels = channels;
            return 0;
        }

    private:
        key_t share_memory_key = 0;     //共享内存地址标识
        int share_memory_id = 0;    // 共享内存id
        void *share_memory_address = nullptr;   // 映射共享内存地址  share_memory_address指针记录了起始地址
        ShareData *p_share_data = nullptr;  // 以ShareData结构体类型-访问共享内存

        //未来加速 可以搞一个图像队列 队列大小3  不停存图，然后挨着丢进共享内存，满了就清除。
        //vector<ShareData> share_data_queue;

        ThreadsPool::ThreadsPool pool;
        vector<std::future<int>> write_task_list;
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
        int DestroyShare();

        /*!
         * @brief 调用从进程对数据进行计算，返回数据结果
         * @return string 类型的响应数据
         */
        std::string CallSlave();

        /*!
         * @brief 将数据写入共享内存,传入要写入数据的指针，数据大小，偏移量，是否使用多线程。
         * @param data_ptr 要写入的数据源的地址
         * @param data_size 要发送的数据大小
         * @param offset 写入位置相对于共享内存地址的偏移量，便于分块存入数据
         * @param multi_thread 使用多线程对数据进行写入，最大4个线程
         * @return 1成功 0失败
         * @details 通过设置偏移量可以决定数据写入的位置，以支持多组数据分块写入。写入支持多
         * 线程并行，通过设置multi_threads决定使用的线程数量。
         */
        int WriteData(u_char *data_ptr, unsigned long data_size, size_t offset = 0, int multi_threads = 1);

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

        /*!
         * @brief 向内存拷贝数据
         * @return
         */
        static int copy_data(char *start_ptr, unsigned long data_size, u_char *data_ptr);
    };

}



#endif //CPP_PYTHON_SHARE_MEMORY_H
