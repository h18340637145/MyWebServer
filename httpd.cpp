#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cctype>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <pthread.h>
#include <cstdlib>
#include <sys/wait.h>
#define SERVER_STRING "Server: hhw 's http/0.1.0\r\n"//定义个人server名称

using std::cout;
using std::endl;

#define isSpace(x) isspace((int)(x))

void *accept_request(void* client);
int startup(int&);

//解析一行http报文
int get_line(int sock, char *buf, int size)
{
	 int i = 0;
	 char c = '\0';
	 int n;

	 while ((i < size - 1) && (c != '\n'))
	 {
		  n = recv(sock, &c, 1, 0);

		  if (n > 0) 
		  {
			   if (c == '\r')
			   {

				n = recv(sock, &c, 1, MSG_PEEK);
				if ((n > 0) && (c == '\n'))
				recv(sock, &c, 1, 0);
				else
				 c = '\n';
			   }
			   buf[i] = c;
			   i++;
			   
		  }
		  else
		   c = '\n';
	 }
	 buf[i] = '\0';
	return(i);
}

//每一个线程 对应一个http请求  通过此回调函数去处理
void* accept_request(void * from_client){
    int client = *(int*)from_client;
    char buf[1024];

    int numchars = get_line(client, buf, sizeof(buf));
    int i = 0 ;
    int j = 0;//主控  主串
    char method[512];
    char url[512]; //存放url
    char path[512]; 
    //http的格式
    //方法 url 版本               --方法和url有用，版本没用   
    // 其中url需要拼接成本地的相对目录路径，所以还需要一个path
//  注意：如果你的http的网址为http://192.168.0.23:47310/index.html
//               那么你得到的第一条http信息为GET /index.html HTTP/1.1，那么
//               解析得到的就是            url = /index.html
    //是空格  或者太长了，超过了方法容纳的极限
    while((isSpace(buf[j]) == false) && (i < sizeof(method) - 1)){
        //提取字符串中的第一个空格之前的部分  即 get或者post
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';

    if(strcasecmp(method, "GET") && strcasecmp(method, "POST")){//== 1 
        //  == 0 表示相同   左右！=0（get post方法都没重写）  才执行 下面
        //忽略大小写  比较字符串
        unimplement(client);
        return  nullptr;
    }
    int cgi = 0;//0 表示不执行cgi   1---表示执行cgi
    if(strcasecmp(method, "post") == 0){
        //post 方法
        //执行cgi
        cgi = 1;
    }

    //method解决之后，下来开始处理url
    i = 0;
    while(isSpace(buf[j]) == true && (j < sizeof(buf))){
        //是空格
        j++;
        //主串继续继续向前
    }
    while(isSpace(buf[j]) != true && (i < sizeof(url) -1 ) && j < sizeof(buf)){
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';//url字符数组的结尾

    //如果是get请求  url中带有？，有参数
    char * query_string = url;
    if(strcasecmp(method, "get") == 0){
        while ((*query_string != '?') && (*query_string != '\0')) {
            //？之后开始才是参数  先前进到？的后一位，即参数的开始
            query_string++;
        }
        //
        if(*query_string == '?'){
            cgi = 1;//get需要开启cgi
            *query_string = '\0';//url  在问号之前的部分算一个串
            query_string++;//query_string 现在的位置就是参数的开头处
        }
    }

    sprintf(path, "httpdocs%s", url);//将格式化字符输出到path中  
    // 此时的path就是"httpdocsurl"
    //strlen是\0之前的长度，而sizeof 是初始声明大小
    if(path[strlen(path) - 1] == '/'){
        strcat(path, "test.html");//拼接path  设置为默认访问界面
    }

    struct stat st;
    if(stat(path, &st) == -1){
        while((numchars > 0) && strcmp("\n", buf)){
            numchars = get_line(client, buf, sizeof(buf));
        }
        not_found(client);
    }else{
        if((st.st_mode & S_IFMT) == S_IFDIR){
            //S_IFDIR代表目录
            //如果请求参数为目录，自动打开test.html
            strcat(path, "/test.html");
        }
        //文件可执行
        if((st.st_mode & S_IXUSR) ||   //文件所有者具有可执行权限
            (st.st_mode & S_IXGRP) ||   //用户组具有可执行权限
            (st.st_mode & S_IXOTH)){    //其他用户具有可读性权限
            cgi = 1;
        }
        if (cgi == 0)
        {
            //不执行cgi
            serve_file(client, path);
        }else{
            //执行cgi
            execute_cgi(client, path, method, query_string);
        }
    }
    close(client);
    return nullptr;
}

void * accept_request(void * from_client){
    int client = *(int*)from_client;
    char buf[1024];
    int numchars = get_line(client, buf, sizeof(buf));

    int i = 0; 
    int j = 0;

    char method[512];
    char url[512];
    char path[512];

    while((isSpace(buf[j]) != true)  && (i < sizeof(method) - 1)){
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';
    if(strcasecmp(method, "get") && strcasecmp(method, "post")){
        //==1  &&什么时候 ==1 ---> 两边都非0--->既不是get  又不是post  
        //即 未实现
        unimplement();
        return nullptr;
    }

    int cgi = 0;//0---不需要解析   1——---需要解析
    if(strcasecmp(method, "post") == 0){
        cgi = 1;
    }

    //解析url
    i = 0;
    while(isSpace(buf[j]) == true && j < sizeof(buf)){
        j++;
    }
    while(isSpace(buf[j]) == false  && i < sizeof(url) -1 && j < sizeof(buf)){
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';
    char* query_string;
    if(strncasecmp(method ,"get") == 0){
        //get 方法有参数
        query_string = url;
        while((*query_string != '?') && (*query_string != '\0')){
            query_string++;
        }
        //是?
        if(*query_string == '?'){
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }
    sprintf(path, "httpdoc%s" , url);
    if(path[strlen(path) -1] == '/'){
        //url是个路径  不是文件 需要加上默认文件
        strcat(path, "test.html");
    }

    struct stat st;
    //将path所指的文件状态赋值到st中
    if(stat(path, &st) == -1){
        while((numchars > 0) && strcmp("\n",buf)){
            numchars = get_line(client, buf, sizeof(buf));
        }
        not_found(client);
    }else{
        //当前st的状态就是path所指文件的状态
        if((st.st_mode & S_IFMT) == S_IFDIR) {
            //如果请求参数为目录，自动打开test.html
            strcat(path, "/test.html");
        }

        if(
            (st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH)  //所有者所属组 其他人任意具有可执行权限
        )
        {
            cgi  = 1;//执行
        }
        if(cgi == 0){
            serve_file(client, path);//该client 该文件
        }else{
            execute_cgi(client, path, method, query_string);//query_string 是参数
        }
    }
    close(client);
    return nullptr;
}


int main(){
    // //启动服务端   httpd = socket     
        // setsockopt  端口复用    
        // bind 绑定    
        // listen-----监听socket有多少通信连接
    int port = 11111;
    int server_socket = startup(port);

    //
    int client_socket = -1;
    struct sockaddr_in client_name;
    pthread_t new_thread;

    socklen_t client_name_len = sizeof(client_name);

    cout << "http serverSocket is " << server_socket << endl;
    cout << "http running on port " << port << endl;

    while(1){
        //accept socket   存放clientname
        client_socket = accept(server_socket, (struct sockaddr*)&client_name,&client_name_len);
        cout << "new connection ip:" << inet_ntoa(client_name.sin_addr)
        << "  port:" << ntohs(client_name.sin_port) << endl;
        if (client_socket == -1)
        {
            perror("accept error");
            exit(-1);
        }

        //创建new_thread线程  调用acceptrequest 去处理clientsocket
        if(pthread_create(&new_thread, NULL, accept_request, (void*)&client_socket) != 0)
        {
            perror("pthread create error");
            exit(-1);
        }
    }
    close(server_socket);
    return 0;
}

int startup(int &port){
    int opt = 1;
    struct sockaddr_in name;
    //1. 建立套接字
    int httpd = socket(AF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
    {
        perror("socket error");
        exit(-1);
    }
    //2. 端口复用
    socklen_t optlen;
    setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, optlen);

    //3. socket的通信方式：ipv4  端口  ip
    bzero(&name, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(port);//short  not long
    name.sin_addr.s_addr = htonl(INADDR_ANY);

    //4. 绑定   套接字和服务端的通信方式及地址
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
    {
        perror("bind error");
        exit(-1);
    }

    //检查端口号----因为主函数中设置为11111  因此这里基本不会执行
    if(port == 0){ //没有设置port的初始值   提供一个随机端口
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
        {
            perror("getsockname error");
            exit(-1);
        }
        port = ntohs(name.sin_port);
    }

    //5. 服务端开始监听socket，最大五个连接
    if(listen(httpd, 5) < 0){
        perror("listen error");
        exit(-1);
    }
    return httpd;//将建立的socket返回
}

int startup(int& port){
    //1. 建立socket
    int httpd = socket(AF_INET, SOCK_STREAM, 0);
    if(httpd == -1){
        perror("socket error");
        exit(-1);
    }
    //2. 端口复用
    int opt = 1;
    setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));

    //3. 服务端设定通信机制地址及端口
    struct sockaddr_in name;
    bzero(&name, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);

    //4. 绑定
    if(bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0){
        perror("bind error");
        exit(-1);
    }
    //检查端口
    if (port == 0)
    {
        //
        socklen_t namelen = sizeof(name);
        if(getsockname(httpd, (struct sockaddr*)&name, &namelen)== -1){
            perror("getsocketname error");
            exit(-1);
        }
        port = ntohs(name.sin_port);
    }
    //5. 监听状态
    if(listen(httpd, 5) < 0){//
        perror("listen error");
        exit(-1);
    }
    return httpd; /// 返回httpd
}

