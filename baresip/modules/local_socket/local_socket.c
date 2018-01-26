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
 * 
 */

static int setnonblocking(int sock) ; 
static void server_handler(int flags, void *arg);
static void connect_handler(int flags, void *arg);
static io_redirection(char key);



int server_sockfd;  
int client_sockfd;  


//static struct local_st *lst;
struct sockaddr_un client_address,server_address;    

int client_len;


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

static int create_bind_and_listen ()  
{  
    int len;
    int temp;
    int sfd;
    struct sockaddr_un server_address; 
    unlink ("/home/jk/songxh/ola_voip_local_socket"); /*删除原有server_socket对象*/  
    sfd = socket (AF_LOCAL, SOCK_STREAM, 0);  

    server_address.sun_family = AF_LOCAL;  
    strcpy (server_address.sun_path, "/home/jk/songxh/ola_voip_local_socket");  
    len = sizeof (struct sockaddr_un);  
    /*绑定 socket 对象*/  
    temp = bind (sfd, (struct sockaddr *)&server_address,len);  

    setnonblocking(sfd);  

    listen (sfd, 5);  

    return sfd;  
}

int local_alloc(void)  
{  
    int len;
    int temp;

    server_sockfd = create_bind_and_listen();
    //加入epoll的监听队列   
    temp = fd_listen(server_sockfd,FD_READ,server_handler,0);

    return 0 ;
}

static void server_handler(int flags, void *arg)
{
    client_sockfd= accept(server_sockfd,(struct sockaddr *)&server_address,\
            (socklen_t *)&client_len);

    setnonblocking(client_sockfd);  
    //将新的连接加入epoll的监听队列   
   // fd_listen(client_sockfd,FD_READ | FD_WRITE | FD_EXCEPT,connect_handler,0);
    fd_listen(client_sockfd,FD_READ,connect_handler,0);

}

static void connect_handler(int flags, void *arg)
{
    int bytes;
    char ch_recv[2];

    bytes = read (client_sockfd, &ch_recv, 1); 
    
    ch_recv[bytes] = '\0'; 
    
    printf("bytes:%d, %s \n",bytes,ch_recv);

    if(bytes == 1)
    {
        io_redirection(ch_recv[0]);
    }

    if(bytes == 0){ close (client_sockfd); } 

    if(bytes == -1)
    {
        if(errno != EAGAIN) perror("read");
    }

}
static int print_local_out_handler(const char *p, size_t size, void *arg)
{
	(void)arg;

//	return 1 == fwrite(p, size, 1, stderr) ? 0 : ENOMEM;
}

static io_redirection(char key)
{
	static struct re_printf pf_local_out = {print_local_out_handler, NULL};

	ui_input_key(baresip_uis(), key, &pf_local_out);

}

static int local_init(void)
{
    local_alloc();
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
