#include "../lock/lock.h"
#include <iostream>
#include <pthread.h>
using namespace std;

locker g_lock;
int g_num = 0;

void* func(void* arg){
    for(int i = 0; i < 10000; ++i){
        g_lock.lock();
        g_num++;
        g_lock.unlock();
    }
    return nullptr;
}

int main(){
    pthread_t t1, t2;
    pthread_create(&t1, nullptr, func, nullptr);
    pthread_create(&t2, nullptr, func, nullptr);

    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);

    cout << "final num is:" << g_num << endl;

    return 0;
}