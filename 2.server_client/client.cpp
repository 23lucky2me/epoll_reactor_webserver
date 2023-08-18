/*  C/S模型下的客户端   */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/socket.h>

#define SERV_PORT 9527

void sys_err(const char *str){
  perror(str)
  exit(1)
}

int main(int argc,char *argv[]){
  int count = 10;
  char buf[BUFSIZE];
  struct sockaddr_in serv_addr;   //服务器的地址结构
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERV_PORT);
  inet_pton(AF_INET,"127.0.0.1",&serv_addr.sin_addr.s_addr);
  
  int cfd = socket(AF_INET,SOCK_STREAM,0);
  if(cfd == -1){
    sys_err("socket error");
  }

  connect(cfd,(struct sockaddr *)serv_addr,sizeof(serv_addr));
  while(--count){
    write(cfd,"hello",5);
    int ret = read(cfd,buf,sizeof(buf));
    write(STDOUT_FILENO,buf,ret);
  }
  close(cfd);
  return 0；
}
