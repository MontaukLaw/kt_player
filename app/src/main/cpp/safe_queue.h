#ifndef MY_APPLICATION_FFMPEG_PLAYER_KT_SAFE_QUEUE_H
#define MY_APPLICATION_FFMPEG_PLAYER_KT_SAFE_QUEUE_H

// 这个队列基本上是生产者/消费者模式
#include <queue>
#include <pthread.h>

using namespace std;

#define MAX_QUEUE_SIZE 10

template<typename T>  // 泛型

class SafeQueue {
private :
    typedef void(*ReleaseCallback)(T *);  // 释放T里面的数据

    typedef void(*SyncHandle)(queue<T> &);  // 同步队列, 让外界完成队列的清理

public:
    queue<T> queue;
    pthread_mutex_t mutex;  // 安全队列的互斥锁
    pthread_cond_t cond;  // 等待和唤醒
    int work;  // 是否工作, 对队列进行控制

    ReleaseCallback releaseCallback;  // 释放T里面的数据
    SyncHandle syncHandle;            // 同步队列, 让外界完成队列的清理

    SafeQueue() {
        pthread_mutex_init(&mutex, 0);
        pthread_cond_init(&cond, 0);
        work = 1;
    }

    ~SafeQueue() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }

    void insert_to_queue(T value) {
        // 先加锁
        pthread_mutex_lock(&mutex);

        // 一旦停止工作， 就尝试销毁数据
        if (!work) {
            if (releaseCallback) {
                releaseCallback(&value);
            }
            pthread_mutex_unlock(&mutex);
            return;
        }
        // 数据加入队列
        queue.push(value);
        // 唤醒消费者
        pthread_cond_signal(&cond);
        // 解锁
        pthread_mutex_unlock(&mutex);
    }

    int get_queue_size() {
        pthread_mutex_lock(&mutex);
        int size = queue.size();
        pthread_mutex_unlock(&mutex);
        return size;
    }

    int pop_from_queue(T &value) {
        pthread_mutex_lock(&mutex);

        // 当队列为空时，等待
        while (work && queue.empty()) {
            pthread_cond_wait(&cond, &mutex);
        }
        // 如果停止工作，就释放锁并直接退出
        if (!work) {
            pthread_mutex_unlock(&mutex);
            return 0;
        }
        value = queue.front();
        queue.pop();
        pthread_mutex_unlock(&mutex);
        return 1;
    }

    void set_work(int work_) {
        pthread_mutex_lock(&mutex);
        this->work = work_;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }

    int empty(void) {
        return queue.empty();
    }

    void clear(void) {
        pthread_mutex_lock(&mutex);
        while (!queue.empty()) {
            T value = queue.front();
            queue.pop();
            if (releaseCallback) {
                releaseCallback(&value);
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    void set_release_callback(ReleaseCallback callback) {
        this->releaseCallback = callback;
    }

    bool if_queue_full(void) {

        if (get_queue_size() >= MAX_QUEUE_SIZE) {
            return true;
        }
        return false;
    }

    // 让外界完成队列的清理
    void set_sync_handle(SyncHandle handle) {
        this->syncHandle = handle;
    }

    // 同步队列
    void sync() {
        pthread_mutex_lock(&mutex);
        if (syncHandle) {
            syncHandle(queue);
        }
        pthread_mutex_unlock(&mutex);
    }
};


#endif //MY_APPLICATION_FFMPEG_PLAYER_KT_SAFE_QUEUE_H
