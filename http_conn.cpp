#include "http_conn.h"
#include "sql_connection_pool.h"
#include <map>
#include <mysql/mysql.h>

const char *doc_root = "/root/www";
connection_pool* connPool = connection_pool::GET_INSTANCE(5,"tiny_http","root",3306,"http_database","localhost");
map<string,string> users;


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
    memset(m_buffer_read,0,READ_BUFFER_SIZE);
}

void http_conn::run_parse_test() {
    char request_line1[] = "GET\t/\tHTTP/1.1";
    char request_line2[] = "BAD\tDATA";
    assert(parse_request_line(request_line1) == NO_REQUEST);
    assert(parse_request_line(request_line2) == BAD_REQUEST);
    printf("TEST PASS");
}


bool http_conn::read_once() {
    if(m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }
    int bytes_read = 0;
    while(true) {
        bytes_read = recv(m_connfd,m_buffer_read,READ_BUFFER_SIZE,0);
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