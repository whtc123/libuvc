#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "uvc.h"

void Producer(void *ptr){
    channel_t ch = *(int *)ptr;
    int o = 0;
    int i = 0;
    for (i = 0; i<10; i++){
        o++;
        if (channel_write(ch, (void *)&o) != 0){
            printf("channel brokern,Producer exit\n");
            break;
        }
        uvc_sleep(100);
    }
    printf("Producer send over\n");
    channel_close(ch);
    uvc_return();
}

void Consumer(void *ptr){
    
    channel_t ch = *(int *)ptr;
    int o = 0;
    while(1){
        if (channel_read(ch, (void *)&o) != 0){
            printf("channel brokern,Consumer exit\n");
            break;
        }
        printf("Consumer read %d\n",o);
    }
    printf("Consumer recv over\n");
    uvc_return();

}

int main(){
    channel_t ch;
    ch = channel_create(0, sizeof(int));
    uvc_create("Producer",10 * 1024, Producer, (void *)&ch);
    uvc_create("Consumer",10 * 1024, Consumer, (void *)&ch);
    uvc_schedule();
}
