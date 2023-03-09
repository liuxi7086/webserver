#include <iostream>

#include "sql_connection_pool.h"

// 构造函数
connection_pool::connection_pool() {
    this->CurConn = 0;
    this->FreeConn = 0;
}

// 析构函数
connection_pool::~connection_pool() {
    DestroyPool();
}

// 单例模式
connection_pool* connection_pool::GetInstance() {
    static connection_pool connPool;
    return &connPool;
}

// 初始化连接池
void connection_pool::init(string url, string User, string PassWord, string DatabaseName, int Port, unsigned int MaxConn) {
    // 初始化数据信息
    this->url = url;
    this->Port = Port;
    this->User = User;
    this->PassWord = PassWord;
    this->DatabaseName = DatabaseName;

    lock.lock();
    // 创建MaxConn条数据库连接
    for (int i = 0; i < MaxConn; i++) {
        
        // 分配或者初始化一个MYSQL对象，用于连接mysql服务端
        MYSQL* con = NULL;
        con = mysql_init(con);  
        if (con == NULL) {
            cout << "Error:" << mysql_error(con);
            exit(-1);
        }

        // 连接MYSQL数据库
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DatabaseName.c_str(), Port, NULL, 0);
        if (con == NULL) {
            cout << "Error:" << mysql_error(con);
        }
        connList.push_back(con);
        ++FreeConn;
    }
    reserve = sem(FreeConn);
    this->MaxConn = FreeConn;
    lock.unlock();
}

// 当有请求时，从数据库连接池中返回一个可用连接
MYSQL* connection_pool::GetConnection() {
    MYSQL* con = NULL;
    if (0 == connList.size()) return NULL;

    // 取出连接，信号量减1，若信号量为0则等待
    reserve.wait();

    lock.lock();
    con = connList.front();  // 从链表头端取出一个连接
    connList.pop_front();
    --FreeConn;
    ++CurConn;
    lock.unlock();
    
    return con;
}

// 释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL* con) {
    if (NULL == con) return false;

    lock.lock();
    connList.push_back(con);
    ++FreeConn;
    --CurConn;
    lock.unlock();

    reserve.post();  // 信号量加1
    return true;
}

// 销毁连接池
void connection_pool::DestroyPool() {
    lock.lock();
    if (connList.size() > 0) {
        // 利用迭代器关闭数据库连接
        list<MYSQL*>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it) {
            MYSQL* con = *it;
            mysql_close(con);
        }
        CurConn = 0;
        FreeConn = 0;

        // 清空链表
        connList.clear();

        lock.unlock();
    }
    lock.unlock();
}

// 获取当前的空闲连接数
int connection_pool::GetFreeConn() {
    return this->FreeConn;
}

connectionRAII::connectionRAII(MYSQL** SQL, connection_pool* connPool) {
    *SQL = connPool->GetConnection();
    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII() {
    poolRAII->ReleaseConnection(conRAII);
}