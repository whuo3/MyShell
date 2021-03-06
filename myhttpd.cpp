
const char * usage =
"                                                               \n"
"daytime-server:                                                \n"
"                                                               \n"
"Simple server program that shows how to use socket calls       \n"
"in the server side.                                            \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   daytime-server <port>                                       \n"
"                                                               \n"
"Where 1024 < port < 65536.             \n"
"                                                               \n"
"In another window type:                                       \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where daytime-server  \n"
"is running. <port> is the port number you used when you run   \n"
"daytime-server.                                               \n"
"                                                               \n"
"Then type your name and return. You will get a greeting and   \n"
"the time of the day.                                          \n"
"                                                               \n";
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

typedef void (*httprunfunc)(int masterSocket);
int QueueLength = 30;
pthread_mutex_t mutex;
// Processes time request
void processTimeRequest( int socket );
void forkServer( int masterSocket);
void basicServer(int masterSocket);
void threadServer(int masterSocket);
void poolOfThreads(int masterSocket);
void ThreadPool(int masterSocket);

extern "C" void hello( int sig )
{	
	//wait3(0,0,NULL);
	while(waitpid(-1,NULL,WNOHANG)>0);
}

int
main( int argc, char ** argv )
{
  sigset(SIGCHLD, hello);
  struct sigaction signalAction;
  signalAction.sa_handler=hello;
  sigemptyset(&signalAction.sa_mask);
  signalAction.sa_flags = SA_RESTART;
  httprunfunc run; int port;
  char Request_Method[]="REQUEST_METHOD=GET";
  putenv(Request_Method);
  // Print usage if not enough arguments
  if ( argc < 2 ) {
    fprintf( stderr, "%s", usage );
    exit( -1 );
  }
  if(!strncmp(argv[1],"-f",2)){
	port = atoi( argv[2] );
	run = forkServer;
  }
  else if(!strncmp(argv[1],"-t",2)){
	port = atoi( argv[2] );
	run = threadServer;
  }
  else if(!strncmp(argv[1],"-p",2)){
	port = atoi( argv[2] );
	run = poolOfThreads;
  }
  else{
	port = atoi( argv[1] );
	run = basicServer;
  }
  
  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
  
  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }
  run(masterSocket);
  
}
void poolOfThreads(int masterSocket){
  pthread_t thread[4];
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_mutex_init(&mutex,NULL);
  for(int i=0;i<4;i++){
	pthread_create(&thread[i],&attr,(void * (*)(void *))ThreadPool,(void*)masterSocket);
  }
  pthread_join(thread[0],NULL);
  pthread_join(thread[1],NULL);
  pthread_join(thread[2],NULL);
  pthread_join(thread[3],NULL);
  basicServer(masterSocket);
}

void threadServer(int masterSocket){
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init( &attr );
  while ( 1 ) {
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);
    
    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }
    pthread_create(&thread,&attr,(void * (*)(void *))processTimeRequest,(void *)slaveSocket);
  }
}
void basicServer(int masterSocket){
  while ( 1 ) {
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);

    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }
    processTimeRequest( slaveSocket );
    close( slaveSocket );
  }
}
void ThreadPool(int masterSocket){
  while ( 1 ) {
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    pthread_mutex_lock(&mutex);
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);
    pthread_mutex_unlock(&mutex);
    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }
    processTimeRequest( slaveSocket );
    close( slaveSocket );
  }
}
void forkServer(int masterSocket){
  while ( 1 ) {
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);

    if ( slaveSocket < 0 ) {
      if(errno==EINTR) continue;
      perror( "accept" );
      exit( -1 );
    }
    int ret = fork();
    if(ret==0){
    	processTimeRequest( slaveSocket );
	exit(0);
    }
    close( slaveSocket );
  }
}

void
processTimeRequest( int fd )
{
  // Buffer used to store the name received from the client
  char *buf=(char*)malloc(1024*sizeof(char)); int bufSize=1024;
  int i=0; int n;
  unsigned char newChar;
  unsigned char lastChar = 0;
  FILE * fp; int again=0;
  while (( n = read( fd, &newChar, sizeof(char) ) ) > 0 ) {
    *(buf+i)=newChar;
    //fprintf(stderr,"%c",newChar);
    if(i==bufSize-1) buf=(char*)realloc(buf,(bufSize*=2));
    i++;
    if ( lastChar == '\015' && newChar == '\012' ) {
      if(again==0) {again =1; lastChar = newChar; continue;}
      if(again==1) {again =0;     break;}
    }
    lastChar = newChar;
  }
  char* tmp2; char argtemp[256]="QUERY_STRING="; char possible_Arg[1024]; bzero(possible_Arg,1024);
  if((tmp2=strrchr(buf+4,'?'))!=NULL){
	*tmp2=' '; char* temp3;
	temp3 = strchr(tmp2+1,' ');
	*temp3 = 0;
	strcat(argtemp,tmp2+1);
	strcpy(possible_Arg,tmp2+1);
	putenv(argtemp);
  }
  //Sort Arguments!
  char * Exe_Arg[50]; int Arg_Num = 0;
  if(strlen(possible_Arg)!=0){
	char* p = possible_Arg; char * p2;
	while((p2=strchr(p,'&'))!=NULL){
		Exe_Arg[++Arg_Num] = p;
		p = p2+1;
		*p2 = 0;
	}
	Exe_Arg[++Arg_Num] = p;
	Exe_Arg[++Arg_Num] = NULL;
  }
  char Document_Name[1024]="./http-root-dir"; char* temp;
  temp = strchr(buf+4,' ');
  *temp = 0; temp=buf+4; 
  if(strlen(temp)==1){
	sprintf(Document_Name,"./http-root-dir/htdocs/index.html");
  }
  else if(!strncmp(temp,"/cgi-bin",8)||!strncmp(temp,"/cgi-src",8)||!strncmp(temp,"/icons",6)){
	strcat(Document_Name,temp);
  }
  else{
	strcat(Document_Name,"/htdocs");
	strcat(Document_Name,temp);
  }
  temp = strrchr(Document_Name,47);
  if(strlen(temp)==1) {temp = strrchr(Document_Name,47);}
  fp = fopen(Document_Name,"r");
  //fprintf(stderr,"%s",Document_Name);
  char buf1[1024];
  DIR *dirptr = opendir(Document_Name);
  if(fp==NULL&&dirptr==NULL){
	sprintf(buf1, "HTTP/1.1 404 NOT FOUND\r\n");
 	write(fd, buf1, strlen(buf1));
	sprintf(buf1, "Server: CS 252 lab5\r\n");
 	write(fd, buf1, strlen(buf1));
	sprintf(buf1, "Content-type: text/html\r\n");
 	write(fd, buf1, strlen(buf1));
	sprintf(buf1, "\r\n");
	write(fd, buf1, strlen(buf1));
	sprintf(buf1, "Could not find the specified URL.\r\n");
	write(fd, buf1, strlen(buf1));
  }
  else{
	sprintf(buf1, "HTTP/1.1 200 %s follows\r\n",temp);
 	write(fd, buf1, strlen(buf1));
	sprintf(buf1, "Server: CS 252 lab5\r\n");
 	write(fd, buf1, strlen(buf1)); 
	char buf2[128]; char* temp1; 
	if(dirptr!=NULL){
		strcpy(buf2,"text/html");
		sprintf(buf1, "Content-type: %s\r\n",buf2);
		write(fd, buf1, strlen(buf1)); 
		sprintf(buf1, "\r\n");
		write(fd, buf1, strlen(buf1));
		struct dirent * direntp;
		sprintf(buf1, "<title>Index of %s</title>\r\n",Document_Name);
		write(fd, buf1, strlen(buf1));
		sprintf(buf1, "<h1>Index of %s</h1>\r\n",Document_Name);
		write(fd, buf1, strlen(buf1));
		sprintf(buf1, "<ul>");
		write(fd, buf1, strlen(buf1));
		while(direntp = readdir(dirptr)){
			if(*direntp->d_name=='.') continue;
			sprintf(buf1, "<li><A HREF=\"%s\">%s</A>\r\n",direntp->d_name,direntp->d_name);
			write(fd, buf1, strlen(buf1));
		}
		sprintf(buf1, "</ul>");
		write(fd, buf1, strlen(buf1));
	}
	else if(!strncmp(buf+4,"/cgi-bin",8)){
		int fdpipe[2]; 
		int err_pipe[2];
		int defaultout = dup(1);
		int defaulterr = dup(2);
		pipe(fdpipe);
		pipe(err_pipe);
		dup2(fdpipe[1],1);
		close(fdpipe[1]);
		dup2(err_pipe[1],2);
		close(err_pipe[1]);
		int pid=fork();
		if(pid == -1){
			perror("ls: fork\n");
			exit(2);
		}
		if(pid==0){
			close(fdpipe[1]);
			close(fdpipe[0]);
			close(err_pipe[1]);
			close(err_pipe[0]);
			close(defaulterr);
			close(defaultout);
			char temp2[128];
			sprintf(temp2,"./http-root-dir%s",buf+4);
			char temp3[256];
			strcpy(temp3,temp2);
			*Exe_Arg = temp3;
			if(strlen(possible_Arg)!=0)	execvp(temp2,Exe_Arg);
			else 				execvp(temp2,0);
			perror("suck!\n");
			exit(2);
		}
		dup2(defaulterr,2);
		dup2(defaultout,1);
		waitpid(pid,0,0);
		char buf3[4096]; int tec;
		while((tec=read(fdpipe[0],buf3,4096))<=4096&&tec>0){
			write(fd,buf3,tec);
			bzero(buf3,4096);
		}
		while((tec=read(err_pipe[0],buf3,4096))<=4096&&tec>0){
			write(fd,buf3,tec);
			bzero(buf3,4096);
		}
		close(fdpipe[0]);
		close(fdpipe[1]);
		close(defaultout);
		close(err_pipe[1]);
		close(err_pipe[0]);
		close(defaulterr);
	}
	else{
		char *extension = strrchr(buf+4,'.');
		if(extension==NULL){
			if(strlen(buf+4)==1) strcpy(buf2,"text/html");
			else strcpy(buf2,"text/plain");
		}
		else{
			extension+=1;
			if(!strcmp(extension,"html")){
				strcpy(buf2,"text/html");
			}
			else if(!strcmp(extension,"gif")){
				strcpy(buf2,"image/gif");
			}
			else if(!strcmp(extension,"jpg")){
				strcpy(buf2,"image/jpeg");
			}
			else if(!strcmp(extension,"jpeg")){
				strcpy(buf2,"image/jpeg");
			}
			else strcpy(buf2,"text/plain");
		}
		//fprintf(stderr,"%s",buf2);
		sprintf(buf1, "Content-type: %s\r\n",buf2);
		write(fd, buf1, strlen(buf1)); 
		sprintf(buf1, "\r\n");
		write(fd, buf1, strlen(buf1));
		char buf3[4096]; int tec;
		while((tec=fread(buf3,1,4096,fp))<=4096&&tec>0){
			write(fd,buf3,tec);
			bzero(buf3,4096);
		}
/*		int tec=fgetc(fp);
		while(tec!=EOF){
			write(fd,&tec,1);
			tec=fgetc(fp);
		}*/
	}
	fclose(fp);
	close(fd);
  }
}




