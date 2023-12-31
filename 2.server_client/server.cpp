/*  C/S模型下的服务端   */

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
  int ret;   //实际读取到的字节数
  char buf[BUFSIZE];
  struct sockaddr_in serv_addr,client_addr;
  socklen_t client_addr_len;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htos(SERV_PORT);
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  int lfd = socket(AF_INET,SOCK_STREAM,0);
  if(lfd == -1){
    sys_err("socket error");
  }
  bind(lfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
  listen(lfd,128);
  client_addr_len = sizeof(client_addr);
  int cfd = accept(lfd,(struct sockaddr *)&client_addr,&client_addr_len);

  while(1){
  ret = read(cfd,buf,sizeof(buf));
  write(STDOUT)FILENO,buf,ret);   //屏幕打印
  for(int i=0;i<ret;i++){
    buf[i] = toupper(buf[i]);
  }
  write(cfd,buf,ret);
  }

  close(cfd);
  close(lfd);
  return 0;
}






