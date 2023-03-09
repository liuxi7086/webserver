#ifndef LOCK_H
#define LOCK_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 互斥锁
class mutex {
public:
    mutex() {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) throw std::exception();  // 始化互斥锁
    }
    ~mutex() {
        pthread_mutex_destroy(&m_mutex);  // 销毁互斥锁
    }
    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;  // 互斥锁加锁
    }
    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;  // 互斥锁解锁
    }
    pthread_mutex_t* get() {
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
};

// 条件变量
class cond {
public:
    cond() {
        if (pthread_cond_init(&m_cond, NULL) != 0) throw std::exception();  // 初始化条件变量
    }
    ~cond() {
        pthread_cond_destroy(&m_cond);  // 销毁条件变量
    }
    bool wait(pthread_mutex_t* mutex) {
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, mutex);
        return ret == 0;
    }
    bool timedwait(pthread_mutex_t* mutex, struct timespec t) {
        int ret = 0;
        /* 
            等待条件变量，该函数调用时需要传入mutex（加锁的互斥锁），函数执行时，
            先把调用线程放入条件变量的请求队列，然后将互斥锁mutex解锁，当函数成功返回为0时，
            互斥锁会再次被锁上. 即函数内部会有一次解锁和加锁操作 
        */
        ret = pthread_cond_timedwait(&m_cond, mutex, &t);
        return ret == 0;
    }
    bool signal() {
        return pthread_cond_signal(&m_cond);  // 唤醒一个或者多个等待的线程
    }
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond);  // 唤醒所有的等待的线程
    }
private:
    pthread_cond_t m_cond;
};

// 信号量
class sem {
public:
    sem() { 
        if (sem_init(&m_sem, 0, 8) != 0) throw std::exception();  // 初始化信号量（无参构造）
    }
    sem(int num) {
        if (sem_init(&m_sem, 0, num) != 0) throw std::exception();  // 初始化信号量（有参构造，可以传入信号量的值）
    }
    ~sem() {
        sem_destroy(&m_sem) != 0;  // 销毁信号量
    }
    bool wait() {
        return sem_wait(&m_sem) == 0;  // 信号量加锁，调用一次对信号量值-1，若值等于0，则阻塞（相当于P）
    }
    bool post() {
        return sem_post(&m_sem) == 0;  // 信号量解锁，调用一次对信号量的值+1，唤醒调用sem_post的线程（相当于V）
    }
private:
    sem_t m_sem;
};

#endif