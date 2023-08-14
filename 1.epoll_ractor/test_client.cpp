#include<stdlib.h>
#include<stdio.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<unistd.h>
#include<time.h>
#include<errno.h>
#include<string.h>

#define SERVER_PORT 8080    //服务器的端口号

int main(int argc,char *argv[])
{
  int count =10;
  char readbufer[1024];
 
  struct sockaddr_in server_addr;  //初始化服务器的地址结构
  

  int fd = socket(AF_INET,SOCK_STREAM,0);
  if(fd==-1){
        printf("创建套接字失败\n");
        perror("socket");
        return -1;
  }
   bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family=AF_INET;
  server_addr.sin_port=htons(SERVER_PORT);
  inet_pton(AF_INET,"127.0.0.1",&server_addr.sin_addr.s_addr);
  printf("套接字创建成功,fd=%d\n",fd);
 
  int ret = connect(fd,(struct sockaddr *)&server_addr,sizeof(server_addr));
  if(ret!=0){
    printf("客户端连接服务器端失败\n");
        perror("connect");
        return -1;
  }
  printf("ret 的值  %d\n",ret);

  while(count--){
    int writenum = write(fd,"test",4);
    printf("writenum 的内容  %d\n",writenum);
    sleep(1);
    int n = read(fd,readbufer,sizeof(readbufer));
    write(STDOUT_FILENO,readbufer,n);
  }
  close(fd);
  return 0 ;
}
    
    

 
