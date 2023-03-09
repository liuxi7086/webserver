#ifndef LST_TIMER
#define LST_TIMER

#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>


class util_timer;  // 定时器类包括连接资源、定时事件和超时时间（由于连接资源结构体中需要用到定时器类，所以在这里前向声明）

// 连接资源结构体包括客户端套接字地址、文件描述符和定时器
struct client_data {
    sockaddr_in address;  // 客户端套接字地址
    int sockfd;  // socket文件描述符
    util_timer* timer;  // 定时器类
};

// 定时器类包括连接资源、定时事件（回调函数）和超时时间
class util_timer {
public:
    util_timer() : prev(NULL), next(NULL) {}  // 列表初始化构造函数
public:
    client_data* user_data;  // 连接资源
    void (*cb_func)(client_data*);  // 任务回调函数
    time_t expire;  // 任务超时时间（绝对时间）
    util_timer* prev;  // 前向定时器
    util_timer* next;  // 后继定时器
};

// 定时器链表（有头结点和尾结点的升序双向链表）
class sort_timer_lst {
public:
    sort_timer_lst() : head(NULL), tail(NULL) {}  // 列表初始化构造函数
    
    // 链表析构（释放链表资源）
    ~sort_timer_lst() {
        util_timer* tmp = head;
        while (tmp) {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    // 将目标定时器按升序添加到链表中（会调用私有函数add_timer）
    void add_timer(util_timer* timer) {
        // 若timer不存在则直接返回
        if (!timer) return ;  
        
        // 若链表为空则直接插入
        if (!head) {  
            head = tail = timer;
            return;
        }

        // 若新的定时器超时时间小于当前头部结点，则直接将当前定时器结点作为头部结点
        if (timer->expire < head->expire) {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return ;
        }

        // 除此之外，调用私有函数add_timer调整内部结点
        add_timer(timer, head);
    }

    // 客户端在设定时间内有数据收发，则当前时刻对该定时器重新设定时间，即延长超时时间，并调整定时器在链表中的位置
    void adjust_timer(util_timer* timer) {
        // 若timer不存在则直接返回
        if (!timer) return ;
        
        // 若调整的结点在链表尾部或定时器新的超时时间仍小于下一个定时器的超时时间则不用调整
        util_timer* tmp = timer->next;
        if (!tmp || (timer->expire < tmp->expire)) return ;

        // 若调整的结点是链表头结点，则将定时器取出并重新插入
        if (timer == head) {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        } else {  // 若调整的结点在链表内部，则将定时器取出并重新插入
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, head);
        }
    }

    // 删除定时器
    void del_timer(util_timer* timer) {
        // 若timer不存在则直接返回 
        if (!timer) return ;

        // 若链表中只有一个要删除的定时器timer
        if ((timer == head) && (timer == tail)) {
            delete timer;
            head = NULL;
            tail = NULL;
            return ;
        }

        // 若被删除的timer是头结点
        if (timer == head) {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return ;
        }

        // 若被删除的timer是尾结点
        if (timer == tail) {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return ;
        }

        // 被删除的结点在链表内部
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    // 定时任务处理函数
    void tick() {
        if (!head) return ;
        // 获取当前时间
        time_t cur = time(NULL);
        util_timer* tmp = head;
        // 遍历定时器链表
        while (tmp) {
            // 若当前时间小于定时器的超时时间，则当前时间一定小于后面定时器的超时时间
            if (cur < tmp->expire) break;

            // 当前定时器到期，调用回调函数
            tmp->cb_func(tmp->user_data);
            // 将处理后的定时器从链表删除，并重置头结点
            head = tmp->next;
            if (head) head->prev = NULL;
            delete tmp;
            tmp = head;
        }
    }

private:
    // 用于调整链表内部结点（公共add_timer函数的重载，是私有成员函数，被共有add_timer和adjust_time调用）
    void add_timer(util_timer* timer, util_timer* lst_head) {
        util_timer* prev = lst_head;
        util_timer* tmp = prev->next;
        
        // 遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置
        while (tmp) {
            if (timer->expire < tmp->expire) {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = prev->next;
        }

        // 若遍历完后，发现timer需要放在尾结点处
        if (!tmp) {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

private:
    util_timer* head;  // 头结点
    util_timer* tail;  // 尾结点
};


#endif