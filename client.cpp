#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <strings.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>


int main(int argc,char *argv[]){
    if(argc <= 1) {
        printf("usage %s:port\n",basename(argv[1]));
        return 1;
    }
    int port = atoi(argv[1]);
    struct sockaddr_in client_address;
    client_address.sin_family = AF_INET;
    client_address.sin_port   = htons(port);
    inet_pton(AF_INET,"127.0.0.1",&client_address.sin_addr);
    int connfd = socket(AF_INET,SOCK_STREAM,0);
    socklen_t socklen = sizeof(client_address);
    int sendfd = connect(connfd,(struct sockaddr *)&client_address
    ,socklen);
    char *data = "hello,world!";
    int ret = send(connfd,data,sizeof(data),0);

    printf("%s\n",strerror(errno));

    return 0;
}