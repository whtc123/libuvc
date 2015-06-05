#include <stdio.h>
#include <stdlib.h>
#include "uvc.h"

void task(void *arg){
		uvc_sleep(100000);
}

int main(int argc ,char **argv){
	int i=0;
	for(i=0;i<100000;i++){
		uvc_create("worker",128,task,NULL);	
	}	
	uvc_schedule();

}
