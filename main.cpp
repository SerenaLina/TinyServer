#include <stdio.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <libgen.h>
#include <cstring>

#include "http_conn.h"
#define MAX_EVENT_NUMBER 100

struct client_data {
    sockaddr_in address;
    char buf[ BUFFER_SIZE];
    int sockfd;
};

void addfd_(int epoll_fd,int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event);
}

int main(int argc,char *argv[]) {
    if(argc <= 1) {
       printf("usage: %s ip_address port_number\n",basename(argv[0]));
       return 1;
    }
    http_conn user;
    // user.run_parse_test();
    int port=atoi(argv[1]);
    // 通信端点
    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    assert(listenfd >= 0);
    // 通信具体信息
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    // SO_REUSEADDR 被指定在option_name时表示支持端口复用
    // SOL_SOCKET 指示应用的层为套接字层，意味着它对所有套接字生效。
    int flag = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));
    int ret = bind(listenfd,(struct sockaddr *)&address,sizeof(address));
    assert(ret >= 0);
    // 在服务器端分配套接字并监听
    ret = listen(listenfd,5);
    assert(ret >= 0);
    // 创建轮询事件表
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd!=-1);
    addfd_(epollfd,listenfd);

    while (1) {
        // epoll_wait会将就绪的事件放到events数组中，并返回数目
        int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if(number < 0 && errno != EINTR) {
            printf("Error1");
            return -1;
        }
        for(int i = 0 ; i < number;i++) {
            int sockfd = events[i].data.fd;
            // 处理到连接请求。
            if(sockfd == listenfd) {
                printf("client listened:%d\n",sockfd);
                struct sockaddr_in client_address;
                socklen_t clientsock_len = sizeof(client_address);
                int connfd = accept(sockfd,(struct sockaddr*)&client_address
                ,&clientsock_len);

                addfd_(epollfd,connfd);
                
            } 
            // 处理数据
            else if(events[i].events & EPOLLIN) {
                if(user.read_once()) {
                    printf("Ready to read user data\n");
                }
            }
        }
    }
    
    close(epollfd);
    close(listenfd);
    return 0;
}