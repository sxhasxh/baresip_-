/**
 * @file local_socket.c add by songxiuhe
 *
 * 
 */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <re.h>
#include <baresip.h>

//#include <sys/types.h>  
//#include <sys/socket.h>  
//#include <stdio.h>  
#include <sys/un.h>  
//#include <unistd.h>  
//#include <stdlib.h>  
/**
 * 编写这个文件是为了：当baresip运行在deamon模式的时候不通过网络方式访问
 * 而是使用local_socket的方式访问后台运行的server
 * 即：为进程间通信编写的local_socket 
 */

static int create_bind_and_listen (void); 
static int setnonblocking(int sock) ; 
static void server_handler(int flags, void *arg);
static void connect_handler(int flags, void *arg);
static void report_cmd(char key);
static int out_to_socket(const char * str);
static int print_handler(const char *p, size_t size, void *arg);
static int output_handler(const char *str);


int server_sockfd;  
int client_sockfd;  
struct sockaddr_un client_address,server_address;    



// 设置套接字为不阻塞  
static int setnonblocking(int sfd)  
{  
    int flags, s;  
    printf("sfd = %d \n",sfd);
    //得到文件状态标志  
    flags = fcntl (sfd, F_GETFL, 0);  
    if (flags == -1)  
    {  
        perror ("fcntl");  
    }  

    //设置文件状态标志  
    flags |= O_NONBLOCK;  
    s = fcntl (sfd, F_SETFL, flags);  
    if (s == -1)  
    {  
        perror ("fcntl");  
    }  

    return 0;  
}

static int create_bind_and_listen (void)  
{  
    int len;
    int temp;
    int sfd;

    unlink ("/data/ola_voip_local_socket"); /*删除原有server_socket对象*/  

    sfd = socket (AF_LOCAL, SOCK_STREAM, 0);  
    if(sfd == -1)
    {
        perror("socket");
        return -1;
    }

    server_address.sun_family = AF_LOCAL;  
    strcpy (server_address.sun_path, "/data/ola_voip_local_socket");  
    len = sizeof (struct sockaddr_un);  
    /*绑定 socket 对象*/  
    temp = bind (sfd, (struct sockaddr *)&server_address,len);  
    if(temp == -1)
    {
        perror("bind");
        return -1;
    }

    setnonblocking(sfd);  

    temp = listen (sfd, 5);  
    if(temp == -1)
    {
        perror("listen");
        return -1;
    }

    return sfd;  
}

static int local_alloc(void)  
{  
    int temp;

    server_sockfd = create_bind_and_listen();
    if(server_sockfd == -1)
    {
        return -1;
    }

    //加入epoll的监听队列   
    temp = fd_listen(server_sockfd,FD_READ,server_handler,0);
    if(temp != 0)
    {
        return -1;
    }

    return 0 ;
}

static void server_handler(int flags, void *arg)
{
    int client_len;

    client_sockfd= accept(server_sockfd,(struct sockaddr *)&server_address,\
            (socklen_t *)&client_len);
    if(client_sockfd < 0 )
    {
        perror("accept");
    }

    setnonblocking(client_sockfd);  
    //将新的连接加入epoll的监听队列   
    fd_listen(client_sockfd,FD_READ,connect_handler,0);

}

static void connect_handler(int flags, void *arg)
{
    int bytes;
//  char *ch_recv;
    char ch_recv;

//  ch_recv = (char *)malloc(2*sizeof(char));

    bytes = read (client_sockfd, &ch_recv, 1); 
    
//  ch_recv[bytes] = '\0'; 
//  *(ch_recv+bytes) = '\0'; 
//  if(bytes > 0)
//  { 
//      printf("bytes:%d, %s \n",bytes,ch_recv);
//  }
    if(bytes == 1)
    {
        report_cmd(ch_recv);
    }

    if(bytes == 0){ close (client_sockfd); } 

    if(bytes == -1)
    {
        if(errno != EAGAIN) perror("read");
    }

 //   out_to_socket(ch_recv);

}

static int out_to_socket(const char * str)
{
    int s;
    char end = 0x0a;
//    printf("out_to_socket size is :%d \n",sizeof(str));

   // s = write (client_sockfd, str, sizeof(str));  
    s = write (client_sockfd, "123", 3);  
    s = write (client_sockfd, &end,1 );  

    if (s == -1)  
    {  
        perror ("write"); //出错则将客户端的连接关闭 
        close(client_sockfd);
    }  
    return 0;
}

static int output_handler(const char *str)
{

    return print_handler(str, strlen(str), NULL);
}

static int print_handler(const char *p, size_t size, void *arg)
{
    (void)arg;

	 if(size != write(client_sockfd, p, size))
     {
        return ENOMEM;
     }

     return 0;
}

static void report_cmd(char key)
{
	static struct re_printf pf_local_out = {print_handler, NULL};

	ui_input_key(baresip_uis(), key, &pf_local_out);

}

static struct ui ui_local_socket = {
    .name = "local_socket",
    .outputh = output_handler
};

static int local_init(void)
{
    local_alloc();

    ui_register(baresip_uis(), &ui_local_socket);

    return 0;
}

static int module_close(void)
{
    printf("close \n");
    return 0;
}

const struct mod_export DECL_EXPORTS(local_socket) = {
    "local_socket",
    "ui",
    local_init,
    module_close
};
