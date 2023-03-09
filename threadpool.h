#ifndef THREADPOOL_H
#define THREADPOOL_H
<<<<<<< HEAD
=======
#include <pthread.h>
>>>>>>> bf7723c55c92d67fd49ab6503b8fc273c0bdb8fd
#include <list>
#include <cstdio>

#include "lock.h"
#include "sql_connection_pool.h"

// 线程池定义为模板类，实现代码复用，其中T是任务类
template <typename T>
class threadpool {
public:
    threadpool(connection_pool* connPool, int thread_number = 8, int max_requests = 10000);  // 构造函数，默认创建8个线程，最大的请求数量是10000
    ~threadpool();  // 析构函数
    bool append(T* request);  // 将任务添加到请求队列

private:
    static void* worker(void* arg);  // 线程处理函数
    void run();  // run执行任务，与worker分开写，因为run中使用了大量的类内成员，与worker不分开写就得写大量的pool->

private:
    int m_thread_number;  // 线程数量
    pthread_t* m_threads;  // 线程池数组，大小为m_thread_number
    int m_max_requests;  // 请求队列最多允许的请求数量
    std::list<T*> m_workqueue;  // 请求队列
    mutex m_queuelocker;  // 队列的互斥锁
    sem m_queuestate;  // 信号量用来判断是否有任务需要处理
    bool m_stop;  // 是否结束线程
    connection_pool* m_connPoll;  // 数据库
};

template <typename T>
threadpool<T>::threadpool(connection_pool* connPool, int thread_number, int max_requests) : 
    m_connPoll(connPool), m_thread_number(thread_number), m_max_requests(max_requests), 
    m_threads(NULL), m_stop(false) {  // 列表初始化

    if (thread_number <= 0 || max_requests <= 0) throw std::exception();  // 如果输入参数不满足要求则抛出异常
    m_threads = new pthread_t[m_thread_number];  // 动态创建线程数组
    if (!m_threads) throw std::exception();  // 如果创建失败则抛出异常
    
    // 创建thread_number个线程并设置线程分离
    for (int i = 0; i < m_thread_number; ++i) {
        printf("create the %dth thread\n", i);

        // 创建线程，如果创建失败则删除数组并抛出异常，第四个参数出入this，因为第三个参数worker是静态成员函数不能访问非静态成员变量
        if (pthread_create(m_threads + i, NULL, worker, this) != 0) {  
            delete[] m_threads;
            throw std::exception();
        }

        // 设置线程分离，如果分离失败则删除数组并抛出异常
        if (pthread_detach(m_threads[i])) {
            delete[] m_threads;
            throw std::exception();
        }  
    }
}

template <typename T>
threadpool<T>::~threadpool() {
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool threadpool<T>::append(T* request) {
    m_queuelocker.lock();  // 队列上锁

    // 如果当前工作队列已满，则队列解锁并退出程序
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }

    // 将任务添加到当前队列
    m_workqueue.push_back(request);  // 尾插
    m_queuelocker.unlock();  // 队列解锁
    m_queuestate.post();  // 队列信号量加1
    return true;  // 退出程序
}

template <typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*) arg;  // 将arg强转为线程池类以便调用成员方法
    pool->run();  // 运行任务
    return pool;
}

template <typename T>
void threadpool<T>::run() {
    while (!m_stop) {
        m_queuestate.wait();  // 信号量减1
        m_queuelocker.lock();  // 队列加锁

        // 如果工作队列为空，队列解锁，并循环访问
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }

        // 工作队列不为空，则从队头取任务并处理
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) continue;

        // 数据库
        connectionRAII mysqlcon(&request->mysql, m_connPoll);

        request->process();
    }
}
    
#endif