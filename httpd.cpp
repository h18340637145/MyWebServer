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
        unimplemented(client);
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
        //==1  && 什么时候 ==1 ---> 两边都非0--->既不是get  又不是post  
        //即 未实现
        unimplemented(client);
        return nullptr;
    }

    int cgi = 0;    //0---不需要解析   1——---需要解析
    if(strcasecmp(method, "post") == 0){
        cgi = 1;
    }

    //解析url
    i = 0;
    while((isSpace(buf[j]) == true) && (j < sizeof(buf))){
        j++;
    }

    while((isSpace(buf[j]) == false)  && (i < sizeof(url) -1) && (j < sizeof(buf))){
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';


    char* query_string;
    //GET请求url可能会带有?,有查询参数
    if(strcasecmp(method ,"get") == 0){
        //get 方法有参数
        query_string = url;
        while((*query_string != '?') && (*query_string != '\0')){
            query_string++;
        }

        //是?
        if(*query_string == '?'){
            cgi = 1 ;
            *query_string = '\0' ;
            query_string++ ;
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


//如果不是cgi文件，也就是静态文件，直接读取文件返回给请求的http客户端即可
void serve_file(int client, const char *filename){
    FILE* resource = NULL;
    int numchars = 1;
    char buf[1024];
    buf[0] = 'A';
    buf[1] = '\0';
    
    while((numchars > 0) && strcmp("\n", buf)){
        numchars = get_line(client, buf, sizeof(buf));
    }

    //打开文件
    resource = fopen(filename, "r");
    if(resource == NULL){
        not_found(client);
    }else{
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);//关闭文件句柄
}

void cat(int client, FILE* resource){
    //发送文件的内容
    char buf[1024];
    fgets(buf, sizeof(buf), resource);
    while(!feof(resource)){
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void execute_cgi(int client, const char * path, const char* method, const char *  query_string){
    char buf[1024];
    buf[0] = 'A';//默认字符
    buf[1] = '\0';
    int numchars = 1;
    int content_length = -1;
    if (strcasecmp(method, "get") == 0)
    {
       //get
       //读取数据，把整个header都读掉，因为get写死了，直接读取html
       while((numchars > 0) && strcmp("\n", buf)){//buf！=\n
           numchars = get_line(client, buf,sizeof(buf));
       }
    }else if(strcasecmp(method, "post") == 0){
        //post  需要contentlength
        //先读一行
        numchars = get_line(client, buf, sizeof(buf));
        while(numchars > 0  && strcmp("\n", buf))
        {
            //如果是post请求， 就需要得到contentlength  
            //contentlength这个字符串一共长为15位，所以
            //去处头部依据后，将16位设置为结束符，进行比较
            //第16位为结束
            buf[15] = '\0';
            if(strcasecmp(buf, "Content-Length:") == 0){
                //内存从第17位开始就是长度，将17位开始的所有字符串转成整数就是content_length
                content_length = atoi(&(buf[16]));
            }
            numchars = get_line(client, buf, sizeof(buf));
        }

        if(content_length == -1){
            bad_request(client);
            return;
        }
    }else{
        // header or other
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf) , 0);

    //2根管道
    int cgi_output[2];
    int cgi_input[2];

    //建立output管道
    if(pipe(cgi_output) < 0){
        cannot_execute(client);
        return;
    }

    //建立input管道
    if(pipe(cgi_input) < 0){
        cannot_execute(client);
        return;
    }

    pid_t pid;
    if((pid = fork()) < 0){
        cannot_execute(client);
        return;
    }


    int i;
    char c;
    int status;
    if( pid == 0 ){//子进程
        //子进程：运行cgi脚本
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        // fork后管道都复制了一份，都是一样的
        // 子进程关闭2个无用的端口，避免浪费
        // x<---------------->1    output
        // 0<---------------->x    input

        //子进程输出重定向到output管道的1端
        dup2(cgi_output[1], 1);
        //子进程输入重定向到input 管道的0端
        dup2(cgi_input[0], 0);

        //关闭无用管道口
        close(cgi_output[0]);
        close(cgi_input[1]);

        //cgi环境变量
        sprintf(meth_env, "REQUEST_METHOD=%s",method);
        putenv(meth_env);
        if(strcasecmp(method, "get") == 0){
            sprintf(query_env , "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }else{
            //post
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        //替换执行path
        execl(path, NULL);
        // int m = excel(path, path, NULL);
        // 如果path有问题， 例如将html网页还在往里面写东西，
        // 触发Program received signal SIGPIPE , Broken pipe.
        exit(0);
    }else{
        //parent 
        // 父进程关闭2个无用的端口，避免浪费
        // 0<---------------->x    output
        // x<---------------->1    input
        //此时父子进程已经可以通信
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0){
            for (i = 0; i < content_length; i++){
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        }
        //从output管道读到子进程处理后的信息，然后send出去
        while(read(cgi_output[0], &c, 1) > 0){
            send(client, &c, 1, 0);
        }
        //完成操作后关闭管道
        close(cgi_output[0]);
        close(cgi_input[1]);
        //等待子进程返回
        waitpid(pid, &status, 0);
    }
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

void cannot_execute(int client)
{
	 char buf[1024];
	//发送500
	 sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "Content-type: text/html\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
	 send(client, buf, strlen(buf), 0);
}

void bad_request(int client)
{
	 char buf[1024];
	//发送400
	 sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
	 send(client, buf, sizeof(buf), 0);
	 sprintf(buf, "Content-type: text/html\r\n");
	 send(client, buf, sizeof(buf), 0);
	 sprintf(buf, "\r\n");
	 send(client, buf, sizeof(buf), 0);
	 sprintf(buf, "<P>Your browser sent a bad request, ");
	 send(client, buf, sizeof(buf), 0);
	 sprintf(buf, "such as a POST without a Content-Length.\r\n");
	 send(client, buf, sizeof(buf), 0);
}

//返回404错误页面，组装信息
void not_found(int client)
{
	 char buf[1024];
	 sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, SERVER_STRING);
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "Content-Type: text/html\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "your request because the resource specified\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "is unavailable or nonexistent.\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "</BODY></HTML>\r\n");
	 send(client, buf, strlen(buf), 0);
}

void headers(int client, const char *filename)
{
	 char buf[1024];
	 (void)filename;  /* could use filename to determine file type */
	//发送HTTP头
	 strcpy(buf, "HTTP/1.0 200 OK\r\n");
	 send(client, buf, strlen(buf), 0);
	 strcpy(buf, SERVER_STRING);
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "Content-Type: text/html\r\n");
	 send(client, buf, strlen(buf), 0);
	 strcpy(buf, "\r\n");
	 send(client, buf, strlen(buf), 0);
}

void unimplemented(int client)
{
	 char buf[1024];
	//发送501说明相应方法没有实现
	 sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, SERVER_STRING);
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "Content-Type: text/html\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "</TITLE></HEAD>\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "</BODY></HTML>\r\n");
	 send(client, buf, strlen(buf), 0);
}