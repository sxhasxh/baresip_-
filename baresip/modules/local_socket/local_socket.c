/**
 * @file local_socket.c add by songxiuhe
 * 编写这个文件是为了：当baresip运行在deamon模式的时候不通过网络方式访问
 * 而是使用local_socket的方式访问后台运行的server
 * 即：为进程间通信编写的local_socket 
 */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <re.h>
#include <baresip.h>
#include <stdio.h>  
#include <sys/un.h>  

static int create_bind_and_listen (void); 
static int setnonblocking(int sock) ; 
static void server_handler(int flags, void *arg);
static void connect_handler(int flags, void *arg);
static void report_cmd(char key);
static int print_handler(const char *p, size_t size, void *arg);
static int output_handler(const char *str);

static int server_sockfd;  
static int client_sockfd = -1;  
static struct sockaddr_un server_address;    
static const char str_local_socket_name[]= "/data/ola_voip_local_socket";

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

    unlink (str_local_socket_name); /*删除原有server_socket对象*/  

    sfd = socket (AF_LOCAL, SOCK_STREAM, 0);  
    if(sfd == -1)
    {
        perror("socket");
        return -1;
    }

    server_address.sun_family = AF_LOCAL;  
    strcpy (server_address.sun_path, str_local_socket_name);  
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
    
//在socket中使用read返回值的含义与读普通文件有不同的含义。
//
//若对方结束了连接，则返回0；
//在read的过程中，如果信号被中断，若已经读取了一部分数据，则返回已读取的字节数；
//若没有读取，则返回-1，且error为EINTR。 
//对于非阻塞socket而言，当接受缓冲区中有数据时，返回实际读取的数据长度；
//当接收缓冲区中没有数据时，read函数不会阻塞而是会失败，
//将errno 设置为EWOULDBLOCK或者EAGAIN，表示该操作本来应该是阻塞的，但是由于socket为非阻塞的socket，因此会立刻返回。
//  
// 
    if(bytes == 1)
    {
        report_cmd(ch_recv);
    }

    if(bytes == 0)
    {
        close (client_sockfd);
        fd_close(client_sockfd);
    }//表示对方结束了连接，所以关闭socket 

    if(bytes == -1)
    {
        if(errno != EAGAIN) 
        {
            perror("read");
            close (client_sockfd);
            fd_close(client_sockfd);
        }
    }

 //   out_to_socket(ch_recv);

}

static int output_handler(const char *str)
{

    return print_handler(str, strlen(str), NULL);
}

static int print_handler(const char *p, size_t size, void *arg)
{
    (void)arg;

	 if(size != (size_t)write(client_sockfd, p, size))
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

static void log_local_handler(uint32_t level, const char *msg)
{
    (void)level;

    output_handler(msg);

}

static struct ui ui_local_socket = {
    .name = "local_socket",
    .outputh = output_handler
};

static struct log lg = {
    .h = log_local_handler
};

static int local_init(void)
{
    local_alloc();

    ui_register(baresip_uis(), &ui_local_socket);

    log_register_handler(&lg);

    return 0;
}

static int module_close(void)
{
    log_unregister_handler(&lg);

    ui_unregister(&ui_local_socket);
    //关闭server_sockfd和client_sockfd,如果他们存在的话
    close(server_sockfd);

    if(client_sockfd > 0)
    {
        close(client_sockfd);
    }
//    printf("close \n");
    return 0;
}

const struct mod_export DECL_EXPORTS(local_socket) = {
    "local_socket",
    "ui",
    local_init,
    module_close
};
