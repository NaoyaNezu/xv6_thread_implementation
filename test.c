#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void child(void *arg){
	printf("child called\n");
	int *num = (int*)arg;
	*num += 10;
	return;
}

int main(int argc, char *argv[]){
	int *num = malloc(sizeof(int));
	int ret;
	*num = 100;
	printf("before num : %d\n",*num);
	ret = create_thread(child,(void *)num);
	join(ret);
	printf("after num : %d\n",*num);
	exit(0);
}
