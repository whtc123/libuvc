
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "uvc.h"


void worker(void *ptr){
	uvc_io *io=(uvc_io *)ptr;
	char buf[256];
	ssize_t cnt=0;
	while(1){
		cnt = uvc_read(io,buf,sizeof(buf));
		if(cnt <=0){
			break;
		}
		buf[cnt]='\0';
		if(buf[0]=='q'){break;}
		//printf("server read: %s\n",buf);

		cnt=uvc_write(io,buf,strlen(buf));
		if(cnt!=0){
			break;
		}
	}
	uvc_close(io);
	free(io);
	printf("connection exit\n");
	uvc_return();
}

void http_hello(void *ptr){

	uvc_io *io=ptr;
	ssize_t cnt=0;
	char buf[256];
	 cnt = uvc_read(io,buf,sizeof(buf));
	 if(cnt <=0){
		 goto err;
	 }	
	 sprintf(buf,"HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html; Charset=gb2312\r\n\r\n%s",strlen("hello wrold!"),"hello wrold!");
	 cnt=uvc_write(io,buf,strlen(buf));
	 if(cnt!=0){
        goto err;
     }
	
err:
	uvc_close(io);
    free(io);
    //printf("connection exit\n");
    uvc_return();
}



void download(void *ptr){

	uvc_io *fs = malloc(sizeof(uvc_io));
	uvc_io_create(fs,UVC_IO_FS);
	uvc_io *io=ptr;
	ssize_t cnt=0;
	char buf[2048];
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
	uvc_io io;
	uvc_io *io_client;
	uvc_io_create(&io,UVC_IO_TCP);
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
		uvc_io_create(io_client,UVC_IO_TCP);
		ret = uvc_accept(&io,io_client);
		if(ret !=0){
			printf("error accept:%d\n",ret);
			exit(1);
		}
		//printf("get a new connection\n");
		uvc_create("hello",10*1024,download,io_client);
	}
	uvc_close(&io);
	uvc_return();
}

int main(){

	uvc_create("listen",128,server,NULL);
	uvc_schedule();	
}
