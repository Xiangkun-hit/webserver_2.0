#include "../blockqueue.h"
#include <iostream>
#include <pthread.h>

using namespace std;

block_queue<int> g_queue(10);

void* producer(void* arg){
    for(int i = 1; i <= 5; ++i){
        g_queue.push(i);
        cout << "生产：" << i << endl;
    }
    return nullptr;
}

void* consumer(void* arg){
    int val;
    while(g_queue.pop(val)){
        cout << "消费：" << val << endl;
    }
    return nullptr;
}

int main(){
    pthread_t t1, t2;
    pthread_create(&t1, nullptr, producer, nullptr);
    pthread_create(&t2, nullptr, consumer, nullptr);

    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);

    return 0;
}