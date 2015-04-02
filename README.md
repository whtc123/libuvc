# libuvc
>Coroutines and asychronous I/O  for  cross-platform


a libuv and libcoro bind lib,help you write synchronization no-callback high-performance network Program. my goal is a network and coroutines framework for embedded system or pc.

this lib has tested on linux and windows.

##example for http get download file

```C
static void download(void *ptr){

    uvc_io *fs = malloc(sizeof(uvc_io));
    uvc_io_create(fs,UV_FS);
    uvc_io *io=ptr;
    ssize_t cnt=0;
    char buf[256];
     cnt = uvc_read(io,buf,sizeof(buf));
     if(cnt <=0){
         goto err;
     }
     sprintf(buf,"HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: application/zip\r\n\r\n",2735243);
    cnt=uvc_write(io,buf,strlen(buf));
    if(cnt!=0){
        goto err;
    }
    if(uvc_fs_open(fs,"/opt/nfshost/master.zip",O_RDONLY) <0){
        printf("uvc_fs_open error\n");
        goto err;
    }
    while(1){
        cnt = uvc_fs_read(fs,buf,sizeof(buf));
        if(cnt>0){
             //printf("uvc_fs_read ok\n");
            cnt=uvc_write(io,buf,cnt);
            if(cnt!=0){
                 printf("write file err\n");
                goto err;
            }
        }else{
             printf("uvc_fs_read err:%d\n",cnt);
            break;
        }


    }
err:
    uvc_fs_close(fs);
    free(fs);
    uvc_close(io);
    free(io);
    printf("connection exit\n");
    uvc_return();
}

void server(void *ptr)
{
    int ret=0;
    uvc_ctx *ctx_client;
    uvc_io io;
    uvc_io *io_client;
    uvc_io_create(&io,UV_TCP);
    ret = uvc_tcp_bind(&io,"0.0.0.0",8080);
    if(ret!=0){
        printf("error bind:%d\n",ret);
        exit(1);
    }
    printf("start listen\n");
    while(1){
        ret = uvc_listen(&io,100);
        if(ret!=0){
            printf("error listen:%d\n",ret);
            exit(1);
        }

        io_client = (uvc_io *)malloc(sizeof(uvc_io));
        uvc_io_create(io_client,UV_TCP);
        ret = uvc_accept(&io,io_client);
        if(ret !=0){
            printf("error accept:%d\n",ret);
            exit(1);
        }
        //printf("get a new connection\n");
        ctx_client =uvc_create(10*1024,download,io_client);
        uvc_resume(ctx_client);
    }
    uvc_close(&io);
    uvc_return();
}


int main(){

    uvc_ctx *server_ctx = uvc_create(128,server,NULL);
    uvc_resume(server_ctx);
    uvc_schedule();
}
```

##Feature highlights

*Cross-platform and embedded system support
*lightweight 
*Asynchronous for every IO
*synchronization logic none-callback.
*Multithreading Support(one thread more coroutines,or more thread more coroutines).
*channels like go chan. buffred channel or unbuffred channel(not finish current ).

##api
```c
uvc_ctx *uvc_create(unsigned int size,coro_func func,void *arg);
void uvc_return( );
void uvc_yield( );
void uvc_resume(uvc_ctx *ctx);
void uvc_schedule();
int uvc_io_create(uvc_io *io, uv_handle_type type);
/*TCP*/
int uvc_tcp_bind(uvc_io *io, char *ip, short port);
ssize_t uvc_read(uvc_io *io,void *data,size_t len);
ssize_t uvc_read2(uvc_io *io,void *data,size_t len,uint64_t timeout);
ssize_t uvc_write(uvc_io *io,void *data,size_t len);
void uvc_close(uvc_io *io);
int uvc_tcp_connect(uvc_io *io,char *ip,short port);
int uvc_listen(uvc_io *io,int backlog);
int uvc_accept( uvc_io *io,uvc_io *c);
/*filesystem*/
uv_file uvc_fs_open(uvc_io *io,char *path,int flasgs);
int uvc_fs_read(uvc_io *io,void *data,ssize_t size);
int uvc_fs_write(uvc_io *io,void *data,ssize_t size);
int uvc_fs_close(uvc_io *io);
int uvc_fs_stat(char *path,uv_stat_t *statbuf);
/*channels*/
channel_t channel_create(int cnt,int elem_size);
int channel_close(channel_t c);
int channel_write(channel_t c,void *buf);
int channel_read(channel_t c,void *buf);
channel_t channel_select(int need_default,char *fmt,...);
```
## build
###linux 
    * build install libuv ,see https://github.com/libuv/libuv
    * make


