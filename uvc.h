#ifndef _UVC_H
#define _UVC_H
#include <uv.h>
#include "coro.h"
#include "queue.h"
#if __cplusplus
extern "C" {
#endif
typedef enum uvc_io_type_s{
	UVC_IO_TCP,
	UVC_IO_UDP,
	UVC_IO_STREAM,
	UVC_IO_FS
}uvc_io_type_t;

typedef enum uvc_status_s{
	UVC_STATUS_INIT,
	UVC_STATUS_PENDING,
	UVC_STATUS_READY,
	UVC_STATUS_RUNING,
	UVC_STATUS_DIE
}uvc_status;

struct _uvc_ctx{
	char name[64];
	coro_context *prev;
	coro_context cur;
	struct coro_stack stack;
	uv_timer_t timer;
	void *data;
	queue_t i_node;//for channel queue
	//queue_t s_node;//for stack queue
	queue_t task_node;
	uvc_status status;
	void *cbuf;
	
};
typedef struct _uvc_ctx uvc_ctx;
void uvc_init();
void uvc_create(char *name, unsigned int size, coro_func func, void *arg);
void uvc_ctx_set_name(char *name);
char *uvc_ctx_get_name();
void uvc_return( );
void uvc_yield( );
void uvc_resume(uvc_ctx *ctx);
void uvc_switch(uvc_ctx *prev,uvc_ctx *next);
void uvc_schedule();
typedef struct   _uvc_io{
	uvc_ctx *cur;
	uv_buf_t buf;
	ssize_t nread;
	int return_status;
	uv_handle_t *handle;
	int timeout;
	uv_file file;
}uvc_io;
uv_loop_t* uvc_loop_default();
void uvc_sleep(uint64_t msec);
int uvc_io_create(uvc_io *io, uvc_io_type_t type);
int uvc_tcp_bind(uvc_io *io, char *ip, short port);
ssize_t uvc_read(uvc_io *io,void *data,size_t len);
ssize_t uvc_read2(uvc_io *io,void *data,size_t len,uint64_t timeout);
ssize_t uvc_write(uvc_io *io,void *data,size_t len);
void uvc_close(uvc_io *io);
int uvc_tcp_connect(uvc_io *io,char *ip,short port);
int uvc_listen(uvc_io *io,int backlog);
int uvc_accept( uvc_io *io,uvc_io *c);
uv_file uvc_fs_open(uvc_io *io,char *path,int flasgs);
int uvc_fs_read(uvc_io *io,void *data,ssize_t size);
int uvc_fs_write(uvc_io *io,void *data,ssize_t size);
int uvc_fs_close(uvc_io *io);
int uvc_fs_stat(char *path,uv_stat_t *statbuf);
int uvc_queue_work(uv_work_cb cb);

typedef int32_t channel_t;
channel_t channel_create(int cnt, int elem_size);
int channel_close(channel_t c);
int channel_write(channel_t c,void *buf);
int channel_read(channel_t c,void *buf);
channel_t channel_select(int need_default, char *fmt, ...);


#if __cplusplus
}
#endif


#endif
