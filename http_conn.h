#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <string.h>  // 提供bzero函数
#include <sys/types.h>
#include <sys/stat.h>  // 提供stat结构体和函数
#include <sys/mman.h>  // 提供mmap函数
#include <stdarg.h>  // 提供va_list宏
#include <sys/uio.h>  // 提供writev函数
#include <map>

#include "sql_connection_pool.h"
class http_conn {
public:

    static int m_epollfd;  // 所有socket上的事件都被注册到同一个epoll
    static int m_user_count;  // 统计用户数量
    static const int READ_BUFFER_SIZE = 2048;  // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;  // 写缓冲区的大小
    static const int FILENAME_LEN = 200;  // 读取文件名称m_read_file大小

    MYSQL* mysql;  // 数据库
    map<string, string> users;  // 用户名（key）和密码（value）
    mutex m_lock;

    // http报文请求方法（声明METHOD为新的数据类型，称为枚举，里面的GET，POST...称为枚举量，其值默认分别为0，1，...）
    enum METHOD {
        GET = 0,  // GET请求
        POST,  // POST请求
        HEAD, 
        PUT, 
        DELETE, 
        TRACE, 
        OPTIONS, 
        CONNECT, 
        PATH
    };  

    // 主状态机三种状态
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0,  // 当前正在分析请求行
        CHECK_STATE_HEADER,  // 当前正在分析头部字段
        CHECK_STATE_CONTENT,  // 当前正在解析请求体
    };

    // 从状态机三种状态（解析某行的读取状态）
    enum LINE_STATUS {
        LINE_OK = 0,  // 读取到一个完整行
        LINE_BAD,  // 行出错
        LINE_OPEN  // 行数据不完整
    };

    // 报文解析结果
    enum HTTP_CODE {
        NO_REQUEST,  // 表示请求不完整，需要继续读取客户数据
        GET_REQUEST,  // 表示获得了一个完整的客户请求
        BAD_REQUEST,  // 表示客户请求语法错误
        NO_RESOURCE,  // 表示服务器没有资源
        FORBIDDEN_REQUEST,  // 表示客户对资源没有足够的权限访问
        FILE_REQUEST,  // 表示文件请求，获取文件成功
        INTERNAL_ERROR,  // 表示服务器内部错误
        CLOSED_CONNECTION  // 表示客户端已经关闭连接
    };

    http_conn() {}  // 构造函数
    ~http_conn() {}  // 析构函数

public:
    void process();  // 处理客户端请求
    void init(int sockfd, const sockaddr_in& addr);  // 初始化套接字地址，函数内部会调用私有方法init
    void close_conn(bool real_close = true);  // 关闭连接
    bool read();  // 非阻塞读
    bool write();  // 非阻塞写

    // 为外部获取通信socket地址提供接口
    sockaddr_in* get_address() {
        return &m_address;
    }

    // 载入数据库表
    void initmysql_result(connection_pool* connPool);

private:
    void init();  // 初始化连接其他信息
    HTTP_CODE process_read();  // 从m_read_buf读取，并处理请求报文
    bool process_write(HTTP_CODE ret);  // 向m_write_buf写入响应报文数据

    // 以下函数被process_read函数调用以分析http请求
    LINE_STATUS parse_line();  // 从状态机读取一行，分析是请求报文的哪一部分
    char* get_line() { return m_read_buf + m_start_line; }  // 获取一行数据，m_start_line是已经解析的字符，get_line用于将指针向后偏移，指向未处理的字符
    HTTP_CODE parse_request_line(char* text);  // 主状态机解析请求报文中的请求行
    HTTP_CODE parse_headers(char* text);  // 主状态机解析请求报文中的请求头
    HTTP_CODE parse_content(char* text);  // 主状态机解析请求报文中的请求体
    HTTP_CODE do_request();  // 生成响应报文

    // 以下函数被process_write函数调用（根据响应报文格式，生成对应函数）
    bool add_response(const char* format, ...);  // 每次添加到写缓存区时进行判断（在声明不肯定形参的函数时，形参部分可使用省略号"..."代替）
    bool add_status_line(int status, const char* title);  // 添加状态行
    bool add_headers(int content_len);  // 添加消息报头，具体添加文本长度、文本类型、连接状态和空行
    bool add_content_length(int content_len);  // 添加Content-Length，表示响应报文的长度
    bool add_content_type();  // 添加文本类型
    bool add_linger();  // 添加连接状态，通知浏览器时保持连接还是关闭连接
    bool add_blank_line();  // 添加空行
    bool add_content(const char* content);  // 添加文本content

    void unmap();  // 关闭内存映射 

private:
    int m_sockfd;  // 该http连接的socket
    sockaddr_in m_address;  // 通信的socket地址

    CHECK_STATE m_check_state;  // 主状态机当前所处的状态
    METHOD m_method;  // 请求方法

    // 存储读取的请求报文数据
    char m_read_buf[READ_BUFFER_SIZE];  // 读缓冲区
    int m_read_idx;  // 读缓冲区m_read_buf中已经读入的客户端数据的最后一个字节的下一个位置
    int m_checked_idx;  // 当前正在分析的字符在读缓冲的位置
    int m_start_line;  // 当前正在解析的行在buf中的起始位置，将该位置后面的数据赋给text
    
    // 存储发出的响应报文数据
    char m_write_buf[WRITE_BUFFER_SIZE];  // 写缓冲区
    int m_write_idx;  // 指示buffer中的长度

    // 以下为解析请求报文中对应的变量
    char* m_url;  // 请求目标文件的文件名
    char* m_version;  // 协议版本
    char* m_host;  // 主机名
    bool m_linger;  // 判断http请求是否保持连接
    int m_content_length;  // 请求体长度
    char m_read_file[FILENAME_LEN];  // 存储读取文件名称

    // 操作m_read_file相关变量
    struct stat m_file_stat;  // m_read_file的文件属性（stat函数的传出参数），stat结构体中有st_mode（文件类型和权限），st_size（文件大小，字节数）
    char* m_file_address;  // 读服务器上的文件地址（m_read_file内存映射地址）
    
    char* m_string;  // 存储请求数据
    int cgi;  // 是否启用的POST
    struct iovec m_iv[2];  // io向量机制iovec，里面有两个元素，指针成员iov_base指向一个缓冲区，存放的是writev将要发送的数据，成员iov_len表示实际写入的长度，该变量用于writev函数
    int m_iv_count;  // 结构体的个数，几块内存
    int bytes_to_send;  // 剩余发送字节数
    int bytes_have_send;  // 已发送字节数

};

#endif