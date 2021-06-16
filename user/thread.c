#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int create_thread(void(*fn)(void *),void *arg){
    int ret = clone();
    if(ret == 0){
        fn(arg);
        t_exit();

        //do't reach here
        exit(0);
    }
    else{
        return ret;
    }
}
