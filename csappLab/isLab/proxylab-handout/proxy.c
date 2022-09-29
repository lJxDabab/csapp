#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_OBJECT_CNT 10
typedef struct {
    char cache_obj[MAX_OBJECT_SIZE];
    char cache_url[MAXLINE];
    int isvalid;
    int timeStamp;
    sem_t readmutex;
    sem_t writemutex;
    int readcnt;
}cache_block;
typedef struct{
    cache_block cache_objs[MAX_OBJECT_CNT];
    int cache_num;
}Cache;
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connection_header="Connection: close\r\n";
static const char *proxy_connection_header="Proxy-Connection: close\r\n";
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum,char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *host, char *port,char* path);
void sigpipe_handler(int sig);
void *thread(void *vargp);
void cache_init();
int cache_search(char *url);
void test();
void cache_exchange(char *url ,char *body);
void ReadModelBefore(int i);
void ReadModelAfter(int i);
void WriteMdodelBefore(int i);
void WriteMdodelAfter(int i);
Cache cache;
int main(int argc,char **argv)
{
    pthread_t tid;
    int listenfd,*connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char hostname[MAXLINE],port[10];
    Signal(SIGPIPE,sigpipe_handler);
    cache_init();
    if(argc!=2)
    {
         fprintf(stderr, "usage:%s <port>\n",argv[0]);
         exit(1);
    }
    listenfd =Open_listenfd(argv[1]);
    while(1) 
    {
        clientlen=sizeof(struct sockaddr_storage);
        connfd=(int*)malloc(sizeof(int));
        *connfd= Accept(listenfd,(SA *)&clientaddr,&clientlen);
        Getnameinfo((SA*)&clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0);
        
        printf("Accepted connection from (%s %s)\n",hostname,port);
        pthread_create(&tid,NULL,thread,connfd);

    }
    printf("%s", user_agent_hdr);
    return 0;
}
void test()
{
    printf("hellow here's test\n");
}
void WriteMdodelBefore(int i)
{
    P(&cache.cache_objs[i].writemutex);
}
void WriteMdodelAfter(int i)
{
    V(&cache.cache_objs[i].writemutex);
}
void ReadModelBefore(int i)
{
   P(&cache.cache_objs[i].readmutex);
   cache.cache_objs[i].readcnt++;
   if(cache.cache_objs[i].readcnt==1)
   {
       P(&cache.cache_objs[i].writemutex);
   }
   V(&cache.cache_objs[i].readmutex);
}
void ReadModelAfter(int i)
{
    P(&cache.cache_objs[i].readmutex);
    cache.cache_objs[i].readcnt--;
    if(cache.cache_objs[i].readcnt==0)
    {
        V(&cache.cache_objs[i].writemutex);
    }
    V(&cache.cache_objs[i].readmutex);
}
//init cache,set the timeStamp to 1
void cache_init()
{
    int i;
    cache.cache_num=0;
    for(i=0;i<MAX_OBJECT_CNT;i++)
    {
        cache.cache_objs[i].isvalid=0;
        cache.cache_objs[i].timeStamp=1;
        cache.cache_objs[i].readcnt=0;
        sem_init(&cache.cache_objs[i].readmutex,0,1);
        sem_init(&cache.cache_objs[i].writemutex,0,1);
    }
}

//fail to get return -1 ,else return the ordernum
int cache_search(char *url)
{
    int i;
    int m=-1;
    for(i=0;i<MAX_OBJECT_CNT;i++)
    {
        ReadModelBefore(i);
        if(cache.cache_objs[i].isvalid)
        {
            cache.cache_objs[i].timeStamp-=1;
        }
        if((strcmp(cache.cache_objs[i].cache_url,url)==0) && cache.cache_objs[i].isvalid)
        {
            cache.cache_objs[i].timeStamp=1;
            m=i;
        }
        ReadModelAfter(i);
    }
    return m;
}
void cache_exchange(char *url,char *body)
{
    int out_num,i;
    if((strlen(body)+1)>MAX_OBJECT_SIZE)
    {
        return;
    }
       for(i=0;i<MAX_OBJECT_CNT;i++)
       {
           WriteMdodelBefore(i);
           if((!cache.cache_objs[i].isvalid)||(cache.cache_objs[i].isvalid&&(cache.cache_objs[i].timeStamp==-1)))
           {
               out_num=i;
           }
           WriteMdodelAfter(i);
       }
       WriteMdodelBefore(out_num);
        strcpy(cache.cache_objs[out_num].cache_url,url);
        strcpy(cache.cache_objs[out_num].cache_obj,body);
        cache.cache_objs[out_num].timeStamp=1;
        cache.cache_objs[out_num].isvalid=1;
        WriteMdodelAfter(out_num);
}
void*thread(void*vargp)
{
    int connfd=*((int*)vargp);
    Free(vargp);
    pthread_detach(pthread_self());
    doit(connfd);
      Close(connfd);
      return NULL;
}
void sigpipe_handler(int sig)
{
  printf("sigpipe handled %d\n", sig);
  return;
}
void doit(int fd)
{
  char buf[MAXLINE];
  char host[MAXLINE],path[MAXLINE],port[5];
  //char cache[MAX_OBJECT_SIZE];
  char method[MAXLINE],uri[MAXLINE],version[MAXLINE];
  char up_buf[MAXLINE],body[MAXLINE],cp_buf[MAXLINE];
  rio_t rio,sio;
  int num;
  int up_fd;
  int turn=0;
  size_t n=0;
  Rio_readinitb(&rio,fd);
  
   if(!Rio_readlineb(&rio,buf,MAXLINE))
   {
        return;
   }
    sscanf(buf,"%s %s %s",method,uri,version);
    if(strcasecmp(method,"GET"))
    {
        clienterror(fd,method,"501","NOT Implement","Proxy does not implement this method");
        return;
    }
    read_requesthdrs(&rio);//the end of the requesting hdr consists of the\r\n,this function just make a loop and "strcmp"
    if(!parse_uri(uri,host,port,path))
    {
       clienterror(fd, uri, "404", "Not found", "Request could not be parsed");
      return;
    }

     if((num=cache_search(uri))<0)
     {
    memset(body,0,strlen(body));
    up_fd=Open_clientfd(host,port);
    Rio_readinitb(&sio,up_fd);
    sprintf(up_buf,"GET %s HTTP/1.0\r\nHost: %s\r\n%s%s%s\r\nAccept: */*",path,host,user_agent_hdr,connection_header,proxy_connection_header);
    Rio_writen(up_fd,up_buf,strlen(up_buf));
    
     while((n=Rio_readlineb(&sio,buf,MAXLINE)))
    {   
        // if(strcmp(buf,"\r\n")==0)
        // {
        //     turn=1;
        //      Rio_writen(fd,buf,n);
        //     continue;
        // }
        // if(turn)
        // {
        strcpy(cp_buf,buf);
        strcat(body,cp_buf);
        // }
        Rio_writen(fd,buf,n);
    }
      cache_exchange(uri,body);
     }
    else{
        ReadModelBefore(num);
        Rio_writen(fd,cache.cache_objs[num].cache_obj,strlen(cache.cache_objs[num].cache_obj));
        cache.cache_objs[num].timeStamp=1;
        ReadModelAfter(num);
    }
}
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
	Rio_readlineb(rp, buf, MAXLINE);
	printf("%s", buf);
    }
    return;
}
int parse_uri(char *uri, char *host, char *port,char* path) 
{
    char *ptr;
    char *tmp;
    int len;
    char scheme[10];
    ptr=uri;
    tmp=strchr(ptr,':');
    if(NULL==tmp)
    {
        return 0;
    }
    len=tmp-ptr;
 (void)strncpy(scheme,ptr,len);
  scheme[len]='\0';
  if(strcasecmp(scheme,"http"))
  {
      return 0;
  }
  tmp++;
  ptr=tmp;
  for(int i=0;i<2;i++)
  {
    if('/'!=*ptr)
    {
        return 0;
    }
    ptr++;
  }
  tmp=ptr;
  while('\0'!=(*tmp))
  {
      if(':'==(*tmp)||'/'==(*tmp))
      {
          break;
      }
      tmp++;
  }
  len=tmp-ptr;
  (void)strncpy(host,ptr,len);
  host[len]='\0';
  ptr=tmp;
if(*ptr==':')
{
    ptr++;
    tmp=ptr;
    while('\0'!=*tmp&&'/'!=*tmp)
    {
        tmp++;
    }
    len=tmp-ptr;
    (void)strncpy(port,ptr,len);
    port[len]='\0';
    ptr=tmp;
}
else{
    strcpy(port,"80");
}

if('\0'==(*tmp))
{
   strcpy(path," ");
   return 1;
}
tmp=ptr;
while('\0'!=*tmp)
{
    tmp++;
}
len=tmp-ptr;
(void)strncpy(path,ptr,len);
path[len]='\0';
return 1;
}