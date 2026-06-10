#include "http_conn.h"
#include "sql_connection_pool.h"
#include <map>
#include <mysql/mysql.h>


const char* ok_200_title="OK";
const char* error_400_title="Bad Request";
const char* error_400_form="Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* error_403_title="Forbidden";
const char* error_403_form="You do not have permission to get file form this server.\n";
const char* error_404_title="Not Found";
const char* error_404_form="The requested file was not found on this server.\n";
const char* error_500_title="Internal Error";
const char* error_500_form="There was an unusual problem serving the request file.\n";
const char *doc_root = "/opt/www";
connection_pool* connPool = connection_pool::GET_INSTANCE(5,"tiny_http","root",3306,"http_database","localhost");
map<string,string> users;

int http_conn::m_epoll_fd = -1;
int http_conn::m_user_count=0;
int setnonblocking(int fd){
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd,bool one_shot)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
    if(one_shot)
        event.events|=EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void http_conn::connect_socket(int connfd) {
    m_connfd = connfd;
}

void modfd(int epollfd,int fd,int ev)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=ev|EPOLLET|EPOLLONESHOT|EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

// 从内核事件表删除描述符
void removefd(int epollfd,int fd)
{
        epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
        close(fd);
}

void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address=0;
    }
}

void http_conn::close_conn(bool real_close)
{
    if(real_close&&(m_connfd!=-1))
    {
        removefd(m_epoll_fd,m_connfd);
        close(m_connfd);
        m_connfd=-1;
        m_user_count--;
    }
}

void http_conn::initmysql_result() {
    MYSQL *mysql=connPool->GET_CONNECTION();

    if(mysql_query(mysql,"SELECT username,passwd FROM user"))
    {
        printf("INSERT error:%s\n",mysql_error(mysql));
        exit(1);
    }

    MYSQL_RES *result=mysql_store_result(mysql);

    int num_fields=mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields=mysql_fetch_fields(result);

    while(MYSQL_ROW row=mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1]=temp2;
    }
    connPool->RELEASE_CONNECTION(mysql);
}

void http_conn::init() {
    m_url = 0;
    m_version = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_content_length = 0;
    m_host = 0;
    m_read_idx = 0;
    m_checked_length = 0;
    m_string  = 0;
    m_start_line = 0;
    m_file_address = 0;
    m_method = GET;
    m_file_stat.st_size = 0;
    m_write_idx = 0;
    memset(m_iv, 0, sizeof(m_iv));
    m_iv_count = 0;
    memset(m_buffer_read,0,READ_BUFFER_SIZE);
    memset(m_buffer_write, 0, WRITE_BUFFER_SIZE);
}

void http_conn::init(int sockfd,const sockaddr_in& addr)
{
    m_connfd=sockfd;
    m_address=addr;
    //int reuse=1;
    //setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    addfd(m_epoll_fd,sockfd,true);
    m_user_count++;
    init();
}


bool http_conn::read_once() {
    if(m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }
    int bytes_read = 0;
    while(true) {
        bytes_read = recv(m_connfd,m_buffer_read + m_read_idx,READ_BUFFER_SIZE - m_read_idx,0);
        if(bytes_read == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
            }
            return false;
        } else if (bytes_read == 0) {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char *text){
    m_url = strpbrk(text," \t");
    if(!m_url) {
        return BAD_REQUEST;
    }
    *m_url = '\0';
    m_url ++;
    char *method_raw = text;
    if(strcasecmp(method_raw,"GET") == 0) {
        m_method = GET;
    }
    else if(strcasecmp(method_raw,"POST")==0)
    {
        m_method=POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url," \t");
    *m_version = '\0';
    m_version ++;
    m_version+=strspn(m_version," \t");
    //FIX :截断制表符
    char *clrf = strpbrk(m_version,"\r\n");
    if(clrf) {
        *clrf = '\0';
    }
    if(strcasecmp(m_version,"HTTP/1.1")!=0) {
        return BAD_REQUEST;
    }
    if(strncasecmp(m_url,"http://",7) == 0) {
        m_url += 7;
        m_url = strchr(m_url,'/');
    }
    if(!m_url || m_url[0] != '/'){
        return BAD_REQUEST;
    }
    if(strlen(m_url) == 1) {
        strcat(m_url,"judge.html");
    }
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_header(char *text) {
    if(text[0] == '\0') {
        if(m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    if(strncasecmp(text,"Connection:",11) == 0) {
        text += 11;
        text += strspn(text,"\t");
        if(strcasecmp(text,"keep-alive")==0)
        {
            m_linger=true;
        }
    }
    if(strncasecmp(text,"Content-length:",15) == 0) {
        text += 15;
        text += strspn(text,"\t");
        m_content_length = atoi(text);
    }
    else if(strncasecmp(text,"Host:",5) == 0) {
        text += 5;
        text += strspn(text,"\t");
        m_host = text;
    }
    else {
        printf("opos,unknow header\n");
    }
    return NO_REQUEST;
}

// 解析内容
http_conn::HTTP_CODE http_conn::parse_content(char *text) {
    if(m_read_idx >= m_content_length + m_checked_length)  {
        // 如果已经读完了
        text[m_content_length] = '\0'; //把末尾截断
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 解析行
// LINE_OPEN : 行尚未接受完毕；
// LINE_OK   : 行接受完毕；
// LINE_BAD  : 行格式错误；
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for(;m_checked_length < m_read_idx;m_checked_length++) {
        temp = m_buffer_read[m_checked_length];
        if(temp == '\r') {
            if(m_checked_length + 1 == m_read_idx) {
                return LINE_OPEN;
            } else if(m_buffer_read[m_checked_length+1]=='\n'){
                m_buffer_read[m_checked_length++] = '\0';
                m_buffer_read[m_checked_length++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if(temp == '\n') {
            if(m_checked_length>1 && m_buffer_read[m_checked_length-1] == '\r'){
                m_buffer_read[m_checked_length-1] = '\0';
                m_buffer_read[m_checked_length] = '\0';
                m_checked_length++;
                return LINE_OK;
            } 
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}


//主要的处理入口
void http_conn::process() {
    HTTP_CODE read_ret = process_read();
    //如果读返回NO_REQUEST,数据可能没有传输完毕，继续监听。
    if(read_ret == NO_REQUEST) {
        modfd(m_epoll_fd,m_connfd,EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if(!write_ret) {
        close_conn(true);
    }
    // 将描述符监听为输出
    modfd(m_epoll_fd,m_connfd,EPOLLOUT);
}


// 主状态机，根据不同状态调用不同处理
http_conn::HTTP_CODE http_conn::process_read() {
    char *text = 0;
    LINE_STATUS line_state = LINE_OK;
    HTTP_CODE ret = BAD_REQUEST;

    while((m_check_state == CHECK_STATE_CONTENT && line_state == LINE_OK) || 
    ((line_state = parse_line() ) == LINE_OK)) {
        text = get_line();
        m_start_line = m_checked_length;
        switch (m_check_state) {
            case CHECK_STATE_CONTENT :
                ret = parse_content(text);
                if(ret == GET_REQUEST) {
                    // do something
                    printf("Get content request"); // (or something)
                    return do_request();
                }
                line_state=LINE_OPEN;
                break;
            
            case CHECK_STATE_REQUESTLINE :
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST)  {
                    return BAD_REQUEST;
                }
                break;
            
            case CHECK_STATE_HEADER : 
                ret = parse_header(text);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if(ret == GET_REQUEST) {
                    printf("Get header request\n");
                    return do_request(); // (or something)
                }
                break;
            default: return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

// 添加响应
bool http_conn::add_response(const char* format,...) {
    if(m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list,format);
    int len=vsnprintf(m_buffer_write+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
    if(len>=(WRITE_BUFFER_SIZE-1-m_write_idx))
        return false;
    m_write_idx+=len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status,const char* title)
{
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool http_conn::add_headers(int content_len)
{
    //add_content_type();
    add_content_length(content_len);
    add_linger();
    add_blank_line();
    return true;
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length: %d\r\n",content_len);
}

bool http_conn::add_linger()
{
    return add_response("Connection: %s\r\n",(m_linger==true)?"keep-alive":"close");
}

bool http_conn::add_content(const char* content)
{
    return add_response("%s",content);
}

bool http_conn::add_blank_line()
{
    return add_response("%s","\r\n");
}

//根据读的返回码处理写
bool http_conn::process_write(HTTP_CODE ret) {
    switch(ret) {
        case INTERNAL_ERROR:
        {
            add_status_line(500,error_500_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form))
                return false;
            break;
        }
        case NO_RESOURCE:
        case BAD_REQUEST:
        {
            add_status_line(404,error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403,error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200,ok_200_title);
            // 如果文件存在，初始化io参数准备返回内容；
            if(m_file_stat.st_size!=0)
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base=m_buffer_write;
                m_iv[0].iov_len=m_write_idx;
                m_iv[1].iov_base=m_file_address;
                m_iv[1].iov_len=m_file_stat.st_size;
                m_iv_count=2;
                return true;
            }
            // 否则返回空内容；
            else
            {
                const char* ok_string="<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string))
                    return false;
            }
        }
        default:
            return false;
    }
    m_iv[0].iov_base=m_buffer_write;
    m_iv[0].iov_len=m_write_idx;
    m_iv_count=1;
    return true;
}


// 处理请求
http_conn::HTTP_CODE http_conn::do_request(){
    printf("Starting process request\n");
    // 拼接真正的路径。
    // 如doc_root = /root/www,而m_url = /index.html
    // 需拼接到 /root/www/index.html;
    strcpy(m_real_file,doc_root);
    int len = strlen(doc_root);
    const char* p = strrchr(m_url,'/');
    // 2CGISQL.cgi || 3CGISQL.cgi 且POST
    if(cgi == 1 && (p[1] == '2' || p[1] == '3')) {
        char flag = m_url[1]; // 2登录 3注册；
        char m_url_real[200];
        strcpy(m_url_real,"/");
        strcat(m_url_real,m_url+2); //跳过 '/' 和 '3'或'2'
        // m_url_real = /CGISQL.cgi
        //m_real_file + len 指向/root/www尾部,拷贝为/root/www/CGISQL.cgi
        strncpy(m_real_file+len,m_url_real,FILENAME_LEN - len -1);
        const char *name_start = strchr(m_string,'=');
        const char *delim = strchr(m_string,'&');
        // 处理账户密码
        char name[200],password[200];
        if(name_start && delim && delim > name_start) {
            name_start ++;
            size_t name_len = delim - name_start;
            // 防止缓冲区溢出
            if (name_len > sizeof(name)) {
                return BAD_REQUEST;
            }
            strncpy(name,name_start,name_len);
            name[name_len] = '\0';
            // 不要查找 '='。
            // 对于name=111&gender=male&password=1222可能会造成安全隐患
            // const char *password_start = strchr(delim,'=');
            const char *password_start = strstr(delim,"password=");
            if(password_start) {
                password_start += 9;
                strcpy(password,password_start);
            } else {
                return BAD_REQUEST;
            }
        } else {
            return BAD_REQUEST;
        }

        pthread_mutex_t lock;
        pthread_mutex_init(&lock, NULL);
        MYSQL *mysql=connPool->GET_CONNECTION();
        char *sql_insert = (char*)malloc(sizeof(char)*200);
        strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
        strcat(sql_insert, "'");
        strcat(sql_insert, name);
        strcat(sql_insert, "', '");
        strcat(sql_insert, password);
        strcat(sql_insert, "')");
        if(*(p+1) == '3'){
            if(users.find(name)==users.end()){
                pthread_mutex_lock(&lock);
                int res = mysql_query(mysql,sql_insert);
                users.insert(pair<string, string>(name, password));
                pthread_mutex_unlock(&lock);
                if(!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
	        }
            else
                strcpy(m_url, "/registerError.html");
        }
	//如果是登录，直接判断
	//若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
            else if(*(p+1) == '2'){
                if(users.find(name)!=users.end()&&users[name]==password)
                    strcpy(m_url, "/welcome.html");
                else
                    strcpy(m_url, "/logError.html");
            }
        connPool->RELEASE_CONNECTION(mysql);
        free(sql_insert);
        pthread_mutex_destroy(&lock);
    }
    if(*(p+1) == '0') {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real,"/register.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        
        free(m_url_real);
    }
    else if( *(p+1) == '1') {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real,"/log.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        
        free(m_url_real);
    }
    else {
        strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);
    }
    // m_real_file存放文件的绝对地址
    if(stat(m_real_file,&m_file_stat)<0)
        return NO_RESOURCE;
    if(!(m_file_stat.st_mode&S_IROTH))
        return FORBIDDEN_REQUEST;
    if(S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    int fd=open(m_real_file,O_RDONLY);
    m_file_address=(char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}


bool http_conn::write() {
    int temp=0;
    int bytes_have_send= 0;
    int bytes_to_send= m_write_idx + m_file_stat.st_size;
    // 如果没有字节待发送，代表发送完成，将连接套接字文件标识符注册到轮询事件表监听；
    if(bytes_to_send == 0) {
        modfd(m_epoll_fd,m_connfd,EPOLLIN);
        init();
        return true;
    }
    while(1) {
        temp = writev(m_connfd,m_iv,m_iv_count);
        // 如果发送失败
        if(temp<=-1)
        {
            // 但是已经到末尾
            if(errno==EAGAIN)
            {
                // 继续监听，本次发送完成
                modfd(m_epoll_fd,m_connfd,EPOLLOUT);
                return true;
            }
            // 否则释放查询文件的虚拟内存，返回错误
            unmap();
            return false;
        }
        bytes_to_send-=temp;
        bytes_have_send+=temp;
        // 是否发完了响应头
        if(temp >= m_iv[0].iov_len) {
            temp -= m_iv[0].iov_len;
            m_iv[0].iov_len = 0;

            //开始调整第二块的首指针
            m_iv[1].iov_base = (char*)m_iv[1].iov_base + temp;
            m_iv[1].iov_len -= temp;
        } else {
            m_iv[0].iov_base = (char*)m_iv[0].iov_base + temp;
            m_iv[0].iov_len -= temp;
        }
        //准备发送的比已经发送的要少，代表客户端可能有持久连接
        if(bytes_to_send<= 0)
        {
            // 释放本次请求
            unmap();
            //如果有持久连接，重新初始化并且监听
            if(m_linger)
            {
                init();
                modfd(m_epoll_fd,m_connfd,EPOLLIN);
                return true;
            }
            // 否则本次写失败
            else
            {
                modfd(m_epoll_fd,m_connfd,EPOLLIN);
                return true;
            }
        }
    }
}

