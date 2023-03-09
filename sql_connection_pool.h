#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <string>
#include <mysql/mysql.h>
#include <list>

#include "lock.h"

using namespace std;

class connection_pool {
public:
    // 局部静态变量单例模式
    static connection_pool* GetInstance();

    // 构造初始化（主机地址、数据库用户名、数据库登录密码、数据库名、数据库端口号、最大连接数）
    void init(string url, string User, string PassWord, string DatabaseName, int Port, unsigned int MaxConn);

    // 获取数据库连接
    MYSQL* GetConnection();

    // 释放连接
    bool ReleaseConnection(MYSQL* conn);

    // 销毁连接池
    void DestroyPool();

    // 获取当前空闲连接数
    int GetFreeConn();

    connection_pool();
    ~connection_pool();
private:

    string url;  // 主机地址
    string Port;  // 数据库端口号
    string User;  // 登录数据库用户名
    string PassWord;  // 登录数据库密码
    string DatabaseName;  // 使用数据库名
    unsigned int MaxConn;  // 最大的连接数
    unsigned int CurConn; // 当前已使用的连接数
    unsigned int FreeConn;  // 当前空闲的连接数

    mutex lock;  // 互斥锁
    list<MYSQL*> connList;  // 连接池
    sem reserve;  // 信号量
};


// 将数据库连接的获取与释放通过RAII机制封装，避免手动释放
class connectionRAII {
public:
    // 有参构造对传入参数进行修改，由于数据库连接本身是指针类型，所以参数需要通过双指针进行修改
    connectionRAII(MYSQL** con, connection_pool* connPool);
    ~connectionRAII();

private:
    MYSQL* conRAII;
    connection_pool* poolRAII;
};


#endif