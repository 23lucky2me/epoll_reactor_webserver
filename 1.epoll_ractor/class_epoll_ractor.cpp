/* epoll反应堆
1.epoll反应堆中用自定义的结构体替代了fd，自定义结构体包含监听的文件描述符fd，回调函数和参数指针
2.epoll反应堆中 传入联合体（ev.data）的是一个自定义结构体指针myev 即 ev.data.ptr=myev;
3.相应事件发生 通过指针调用相应的回调函数 events[i].data.ptr->call_back(arg)；*/

/*工作流程
监听可读事件(ET) ⇒ 数据到来 ⇒ 触发读事件 ⇒epoll_wait()返回 ⇒read完数据; 节点下树;
设置监听写事件和对应写回调函数; 节点上树(可读事件回调函数内)*/

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

#define MAX_EVENTS 1014      //监听的上限
#define SERVER_PORT 8080    //默认服务器的端口号
#define BUFFER_LEN 4096     //缓冲区的大小

/*描述 就绪文件描述符  相关信息*/
struct myevents
{
    int fd;               //要监听的文件描述符
    int events;           //对应的监听事件（读、写）
    void *arg;            //泛型参数
    void (*call_back)(int fd,int events,void *arg);  //参数3可以传结构体本身
    int status;           //是否在监听   1在监听 0不在监听
    char buffer[BUFFER_LEN];  //缓冲区
    int len;
    long last_active;   //记录每次加入红黑树（epoll)的时间
};

/*结构体   成员变量   初始化  */
void event_set(struct myevents *ev,int fd,void (*call_back)(int,int,void*),void *arg){
    ev->fd =fd;
    ev->call_back = call_back;
    ev->events=0;
    ev->arg=arg;
    ev->status=0;
    memset(ev->buffer,0,sizeof(ev->buffer));
    ev->len=0;
    ev->last_active = time(NULL);
    return;
}
int epollfd;     //全局变量，保存epoll_creat创建返回的文件描述符
struct myevents My_events[MAX_EVENTS+1];  //自定义结构体的数组， +1 是存放监听的listen fd
class event_loop
{
public:
   
   // static struct epoll_event events[MAX_EVENTS+1];        //保存以满足就绪事件的文件描述符数组， 给epoll_wait服务的
public:
    event_loop(/* args */);
    ~event_loop();
    /*初始化监听socket函数*/
    void InitListenSocket(int epollfd,short port);
    /*accept接受 建立连接的函数*/
    static void accept_connect(int lfd,int events,void * arg);
    /*添加事件*/
    static void event_add(int epollfd,int events,struct myevents *ev);
    //读取数据
    static void recvdata(int fd,int events,void *arg);
    //发送数据
    static void senddata(int fd,int events,void *arg);
    //删除事件
    static void event_del(int epollfd,struct myevents *ev);
    void event_loop_run();//运行，无线循环监听读写事件有无发生，然后建立和客户端的连接
};

event_loop::event_loop(/* args */)
{
    event_loop_run();
}

event_loop::~event_loop()
{
}

void event_loop::InitListenSocket(int epollfd,short port){
    struct sockaddr_in sever_addr;  //服务器端相关的地址信息
    memset(&sever_addr,0,sizeof(sever_addr));
    sever_addr.sin_family = AF_INET;
    sever_addr.sin_port = htons(port);
    sever_addr.sin_addr.s_addr = INADDR_ANY;
    int lfd ;int opt=1;
    setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));  //设置端口复用
    lfd = socket(AF_INET,SOCK_STREAM,0);//创建一个绑定到特定传输服务提供者的套接字
    fcntl(lfd,F_SETFL,O_NONBLOCK); //设置 监听套接字 非阻塞
    bind(lfd,(struct sockaddr *)&sever_addr,sizeof(sever_addr));
    listen(lfd,128);
    printf("服务器端开始运行，服务器的端口号：[%d]\n",port);
    //取事件数组的最后一位，放入listen 监听fd,
    event_set(&My_events[MAX_EVENTS],lfd,accept_connect,&My_events[MAX_EVENTS]);
    event_add(epollfd,EPOLLIN,&My_events[MAX_EVENTS]);

}

/*当有文件描述符就绪时，epoll返回，调用该函数与客户端建立连接*/
void event_loop::accept_connect(int lfd,int events,void * arg){
    struct sockaddr_in client_addr;//客户端的地址信息
    socklen_t len = sizeof(client_addr);
    int cfd,i;

    if((cfd = accept(lfd,(struct sockaddr *)&client_addr,&len))==-1){
        if((errno!= EAGAIN) && (errno!=EINTR)){
            //不做处理
        }
        printf("客户端连接错误\n");
        return;
    }
    printf("客户端已连接\n");

    do
    {
        
        /*从全局数组My_events 中找到一个空闲位置*/
        for(i=0;i<MAX_EVENTS;i++){
            if(My_events[i].status==0){
                break;
            }
        }
        if(i==MAX_EVENTS){
            printf("已达到最大的连接[%d]\n",MAX_EVENTS);
            break;
        }

        int flag=0;
        if((flag = fcntl(cfd,F_SETFL,O_NONBLOCK))<0){
            printf("将连接到的客户端设置为非阻塞失败\n");
            break;
        }

        /*给cfd 设置一个myevents结构体，回调函数，设置为recvdata*/
        event_set(&My_events[i],cfd,recvdata,&My_events[i]);
        event_add(epollfd,EPOLLIN,&My_events[i]); //将cfd添加到监听，监听读事件



    } while (0);
    
}

void event_loop::event_add(int epollfd,int events,struct myevents *ev){
    /*向epoll监听的红黑树  添加一个文件描述符*/
    struct epoll_event epv = {0,{0}};   
    int op;
    epv.data.ptr = ev;
    epv.events = ev->events = events; //EPOLLIN 或epollout

    if(ev->status ==0){
        //原来不在树上
        op = EPOLL_CTL_ADD;
    }  else{
        printf("增加失败，事件已经存在\n");
        return;
    }

    if(epoll_ctl(epollfd,op,ev->fd,&epv)<0){
        printf("事件添加失败，监听的描述符是：[%d], 要添加的事件是：[%d]\n",ev->fd,events);
    }else{
        ev->status=1;
        printf("事件添加成功，添加监听的描述符是：[%d], 添加的事件是：[%d]\n",ev->fd,events);
    }
}  

void event_loop::recvdata(int fd,int events,void *arg){
    struct myevents *ev = (struct myevents *)arg;
    //read()    recv等于read函数
    int len = recv(fd,ev->buffer,sizeof(ev->buffer),0);//读文件 描述符，数据存入muevents成员的buffer中

    //将该节点从红黑树上删除
    event_del(epollfd,ev);

    if(len>0){
        ev->len = len;
        ev->buffer[len] = '\0'; //手动添加字符串结束标记
        printf("客户端:[%d],数据内容:[%s]\n",fd,ev->buffer);
        //设置此fd对应的回调函数为发送数据
        event_set(ev,fd,senddata,ev);
        event_add(epollfd,EPOLLOUT,ev);//3.将cfd上树，监听写事件
    }else if(len == 0){
        //客户端关闭连接
        close(ev->fd);
        printf("\n [Client:%d] 未连接 \n", ev->fd);
    }else{
        close(ev->fd);
        printf("\n 错误: [Client:%d] disconnection\n", ev->fd);

    }
}

void event_loop::senddata(int fd,int events,void *arg){
    struct myevents *ev = (struct myevents *)arg;
    int len = send(fd,"received client data",20,0);
    event_del(epollfd,ev);
    if(len>0){
        printf("服务器端向客户端[fd=%d]发送的数据为：[%s]\n",fd,ev->buffer);
        event_set(ev,fd,recvdata,ev);
        event_add(epollfd,EPOLLIN,ev);
    }else{
        close(ev->fd);
        printf("向客户端[fd= %d]发送数据失败\n",fd);

    }
}

void event_loop::event_del(int epollfd,struct myevents *ev){
    struct epoll_event epv = {0,{0}};
    if(ev->status!=1){
        return;
    }
    epv.data.ptr=NULL;
    ev->status=0;
    epoll_ctl(epollfd,EPOLL_CTL_DEL,ev->fd,&epv);
}

void event_loop::event_loop_run(){
    unsigned short port = SERVER_PORT; 
    epollfd = epoll_create(MAX_EVENTS+1);   //创建红黑树，epoll对象
    if(epollfd<=0){
        printf("创建epoll对象失败\n");
        perror("epoll_create");
        exit(-1);
    }

    InitListenSocket(epollfd,port);                  //初始化监听socket套接字
    struct epoll_event events[MAX_EVENTS+1];        //保存以满足就绪事件的文件描述符数组， 给epoll_wait服务的
    
    int checkpos = 0,i;       //用来检测客户端太久不发数据，就踢掉它
    while(1){
        /*超时验证，每次测试100个连接，不测试listenfd 
        当客户端60秒内没有和服务器通信，就关闭此客户端连接*/

        
        /*主逻辑  监听红黑树epollfd,将满足事件的文件描述符加入到events数组中,5秒没有事件满足，就返回0*/
        int events_number = epoll_wait(epollfd,events,MAX_EVENTS+1,2000);
        printf("events_number=%d\n",events_number);
        if(events_number<0){
            printf("等待epoll文件描述符上满足条件的I / O事件错误,即将退出\n");
            break;
        }
        for(int i=0;i<events_number;i++){
            /*使用自定义的结构体类型的指针，接收 联合体datda的void *ptr 成员*/
            struct myevents *ev = (struct myevents *)events[i].data.ptr; //相当于ev指针 指向了 event[i]结构体
            /*读 就绪 事件*/
            if((events[i].events & EPOLLIN) && (ev->events & EPOLLIN)){
                //保证 传入的 ptr 和传出的ptr是一样的，相互对应
                ev->call_back(ev->fd,events[i].events,ev->arg);

            }    
            /*写 就绪 事件*/
            if((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT)){
                //保证 传入的 ptr 和传出的ptr是一样的，相互对应
                ev->call_back(ev->fd,events[i].events,ev->arg);

            } 


        }
        printf("进入循环\n");
    }


    return ;
}

int main(int argc, char  *argv[])
{
    event_loop epollractr;
    
    return 0;
}
