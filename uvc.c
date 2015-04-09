#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <uv.h>
#include "coro.h"
#include <assert.h>
#include "uvc.h"
#include "queue.h"


#define UV_TASK_STACK_SIZE 1024*1024
//----------------------------------------------defaultloop----------------------------------------------------
static uv_key_t uvc_key;
static uv_once_t once;


typedef struct {
	size_t size;
	size_t cnt;
	channel_t id;

	int start;
	int end;
	int cur_cnt;
	queue_t readq;
	queue_t writq;
	int closeing;
	//void cbuf[0];/*for unbuffered euqe to*/
	uint8_t buf[0];/*for buffered copy to*/
}channel;

#define MAX_CHANNEL_POOL 10240

/*为了达到O1的效率，使用数组*/
/*为了防止被释放的chan index，被新的chan占用，
在chan中记录了ID，查找后进行对比，ID不会重复
*/
struct channel_pool_s{
	uint32_t current_empty;
	uint32_t maxid;
	channel *channels[MAX_CHANNEL_POOL];
	int cnt;
};
typedef struct channel_pool_s channel_pool;
typedef struct {
	uv_loop_t *loop;
	coro_context ctx;
	channel_pool pool;
	uv_timer_t schedule_timer;
	uvc_ctx *uv_task;
	uvc_ctx *schedule_task;
	uvc_ctx *runing_task;
	queue_t ready_queue;
	queue_t pending_queue;
	//stacks
}uvc_thread_env;

void uvc_init(void){
	uv_key_create(&uvc_key);
}

static uvc_thread_env *uvc_get_env(){
	uvc_ctx *ctx = NULL;
	uv_once(&once,uvc_init);
	uvc_thread_env *env=(uvc_thread_env *)uv_key_get(&uvc_key);
	if(env==NULL){
		env=(uvc_thread_env *)malloc(sizeof(uvc_thread_env));
		memset(env,0,sizeof(uvc_thread_env));
		env->loop = uv_loop_new();
		queue_init(&env->pending_queue);
		queue_init(&env->ready_queue);
		
		ctx = (uvc_ctx *)malloc(sizeof(uvc_ctx));
		memset(ctx, 0, sizeof(uvc_ctx));
		coro_stack_alloc(&ctx->stack, 0);
		coro_create(&ctx->cur, NULL, NULL, ctx->stack.sptr, ctx->stack.ssze);
		sprintf(ctx->name, "ROOT");
		env->schedule_task = ctx;
		env->runing_task = ctx;
		ctx->status = UVC_STATUS_RUNING;
		uv_key_set(&uvc_key,env);
	}
	return env;
}

uv_loop_t* uvc_loop_default(){
	uvc_thread_env *env=uvc_get_env();
#ifdef UVC_DEBUG
	if(env ==NULL || env->loop==NULL){
		assert("env ==NULL || env->loop==NULL");
	}
#endif
	return env->loop;
}

static channel_pool *get_chan_pool(){
	uvc_thread_env *env=uvc_get_env();
	return &env->pool;
}



static uvc_ctx *uvc_self(){
	uvc_thread_env *env=uvc_get_env();
#ifdef UVC_DEBUG
	if(env ==NULL || queue_empty(&env->stack)){
		assert("env ==NULL || env->stack.top==NULL || env->stack.top->ctx ==NULL");
	}
#endif
	return env->runing_task;
}



#if OLD_LIBUV
static void schedule_timer_cb(uv_timer_t *timer,  int status){
#else
void schedule_timer_cb(uv_timer_t *timer){
#endif
	return;
}

void uvc_task_uv(void *ptr){
	uvc_thread_env *env = uvc_get_env();
	env->uv_task = uvc_self();
	uv_run(env->loop, UV_RUN_DEFAULT);
	env->uv_task = NULL;
	uvc_return();
}

void uvc_schedule(){
	uvc_ctx *ctx;
	queue_t *node;
	uvc_thread_env *env;
	env = uvc_get_env();
	for (;;){
		if (!queue_empty(&env->ready_queue) ){
			node = queue_last(&env->ready_queue);
			queue_remove( node);
			ctx = queue_data(node, uvc_ctx, task_node);
		}
		else{
			//只有当没有ready任务时才运行uvloop
			if (env->uv_task){
				ctx = env->uv_task;
			}
			else{
				printf("no task need run ,exit\n");
				exit(0);
			}
		}
		
		if (ctx != NULL){
			env->runing_task = ctx;
			ctx->status = UVC_STATUS_RUNING;
			//printf("task[%s] runing\n",ctx->name);
			uvc_resume(ctx);
			//printf("task[%s] stoping\n",ctx->name);
		}
		if (ctx->status == UVC_STATUS_DIE){
			coro_stack_free(&ctx->stack);
			free(ctx);
		}else{
			ctx->status = UVC_STATUS_PENDING;
		}
		

	}

}

//----------------------------------------------base----------------------------------------------------


void uvc_ctx_set_name(char *name){
	uvc_ctx *ctx = uvc_self();
	sprintf(ctx->name, "%s",name);
}

char *uvc_ctx_get_name(){
	uvc_ctx *ctx = uvc_self();
	return ctx->name;
}

#define YIELD(e) coro_transfer(&(e)->runing_task->cur, &(e)->schedule_task->cur)
#define RESUME(e,c) coro_transfer( &(e)->schedule_task->cur,&(c)->cur)

void uvc_return(){
	uvc_ctx *ctx=uvc_self();
	uvc_thread_env *env = uvc_get_env();
	env->runing_task->status= UVC_STATUS_DIE;
	printf("task[%s] exit\n",uvc_ctx_get_name());
	YIELD(env);
	//TODO 当前协程先出栈，然后resume到专门释放协程的协程。
}

void uvc_io_ready(uvc_ctx *ctx){
	uvc_thread_env *env = uvc_get_env();
	queue_insert_head(&env->ready_queue, &ctx->task_node);
	YIELD(env);;
}

void uvc_yield(){
	uvc_thread_env *env=uvc_get_env();
	YIELD(env);;
}

void uvc_ready(uvc_ctx *ctx){
	if (ctx){
		uvc_thread_env *env = uvc_get_env();
		queue_insert_head(&env->ready_queue, &ctx->task_node);
	}
}

void uvc_resume(uvc_ctx *ctx){
	uvc_thread_env *env=uvc_get_env();
	ctx->status = UVC_STATUS_RUNING;
	env->runing_task = ctx;
	RESUME(env, ctx);
}

void uvc_switch(uvc_ctx *prev, uvc_ctx *next){
	printf("[switch]  %s  ->  %s\n", prev->name, next->name);
	uvc_thread_env *env = uvc_get_env();
	next->status = UVC_STATUS_RUNING;
	prev->status = UVC_STATUS_READY;
	env->runing_task = next;
	coro_transfer(&prev->cur, &next->cur);
}

void uvc_create(char *name, unsigned int size, coro_func func, void *arg){
	uvc_ctx *ctx = (uvc_ctx *)malloc(sizeof(uvc_ctx));
	memset(ctx, 0, sizeof(uvc_ctx));
	//ctx->data=arg;
	coro_stack_alloc(&ctx->stack, size);
	coro_create(&ctx->cur, func, arg, ctx->stack.sptr, ctx->stack.ssze);
	if (name == NULL || strlen(name) == 0){
		sprintf(ctx->name, "coro");
	}
	else{
		sprintf(ctx->name, name);
	}
	uvc_ready(ctx);
	//uvc_resume(ctx);

	return;
}


static void uvc_timer_close_cb(uv_handle_t *handle){
	uvc_io_ready((uvc_ctx *)handle->data);
}

#if OLD_LIBUV
static void uvc_timer_cb(uv_timer_t* handle, int status){
#else
static void uvc_timer_cb(uv_timer_t* handle){
#endif
	uvc_io_ready((uvc_ctx *)handle->data);
}

void uvc_sleep(uint64_t msec){
	uvc_thread_env *env = uvc_get_env();
	uv_timer_t timer;
	timer.data = uvc_self();
	uv_timer_init(uvc_loop_default(), &timer);
	uv_timer_start(&timer, uvc_timer_cb, msec, 0);

	if (env->uv_task == NULL){
		uvc_create("UV_LOOP", UV_TASK_STACK_SIZE, uvc_task_uv, NULL);
	}

	uvc_yield();
	uv_close((uv_handle_t *)&timer, uvc_timer_close_cb);
	uvc_yield();
}

int uvc_io_create(uvc_io *io, uvc_io_type_t type)
{
	uv_handle_t *h;
	uvc_thread_env *env = uvc_get_env();
	
	memset(io,0,sizeof(uvc_io));
	switch(type){
	case UVC_IO_TCP:
		h = (uv_handle_t *)malloc(sizeof(uv_tcp_t));
		uv_tcp_init(uvc_loop_default(),(uv_tcp_t *)h);
		//io->cur=ctx;
		io->handle=h;
		break;
	case UVC_IO_FS:
		 h = (uv_handle_t *)malloc(sizeof(uv_fs_t));
		io->handle=h;
		break;

	default:
		assert("unknown handle type");
	}
	if (env->uv_task == NULL){
		uvc_create("UV_LOOP", UV_TASK_STACK_SIZE, uvc_task_uv, NULL);
	}
	return 0;
}

int uvc_tcp_bind(uvc_io *io,char *ip,short port){
	int status;
	struct sockaddr_in addr;
	status = uv_ip4_addr(ip,port,&addr);
	if (status){
		return status;
	}
#if OLD_LIBUV
    return uv_tcp_bind((uv_tcp_t*)io->handle,(const struct sockaddr *)&addr);
#else
    return uv_tcp_bind((uv_tcp_t*)io->handle,(const struct sockaddr *)&addr,0);
#endif
}

static void uvc_alloc_cb(uv_handle_t* handle,size_t s,uv_buf_t* buf){
	uvc_io *io= (uvc_io *)handle->data;
	buf->base=io->buf.base;
	buf->len=io->buf.len;

}
static void uvc_read_cb(uv_stream_t* stream,ssize_t nread,const uv_buf_t* buf)
{
	uvc_io *io= (uvc_io *)stream->data;
	io->buf.base=buf->base;
	io->buf.len=buf->len;
	io->nread=nread;
	uvc_io_ready(io->cur);
}

static void uvc_write_cb(uv_write_t* req, int status)
{
	uvc_io *io= (uvc_io *)req->data;
	io->return_status = status;
	uvc_io_ready(io->cur);
}

static void uvc_close_cb(uv_handle_t* handle)
{
	//uvc_io *io=uvc_container_of(handle,uvc_io,handle);
	uvc_io *io=(uvc_io *)handle->data;
	free(io->handle);
	uvc_io_ready(io->cur);
}

static void uvc_connect_cb(uv_connect_t* req, int status)
{
	uvc_io *io=(uvc_io *)req->data;
	io->return_status = status;
	uvc_io_ready(io->cur);
}

static void uvc_connection_cb(uv_stream_t* server, int status)
{

	uvc_io *io=(uvc_io *)server->data;
	//uvc_io *io=((uvc_io*)(((char*)(server)) - offsetof(uvc_io, handle)));
	io->return_status=status;
	uvc_io_ready(io->cur);
}

static void uvc_fs_cb(uv_fs_t* req)
{
	uvc_io *io=(uvc_io *)req->data;
	uvc_io_ready(io->cur);
}
static void uvc_fs_cb2(uv_fs_t* req)
{
	uvc_ctx *ctx=(uvc_ctx *)req->data;
	uvc_io_ready(ctx);
}

static void uvc_after_work_cb(uv_work_t* req, int status)
{
	uvc_ctx *ctx =(uvc_ctx *)req->data;
	uvc_io_ready(ctx);
}
#ifdef OLD_LIBUV
static void uvc_iotimer_cb(uv_timer_t* handle, int status)
#else
static void uvc_iotimer_cb(uv_timer_t* handle)
#endif
{
	uvc_io *io= (uvc_io *)handle->data;
	io->timeout=1;
	uvc_io_ready(io->cur);
}

//----------------------------------------------network----------------------------------------------------

ssize_t uvc_read(uvc_io *io,void *data,size_t len){
	ssize_t nread=0;
	io->buf.base=(char *)data;
	io->buf.len=len;
	io->handle->data=io;
	nread=uv_read_start((uv_stream_t *)io->handle,uvc_alloc_cb,uvc_read_cb);
	if(nread ==UV_EOF){
		return nread;
	}
	io->cur =uvc_self();
	uvc_yield();
	uv_read_stop((uv_stream_t *)io->handle);
	return io->nread;
}

ssize_t uvc_read2(uvc_io *io,void *data,size_t len,uint64_t timeout){
	ssize_t nread=0;
	io->buf.base=(char *)data;
	io->buf.len=len;

	uv_timer_t timer;
	timer.data=io;
	io->timeout=0;
	uv_timer_init(uvc_loop_default(),&timer);
	uv_timer_start(&timer,uvc_iotimer_cb,timeout,0);

	nread=uv_read_start((uv_stream_t *)io->handle,uvc_alloc_cb,uvc_read_cb);
	if(nread ==UV_EOF){
		return nread;
	}
	io->cur =uvc_self();
	uvc_yield( );
	uv_read_stop((uv_stream_t *)io->handle);
	if(io->timeout == 0){
		return ETIMEDOUT;
	}
	return io->nread;
}

ssize_t uvc_write(uvc_io *io,void *data,size_t len){
	uv_buf_t buf;
	uv_write_t req;
	ssize_t nwrite=0;
	buf.base=(char *)data;
	buf.len=len;
	req.data=io;
	uv_write(&req,(uv_stream_t *)io->handle,&buf,1,uvc_write_cb);
	io->cur =uvc_self();
	uvc_yield( );
	return (ssize_t)io->return_status;
}

void uvc_close(uvc_io *io){
	io->handle->data=io;
	uv_close(io->handle,uvc_close_cb);
	io->cur =uvc_self();
	uvc_yield( );
}

int uvc_tcp_connect(uvc_io *io,char *ip,short port){
	int status;
	uv_connect_t req;
	struct sockaddr_in addr;
	status = uv_ip4_addr(ip,port,&addr);
	if (status){
		return status;
	}

	req.data = io;
	uv_tcp_connect(&req, (uv_tcp_t *)io->handle, (const struct sockaddr*)&addr, uvc_connect_cb);
	io->cur =uvc_self();
	uvc_yield( );
	return io->return_status;
}

int uvc_listen(uvc_io *io,int backlog){
	io->handle->data=io;
	uv_listen((uv_stream_t *)io->handle,backlog,uvc_connection_cb);
	io->cur =uvc_self();
	uvc_yield( );
	return io->return_status;
}

int uvc_accept( uvc_io *io,uvc_io *c){
	return uv_accept((uv_stream_t *)io->handle,(uv_stream_t *)c->handle);
}

//----------------------------------------------filesystem--------------------------------------------------


uv_file uvc_fs_open(uvc_io *io,char *path,int flasgs){
	io->handle->data=io;
	uv_fs_open(uvc_loop_default(),(uv_fs_t *)io->handle,path,flasgs,0,uvc_fs_cb);
	io->cur =uvc_self();
	uvc_yield( );
	io->file = ((uv_fs_t *)(io->handle))->result;
	uv_fs_req_cleanup((uv_fs_t *)io->handle);
	return io->file;
}

int uvc_fs_read(uvc_io *io,void *data,ssize_t size){
	io->handle->data=io;
	uv_buf_t buf;
	buf.len=size;
	buf.base=data;
	uv_fs_read(uvc_loop_default(),(uv_fs_t *)io->handle,io->file,&buf,1,-1,uvc_fs_cb);
	io->cur =uvc_self();
	uvc_yield( );
	uv_fs_req_cleanup((uv_fs_t *)io->handle);
	return ((uv_fs_t *)(io->handle))->result;;
}

int uvc_fs_write(uvc_io *io,void *data,ssize_t size){
    uv_buf_t buf;
	io->handle->data=io;
    buf.len=size;
    buf.base=data;
	uv_fs_write(uvc_loop_default() ,(uv_fs_t *)io->handle,io->file,&buf,1,-1,uvc_fs_cb);
	io->cur =uvc_self();
	uvc_yield( );
	return ((uv_fs_t *)(io->handle))->result;;
}

int uvc_fs_close(uvc_io *io){
	io->handle->data=io;
	uv_fs_close(uvc_loop_default(),(uv_fs_t *)io->handle,io->file,uvc_fs_cb);
	io->cur =uvc_self();
	uvc_yield();
	uv_fs_req_cleanup((uv_fs_t *)io->handle);
	free(io->handle);
	return 0;
}

int uvc_fs_stat( char *path,uv_stat_t *statbuf){
	uv_fs_t req;
	req.data=uvc_self();
	uv_fs_stat(uvc_loop_default(),&req,path,uvc_fs_cb2);

	uvc_yield();
	memcpy(statbuf,&req.statbuf,sizeof(uv_stat_t));
	uv_fs_req_cleanup(&req);
	return req.result;
}

//----------------------------------------------queue work--------------------------------------------------

int uvc_queue_work( uv_work_cb cb){
	uv_work_t req;
	req.data=uvc_self();
	uv_queue_work(uvc_loop_default(),&req,cb,uvc_after_work_cb);
	uvc_yield();
	return 0;
}


//----------------------------------------------channel----------------------------------------------------
#define chanbuf_next_start(c) (((c)->start + 1) % (c)->cnt)
#define chanbuf_next_end(c) (((c)->end + 1) % (c)->cnt)
#define chanbuf_empty(c) ((c)->start == (c)->end)
#define chanbuf_full(c)  ((c)->cnt==0? 1:(chanbuf_next_end(c) == (c)->start))

static void chanbuf_push(channel *chan,uint8_t *buf){
	if(!chanbuf_full(chan)){
		memcpy(chan->buf+chan->end*chan->size,buf,chan->size);
		chan->end=chanbuf_next_end(chan);
	}
}

static void chanbuf_pop(channel *chan,uint8_t *buf){
	if(!chanbuf_full(chan)){
		memcpy(buf,chan->buf+chan->start*chan->size,chan->size);
		chan->start=chanbuf_next_start(chan);
	}
}



channel_pool pool;

//#define channel_pool_get(p,i) (p)->channels[(i)%MAX_CHANNEL_POOL]
#define channel_pool_put(p,i,c) (p)->channels[(i)%MAX_CHANNEL_POOL]=(c)
#define channel_pool_remove(p,i) (p)->channels[(i)%MAX_CHANNEL_POOL]=NULL
static channel *channel_pool_get(channel_pool *pool, channel_t i){
	channel *chan = pool->channels[(i) % MAX_CHANNEL_POOL];
	if (chan && chan->id == i){
		return chan;
	}
	return NULL;
	
}

static void channel_queue_put(queue_t *q, uvc_ctx *ctx){
	queue_insert_head(q, &ctx->i_node);
}

static uvc_ctx *channel_queue_get(queue_t *q){
	queue_t *node = NULL;
	node = queue_last(q);
	queue_remove(node);
	/*is selected*/
	if (node->ext == NULL){
		return queue_data(node, uvc_ctx, i_node);
	}
	else{
		return (uvc_ctx *)node->ext;
	}
	
}

static int find_empty_slot(channel_pool *pool){
	int i=0;

	for (i = pool->current_empty + 1; i != pool->current_empty; i++){
		pool->maxid++;
		if(channel_pool_get(pool,i)==NULL){
			pool->current_empty = i;
			return pool->maxid;
		}
	}
	return -1;
}

int channel_is_closed(channel_t c){
	channel *chan = channel_pool_get(get_chan_pool(),c);
	if(chan!=NULL && chan->id == c){
		return 1;
	} 
	return 0;
} 




channel_t channel_create(int cnt,int elem_size){

	channel *c=NULL;
	int idx=0;
	channel_pool *pool=get_chan_pool();
	idx=find_empty_slot(pool);
	if(idx<0){
		return -1;
	}
	c=malloc(sizeof(channel) +(elem_size*cnt));
	memset(c,0,sizeof(channel));
	c->size=elem_size;
	c->cnt=cnt;
	c->id = idx;
	queue_init(&c->readq);
	queue_init(&c->writq);
	
	channel_pool_put(pool,idx,c);
	return idx;
}

int channel_close(channel_t c){
	channel_pool *pool=get_chan_pool();
	channel *chan;
	uvc_ctx *ctx;
	uvc_ctx *ctx_self =uvc_self();
	chan=channel_pool_get(pool,c);
	if(chan == NULL ||chan->id !=c || chan->closeing==1){
		return -1;
	}
	chan->closeing=1;
	
	
	//唤醒所有发送队列，让发生者知道已经发送失败。
	do
    {
        if(queue_empty(&chan->writq) )break;
		ctx = channel_queue_get(&chan->writq);
		uvc_ready(ctx);
		uvc_ready(ctx_self);
		uvc_yield();
    }while(1);


	//如果chanbuf中还有数据，那么不能立即关闭管道，
	//否则发送放无法获知释放已经送达数据。
	if (chanbuf_empty(chan) && queue_empty(&chan->readq)){
		channel_pool_remove(pool, c);
		free(chan); 
	}else{
		do
	    {
	        if(queue_empty(&chan->readq) )break;
			ctx = channel_queue_get(&chan->readq);
			if (ctx!=ctx_self){
				printf("channel closeing,wakeup task[%s]\n",ctx->name);
				uvc_ready(ctx);
				uvc_ready(ctx_self);
				uvc_yield();
			}
	    }while(1);
	}
	
	return 0;
}

int channel_write(channel_t c,void *buf){
	channel *chan = NULL;
	uvc_ctx *ctx;
	void *p=NULL;
	chan = channel_pool_get(get_chan_pool(), c);
	if(chan == NULL ||chan->id !=c || chan->closeing==1){
		return -1;
	}

	if(!chanbuf_full(chan)){
		//buffered channel,and buffer not full
		chanbuf_push(chan, buf);
		if(!queue_empty(&chan->readq)){
			ctx = channel_queue_get(&chan->readq);
			uvc_ready(ctx);
		}
		//写入buffer的数据不管
		return 0;
	}else{
		ctx = uvc_self();
		ctx->cbuf = buf;
		channel_queue_put(&chan->writq, ctx);
		//当readq不为空的时候，writeq一定为空
		if(queue_empty(&chan->readq)){
			uvc_yield();
		}else{
			ctx = channel_queue_get(&chan->readq);
			uvc_ready(ctx);
		}
	}
	//检查管道释放
	if(chan == NULL ||chan->id !=c || chan->closeing==1){
		return -1;
	}
	return 0;
}

int channel_read(channel_t c,void *buf){
	channel *chan = NULL;
	uvc_ctx *ctx=uvc_self();
	void *p=NULL;
	queue_t *node = NULL;
	channel_pool *pool = get_chan_pool();
	chan = channel_pool_get(pool,c);
	if(chan == NULL ||chan->id !=c)return -1;
	//当管道关闭的时候，buf中数据任然可读
	if (!chanbuf_empty(chan) ){
		chanbuf_pop(chan,buf);
		if(chan->closeing==1 &&chanbuf_empty(chan)){
			//管道关闭状态，且buf中数据已经读完，失败chan
			free(chan); 
			channel_pool_remove(pool,c);
			return -1;
		}
		return 0;
	}
	else if (chan->closeing == 1){
		return -1;
	}
	
	// 没有协程等待写，入队，调度
	if(queue_empty(&chan->writq) ){
		channel_queue_put(&chan->readq, ctx);
		//queue_add(&chan->readq, &ctx->i_node);
		uvc_yield();
		if(chan == NULL ||chan->id !=c || chan->closeing==1){
			return -1;
		}
		ctx = channel_queue_get(&chan->writq);
        
	}else {
		ctx = channel_queue_get(&chan->writq);
		uvc_ready(ctx);
		//uvc_ready(uvc_self());
		//uvc_yield();
		//uvc_io_ready(ctx);
		//uvc_resume(ctx);
	}
	memcpy(buf, ctx->cbuf, chan->size);
	return 0;

}

int channel_readable(channel_t c){
	channel *chan = NULL;
	chan=channel_pool_get(get_chan_pool(),c);
	if(chan == NULL||chan->id !=c)return -1;
	if (!queue_empty(&chan->writq) || !chanbuf_empty(chan)){
		return 1;
	}
	return 0;
}

int channel_writeable(channel_t c){
	channel *chan = NULL;
	chan = channel_pool_get(get_chan_pool(), c);
	if(chan == NULL||chan->id !=c)return -1;
	if (!queue_empty(&chan->readq) || !chanbuf_full(chan)){
		return 1;
	}
	return 0;
}


int channel_select_remove(channel *chan){
	uvc_ctx *ctx=uvc_self();
	//queue_t *node;
	queue_t *q = queue_head(&chan->readq);  
   for (; q != queue_sentinel(&chan->readq); q = queue_next(&chan->readq))  {
   		if(q->ext == ctx){
   			queue_remove(q);
   		}
   }

   for (; q != queue_sentinel(&chan->writq); q = queue_next(&chan->writq))  {
   		if(q->ext == ctx){
   			queue_remove(q);
   		}
   }
   return 0;
}

#define HEAP_SELECT_CNT 20
channel_t channel_select(int need_default,char *fmt,...){
	va_list argp;
	int cnt = strlen(fmt);
	queue_t select_node[HEAP_SELECT_CNT];
	channel_t channels[HEAP_SELECT_CNT];
	channel *chan=NULL;
	uvc_ctx *ctx=uvc_self();
	channel_t c;
	int i = 0;
	if (cnt < 0)return -1;
	
	va_start(argp, fmt);
	for (i = 0; i<cnt; i++)
	{
		c = va_arg(argp, channel_t);
		channels[i] = c;
		select_node[i].ext = ctx;
		if (fmt[i] == 'r' && channel_readable(c)){
			return c;
		}
		else if (fmt[i] == 'w' && channel_writeable(c)){
			return c;
		}
		else if (fmt[i] != 'w' && fmt[i] != 'r'){
			abort();
		}
	}
	va_end(argp);
	if (need_default){
		return -1;
	}
   
	for(i=0;i<cnt;i++)
	{
		c = channels[i];

	    if(fmt[i]=='r' ){
			chan = channel_pool_get(get_chan_pool(), c);
			queue_insert_head(&chan->readq, &select_node[i]);
	    }
	    else if(fmt[i]=='w'){
			chan = channel_pool_get(get_chan_pool(), c);
			queue_insert_head(&chan->writq, &select_node[i]);
		}else if(fmt[i] != 'w' && fmt[i] != 'r'){
			abort();
	    }
	}
	uvc_yield();
	c = 0;
	for (i = 0; i < cnt; i++)
	{
			
		if (fmt[i] == 'r' && channel_readable(channels[i])){
			assert(c == 0);
			c = channels[i];
		}
		else if (fmt[i] == 'w' && channel_writeable(channels[i])){
			assert(c == 0);
			c = channels[i];
		}
		else{
			queue_remove(&select_node[i]);
		}
	}
	assert(c != 0);
	//assert("won't be run here :\n");


    return c;    
}



//-----------------------------------------thread env-----------------------------------------------

