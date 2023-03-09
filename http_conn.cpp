#include <fstream>
#include "http_conn.h"
#include "log.h"

// #define listenfdLT // 设置监听文件描述符为水平触发模式
#define listenfdET  // 设置监听文件描述符为边缘触发模式

// #define connfdLT // 设置连接文件描述符为水平触发模式
#define connfdET  // 设置连接文件描述符为边缘触发模式

// http状态码
const char* ok_200_title = "OK";  // 请求成功
const char* error_400_title = "Bad Request";  // 错误请求
const char* error_400_form = "Your request has bad syntax or is inherently impossible to statisfy.\n";  // 您的请求有错误的语法或者根本不可能被满足
const char* error_403_title = "Forbidden";  // 禁止请求
const char* error_403_form = "You do not have permission to get file from this server.\n";  // 您没有从此服务器获取文件的权限
const char* error_404_title = "Not Found";  // 没有找到资源
const char* error_404_form = "The request file was not found on this server.\n";  // 在此服务器上找不到请求文件
const char* error_500_title = "Internal Error";  // 内部错误
const char* error_500_form = "There was an unusual problem serving the request file.\n";  // 在处理请求文件时出现了一个不寻常的问题

// 静态成员变量需要初始化
int http_conn::m_epollfd = -1;  // 所有socket上的事件都被注册到同一个epoll
int http_conn::m_user_count = 0;  // 统计用户数量

// 网站根目录，文件中存放请求的资源和跳转的html文件
<<<<<<< HEAD
const char* doc_root = "/home/chaopro/webServer/myWebServer/root";  
=======
const char* doc_root = "/home/chaopro/TinyWebServer/myWebServer/root";  
>>>>>>> bf7723c55c92d67fd49ab6503b8fc273c0bdb8fd

/*------------文件描述符操作----------*/
// 设置文件描述符非阻塞
void setnonblocking(int fd) {
    // 获取文件描述符文件状态flag
    int old_flag = fcntl(fd, F_GETFL); 
    // 添加非阻塞 
    int new_flag = old_flag | O_NONBLOCK;  
    // 设置文件描述符文件状态flag
    fcntl(fd, F_SETFL, new_flag);  
}

// 添加文件描述符到epoll
int addfd(int epollfd, int fd, bool one_shot) {
    // 事件设置
    epoll_event event;
    event.data.fd = fd;

#ifdef listenfdLT
    event.events = EPOLLIN | EPOLLRDHUP;  // 默认是水平触发，EPOLLIN表示对应的文件描述符可以读（包括对端SOCKET正常关闭），EPOLLHUP表示对应的文件描述符被挂断
#endif

#ifdef listenfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

<<<<<<< HEAD
    if (one_shot) event.events |= EPOLLONESHOT;  // 注册epolloneshot事件，一个线程处理socket时，其他线程将无法处理（listenfd不用开启）
=======
    if (one_shot) event.events |= EPOLLONESHOT;  // 注册epolloneshot事件，一个线程处理socket时，其他线程将无法处理
>>>>>>> bf7723c55c92d67fd49ab6503b8fc273c0bdb8fd
    
    // 注册内核事件表监控的文件描述符上的事件
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);  
    
<<<<<<< HEAD
    // 设置文件描述符非阻塞（ET模式只支持非阻塞）
=======
    // 设置文件描述符非阻塞（因为ET模式只支持非阻塞）
>>>>>>> bf7723c55c92d67fd49ab6503b8fc273c0bdb8fd
    setnonblocking(fd);  
}

// 从epoll中删除文件描述符
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);  // 删除内核事件表监控的文件描述符上的事件
    close(fd);
}

// 修改文件描述符到epoll，重置socket上的EPOLLPONESHOT事件，以确保下次可读时EPOLLIN事件能被触发
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


/*------------载入数据库表----------*/
void http_conn::initmysql_result(connection_pool* connPool) {
    // 从连接池中取出一个连接
    MYSQL* mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    // 在user表中检索username，passwd数据
    if (mysql_query(mysql, "SELECT username, passwd FROM user")) LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    // 从表中检索出完整的结果集
    MYSQL_RES* result = mysql_store_result(mysql);
    // 返回结果集中的列数
    int num_fields = mysql_num_fields(result);
    // 返回所有字段结构的数组
    MYSQL_FIELD* fields = mysql_fetch_fields(result);
    // 从结果集中获取下一行，将对应的用户名和密码存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}


/*------------连接状态----------*/
// 初始化新接收的连接
void http_conn::init(int sockfd, const sockaddr_in& addr) {
    m_sockfd = sockfd;
    m_address = addr;

    // 端口复用（SOL_SOCKET是端口复用的级别，SO_REUSEADDR表示端口复用）
    // int reuse = 1;  // 端口复用的值，1表示可以复用，0表示不可以复用
    // setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 添加到epoll对象中
    addfd(m_epollfd, sockfd, true);

    // 用户数量加1
    m_user_count++;  

    // 初始化其他信息
    init();  
}

// 初始化其他连接
void http_conn::init() {
    m_check_state = CHECK_STATE_REQUESTLINE;  // 初始化状态为解析请求行
    m_checked_idx = 0;
    m_start_line = 0;
    m_read_idx = 0;
    m_write_idx = 0;

    m_method = GET;
    m_url = NULL;
    m_version = NULL;
    m_content_length = 0;
    m_host = 0;
    m_linger = false;

    mysql = NULL;
    cgi = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_read_file, '\0', FILENAME_LEN);

    bytes_to_send = 0;
    bytes_have_send = 0;
}

// 关闭连接
void http_conn::close_conn(bool real_close) {
    if (m_sockfd != -1 && real_close) {
        removefd(m_epollfd, m_sockfd);  // 从epoll中移除
        m_sockfd = -1;
        m_user_count--;  // 客户数量减1
    }
}


/*------------读----------*/
<<<<<<< HEAD
// 服务器主线程循环读取客户数据，直到无数据可读或对方关闭连接，如果时ET模式，则需要循环读取，而LT不需要
=======
// 循环读取客户数据，直到无数据可读或对方关闭连接，如果时ET模式，则需要循环读取，而LT不需要
>>>>>>> bf7723c55c92d67fd49ab6503b8fc273c0bdb8fd
bool http_conn::read() {
    // 如果读缓冲区满了，则返回false
    if (m_read_idx >= READ_BUFFER_SIZE) return false;  

    // 读取到的字节
    int bytes_read = 0;

#ifdef connfdLT
    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
    m_read_idx += bytes_read;
    if (bytes_read <= 0) return false;
    printf("读取到了数据：%s\n", m_read_buf);
    return true;
#endif


#ifdef connfdET
    while (true) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // 没有数据
            return false;
        } else if (bytes_read == 0) return false;  // 客户端已经断开连接
        m_read_idx += bytes_read;
    }
    
    printf("读取到了数据：%s\n", m_read_buf);
    
    return true;
#endif
}

<<<<<<< HEAD
=======
// 从状态机读取一行，分析是请求报文的哪一部分
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

>>>>>>> bf7723c55c92d67fd49ab6503b8fc273c0bdb8fd
// 从m_read_buf读取，并处理请求报文
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;  // line_state初始化为LINE_OK
    HTTP_CODE ret =  NO_REQUEST;
    char* text = 0;
    // 在GET请求报文中，每一行都是\r\n作为结束，所以对报文进行拆解时，仅用从状态机的状态
    // (line_status=parse_line())==LINE_OK语句即可。但在POST请求报文中，消息体的末尾没
    // 有任何字符，所以不能使用从状态机的状态，只能使用主状态机的状态作为循环入口条件。此外
    // 解析完消息体后，报文的完整解析就完成了，但此时主状态机的状态还是CHECK_STATE_CONTENT，
    // 如果不添加line_status == LINE_OK则符合循环入口条件，还会再次进入循环，这不是我们所希望的。
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)) {
        // 解析到了一行完整的数据，或者解析到了请求体（也是完整数据）
        // 获取一行数据，此时从状态机已提前将一行的末尾字符\r\n变为\0\0，所以text可以直接取出完整的行进行解析
        
        text = get_line();
        m_start_line = m_checked_idx;
        // 日志
        LOG_INFO("%s", text);
        Log::get_instance()->flush();
        // 主状态机的三种状态转移逻辑
        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                // 解析请求行
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST) return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER: {
                // 解析请求头
                ret = parse_headers(text);
                if (ret == BAD_REQUEST) return BAD_REQUEST;
                else if (ret == GET_REQUEST) return do_request();  // 完整解析请求后，跳转到报文响应函数
                break;
            }
            case CHECK_STATE_CONTENT: {
                // 解析请求体
                ret = parse_content(text);
                if (ret == GET_REQUEST) return do_request();  // 完整解析请求后，跳转到报文响应函数
                line_status = LINE_OPEN;  // 解析完消息体即完成报文解析，避免再次进入循环，更新line_status
                break;
            }
            default: return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

<<<<<<< HEAD
// 从状态机读取一行，标识解析一行的读取状态。
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 解析请求行（获得请求方法、目标url，http版本）
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
    // 在http报文中，请求行用来说明请求类型，要访问的资源以及所使用的http版本，其中各个部分通过空格和\t分割
    
    // 1.请求方法提取（请求行中最先含有空格和\t任意字符的位置并返回）
    m_url = strpbrk(text, " \t");  // strpbrk是在源字符串（text）中找出最先含有搜索字符串（" \t"）中任一字符的位置并返回
    if (!m_url) return BAD_REQUEST;  // 如果没有空格和\t，则报文格式错误
    *m_url++ = '\0';  // 将当前位置改为\0，用于将前面数据取出
    char* method = text; 
    // GET和POST请求报文的区别之一是有无消息体部分，GET请求没有消息体，当解析完空行之后，便完成了报文的解析。
    if (strcasecmp(method, "GET") == 0) m_method = GET;  // strcasecmp忽略大小写比较字符串
    else if (strcasecmp(method, "POST") == 0) {
        m_method == POST;
        cgi = 1;
    }
    else return BAD_REQUEST;

    // 2.version提取
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version) return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0) return BAD_REQUEST;

    // 3.目标url提取（http）
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');  // 返回字符'/'第一次在字符串m_url中出现的位置，如果未找到字符，则返回 NULL
    }
    
    // 3.目标url提取（https)
    if (strncasecmp(m_url, "https://", 8) == 0) {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    // 通常不会有http和https符号，如果三种情况均不符，则返回错误
    if(!m_url || m_url[0] != '/') return BAD_REQUEST;

    // 当url为/时，显示欢迎界面
    if (strlen(m_url) == 1) strcat(m_url, "judge.html");
    // 请求行处理完毕，将主状态机转移到请求头
    m_check_state = CHECK_STATE_HEADER;  

    return NO_REQUEST;
}

// 解析http请求头
http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
    if (text[0] == '\0') {
        // 如果遇到空行，表示头部字段解析完毕
        // 判断时GET请求还是POST请求
        if (m_content_length != 0) {
            // POST请求需要跳转到消息体处理状态
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        // 解析Connection头部字段，Connection: keep-alive
        text += 11;
        text += strspn(text, " \t");  // 跳过空格和\t字符（检索字符串text中第一个不在字符串" \t"中出现的字符下标）
        if (strcasecmp(text, "keep-alive") == 0)  m_linger = true;  // 如果是长连接，则将linger标志设置为true
    } else if (strncasecmp(text, "Content-length:", 15) == 0) {
        // 解析Content-length头部字段
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        // 解析HOST字段
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    } else {
        // 日志
        LOG_INFO("oop!unknow header: %s", text);
        Log::get_instance()->flush();
    }
    return NO_REQUEST;
}

// 解析http请求体，判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
    // 判断buffer中是否读入了消息体
    if (m_read_idx >= (m_content_length + m_checked_idx)) {
        text[m_content_length] = '\0';

        // POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


/*------------根据请求报文生成响应正文----------*/
=======
// 生成响应报文 
>>>>>>> bf7723c55c92d67fd49ab6503b8fc273c0bdb8fd
http_conn::HTTP_CODE http_conn::do_request() {
    // 将初始化的m_read_file赋值为网站根目录
    strcpy(m_read_file, doc_root);
    int len = strlen(doc_root);  // doc_root字符串长度
    // printf("m_url:%s\n", m_url);
    // 找到m_url中'/'的位置
    const char* p = strrchr(m_url, '/');  // 在m_url字符串中搜索最后一次出现字符'/'的位置

    // 登录和注册校验（'/2'为登录校验，'/3'为注册校验）
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {
        
        // 将"/"赋值给m_url_real
        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        // 将网站目录进而m_url_real拼接，更新到m_read_file中
        strncpy(m_read_file + len, m_url_real, FILENAME_LEN - len - 1);
        // 释放m_url_real
        free(m_url_real);

        // 提取用户名和密码（原格式如下：user=123&password=123，以&分割，前面为用户名，后面是密码）
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i) name[i - 5] = m_string[i];  // 从下标5开始提取（下标0-4为user=）
        name[i - 5] = '\0';  // 结束标志位
        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j) password[j] = m_string[i];
        password[j] = '\0';


        // 校验
        // 注册校验
        if (*(p + 1) == '3') {

            // 拼接SQL语句
            char* sql_insert = (char*)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            // 如果数据库没有有重名的用户名则加入数据库，并更新users map表
            if (users.find(name) == users.end()) {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res) strcpy(m_url, "/log.html");
                else strcpy(m_url, "/registerError.html");
            } else strcpy(m_url, "/registerError.html");
        }
        else if (*(p + 1) == '2') {  // 登录校验
            // 判断浏览器输入的用户名和密码是否可以查到
            if (users.find(name) != users.end() && users[name] == password) strcpy(m_url, "/welcome.html");
            else strcpy(m_url, "/logError.html");
        }
    }
    // 如果请求资源为'/0'，表示跳转注册界面
    if (*(p + 1) == '0') {
        // 将"/register.html"赋值给m_url_real
        char* m_url_real = (char*)malloc(sizeof(char) * 200);  
        strcpy(m_url_real, "/register.html");
        // 将网站目录进而m_url_real拼接，更新到m_read_file中
        strncpy(m_read_file + len, m_url_real, strlen(m_url_real));
        // 释放m_url_real
        free(m_url_real);
        

    } else if (*(p + 1) == '1') {  // 如果请求资源为'/1'，表示跳转登录界面
        // 将"/log.html"赋值给m_url_real
        char* m_url_real = (char*)malloc(sizeof(char) * 200);  
        strcpy(m_url_real, "/log.html");
        // 将网站目录进而m_url_real拼接，更新到m_read_file中
        strncpy(m_read_file + len, m_url_real, strlen(m_url_real));
        // 释放m_url_real
        free(m_url_real);

    } else if (*(p + 1) == '5') {  // 如果请求资源为'/5'，表示跳转图片界面 
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_read_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    } else if (*(p + 1) == '6') {  // 如果请求资源为'/6'，表示跳转视频界面 
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_read_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    } else if (*(p + 1) == '7') {  // 如果请求资源为'/7'，表示跳转关注界面 
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_read_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    } else strncpy(m_read_file + len, m_url, FILENAME_LEN - len -1);  // 如果以上均不符，直接将url与网站目录拼接

    // 将m_read_file的状态传递给m_file_stat，如果失败则返回NO_RESOURCE
    if (stat(m_read_file, &m_file_stat) < 0) return NO_RESOURCE;

    // 判断文件权限是否可读，不可读则返回FORBIDDON_REQUEST状态
    if (!(m_file_stat.st_mode & S_IROTH)) return FORBIDDEN_REQUEST;

    // 判断文件类型，如果是目录，则返回BAD_REQUEST，即请求报文有误
    if (S_ISDIR(m_file_stat.st_mode)) return BAD_REQUEST;

    // 以只读方式获取文件描述符，通过mmap将该文件映射到内存中
    int fd = open(m_read_file, O_RDONLY);
    // mmap用于将一个文件或其他对象映射到内存中，提高文件的访问速度
    /*
    void* mmap(void* start,size_t length,int prot,int flags,int fd,off_t offset);
        start：映射区的开始地址，设置为0时表示由系统决定映射区的起始地址
        length：映射区的长度
        prot：期望的内存保护标志，不能与文件的打开模式冲突
            PROT_READ 表示页内容可以被读取
        flags：指定映射对象的类型，映射选项和映射页是否可以共享
            MAP_PRIVATE 建立一个写入时拷贝的私有映射，内存区域的写入不会影响到原文件
        fd：有效的文件描述符，一般是由open()函数返回
        off_toffset：被映射对象内容的起点
    */
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    // 关闭文件描述符，避免文件描述符的占用和浪费
    close(fd);

    // 文件请求成功
    return FILE_REQUEST;
}

<<<<<<< HEAD
=======
// 解析请求行，获得请求方法、目标url，http版本
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
    // 在http报文中，请求行用来说明请求类型，要访问的资源以及所使用的http版本，其中各个部分通过空格和\t分割
    
    // 1.请求方法提取（请求行中最先含有空格和\t任意字符的位置并返回）
    m_url = strpbrk(text, " \t");  // strpbrk是在源字符串（text）中找出最先含有搜索字符串（" \t"）中任一字符的位置并返回
    if (!m_url) return BAD_REQUEST;  // 如果没有空格和\t，则报文格式错误
    *m_url++ = '\0';  // 将当前位置改为\0，用于将前面数据取出
    char* method = text; 
    // GET和POST请求报文的区别之一是有无消息体部分，GET请求没有消息体，当解析完空行之后，便完成了报文的解析。
    if (strcasecmp(method, "GET") == 0) m_method = GET;  // strcasecmp忽略大小写比较字符串
    else if (strcasecmp(method, "POST") == 0) {
        m_method == POST;
        cgi = 1;
    }
    else return BAD_REQUEST;

    // 2.version提取
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version) return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0) return BAD_REQUEST;

    // 3.目标url提取（http）
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');  // 返回字符'/'第一次在字符串m_url中出现的位置，如果未找到字符，则返回 NULL
    }
    
    // 3.目标url提取（https)
    if (strncasecmp(m_url, "https://", 8) == 0) {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    // 通常不会有http和https符号，如果三种情况均不符，则返回错误
    if(!m_url || m_url[0] != '/') return BAD_REQUEST;

    // 当url为/时，显示欢迎界面
    if (strlen(m_url) == 1) strcat(m_url, "judge.html");
    // 请求行处理完毕，将主状态机转移到请求头
    m_check_state = CHECK_STATE_HEADER;  

    return NO_REQUEST;
}

// 解析http请求头
//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
    if (text[0] == '\0') {
        // 如果遇到空行，表示头部字段解析完毕
        // 判断时GET请求还是POST请求
        if (m_content_length != 0) {
            // POST请求需要跳转到消息体处理状态
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        // 解析Connection头部字段，Connection: keep-alive
        text += 11;
        text += strspn(text, " \t");  // 跳过空格和\t字符（检索字符串text中第一个不在字符串" \t"中出现的字符下标）
        if (strcasecmp(text, "keep-alive") == 0)  m_linger = true;  // 如果是长连接，则将linger标志设置为true
    } else if (strncasecmp(text, "Content-length:", 15) == 0) {
        // 解析Content-length头部字段
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        // 解析HOST字段
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    } else {
        // 日志
        LOG_INFO("oop!unknow header: %s", text);
        Log::get_instance()->flush();
    }
    return NO_REQUEST;
}

// 解析http请求体，判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
    // 判断buffer中是否读入了消息体
    if (m_read_idx >= (m_content_length + m_checked_idx)) {
        text[m_content_length] = '\0';

        // POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}



>>>>>>> bf7723c55c92d67fd49ab6503b8fc273c0bdb8fd

/*------------写----------*/
// 服务器主线程检测写事件，并调用http_conn::write函数将响应报文发送给浏览器端
bool http_conn::write() {
    int temp = 0;  // 发送字节数

    // 若要发送数据长度为0，表示响应报文为空，一般不会出现这种情况
    if (bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }
    while (1) {
        // 将响应报文的状态行、消息头、空行和响应正文发送给浏览器端
        temp = writev(m_sockfd, m_iv, m_iv_count);  // writev函数用于在一次函数调用中写多个非连续缓冲区，称为聚集写
        if (temp < 0) {
            if (errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_have_send += temp;
        bytes_to_send -= temp;
        // 第一个iovec头部信息的数据已发送完，发送第二个iovec数据
        if (bytes_have_send >= m_iv[0].iov_len) {
            // 不再继续发送头部信息
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + bytes_have_send - m_write_idx;
            m_iv[1].iov_len = bytes_to_send;
        } else {
            // 继续发送第一个iovec头部信息数据
            m_iv[0].iov_base = m_write_buf + bytes_to_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }
        // 若数据全部发送完毕
        if (bytes_to_send <= 0) {
            unmap();  // 取消映射
            // 重新注册写事件
            modfd(m_epollfd, m_sockfd, EPOLLIN);
            
            if (m_linger) {
                init();
                return true;
            } else return false;
        }
    }
}

// 关闭内存映射
void http_conn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

// 服务器子线程调用process_write完成响应报文，随后注册epollout事件。根据do_request的返回状态，服务器子线程调用process_write向m_write_buf中写入响应报文
bool http_conn::process_write(HTTP_CODE ret) {
    switch(ret) {
        // 内部错误：500
        case INTERNAL_ERROR: {
            // 状态行
            add_status_line(500, error_500_title);
            // 消息报头
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form)) return false;
            break;
        }  
        // 语法错误：404
        case BAD_REQUEST: {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form)) return false;
            break;
        }
        // 资源没有访问权限：403
        case FORBIDDEN_REQUEST: {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form)) return false;
            break;
        }
        // 文件存在：200
        case FILE_REQUEST: {
            add_status_line(200, ok_200_title);
            // 如果请求的资源存在
            if (m_file_stat.st_size != 0) {
                add_headers(m_file_stat.st_size);
                // 响应报文分为两种，一种是请求文件的存在，通过io向量机制iovec，声明两个iovec，
                // 第一个指向m_write_buf，第二个指向mmap的地址m_file_address；一种是请求出错，
                // 这时候只申请一个iovec，指向m_write_buf
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                
                // 发送的全部数据为响应报文头部信息和文件大小
                bytes_to_send = m_write_idx + m_file_stat.st_size;  
                return true;
            } else {
                // 如果请求资源大小为0，则返回空白html文件
                const char* ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if (!add_content(ok_string)) return false;
            }
        }
        default: return false; 
    }
    
    // 除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

// 每次添加到写缓存区时进行判断
bool http_conn::add_response(const char* format, ...) {

    // 如果写入内容超过m_write_buf的大小则报错
    if (m_write_idx >= WRITE_BUFFER_SIZE) return false;

    // 定义可变参数列表
    va_list arg_list;
    // 将arg_list初始化为传入参数
    va_start(arg_list, format);
    // 将数据format从可变参数列表写入写缓冲区，返回写入数据的长度
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    // 如果写入的数据长度超过缓冲区剩余空间，则报错
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
        va_end(arg_list);  // 清空可变参数列表
        return false;
    }
    // 更新m_write_idx位置
    m_write_idx += len;
    va_end(arg_list);
    // 日志
    LOG_INFO("request:%s", m_write_buf);
    Log::get_instance()->flush(); 

    return true;
}

// 添加状态行
bool http_conn::add_status_line(int status, const char* title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

// 添加消息报头，具体添加文本长度、文本类型、连接状态和空行
bool http_conn::add_headers(int content_len) {
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

// 添加Content-Length，表示响应报文的长度
bool http_conn::add_content_length(int content_len) {
    return add_response("Content-Length:%d\r\n", content_len);
}

// 添加文本类型，此处为html
bool http_conn::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}

// 添加状态行
bool http_conn::add_linger() {
    return add_response("Connections:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

// 添加空行
bool http_conn::add_blank_line() {
    return add_response("%s", "\r\n");
}

// 添加文本content
bool http_conn::add_content(const char* content) {
    return add_response("%s", content);
}


<<<<<<< HEAD
/*------------子线程处理读写入口----------*/
=======
/*------------读写入口----------*/
>>>>>>> bf7723c55c92d67fd49ab6503b8fc273c0bdb8fd
// 由线程池中的工作线程调用，这是处理http请求的入口函数
void http_conn::process() {
    // 解析http请求
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);  // 表示请求不完整，需要继续接收请求数据，注册并监听读事件
        return;
    }

    // 生成响应
    bool write_ret = process_write(read_ret);
    if (!write_ret) close_conn();
    modfd(m_epollfd, m_sockfd, EPOLLOUT);  // 注册并监听写事件
}