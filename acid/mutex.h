//
// Created by zavier on 2021/11/25.
//

#ifndef ACID_MUTEX_H
#define ACID_MUTEX_H
#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include "noncopyable.h"
namespace acid{
class Semaphore : Noncopyable{
public:
    Semaphore(uint32_t count = 0);
    ~Semaphore();
    void wait();
    void notify();
private:
    sem_t m_sem;
};

template<class T>
class ScopedLock{
public:
    ScopedLock(T& mutex):m_mutex(mutex){
        m_mutex.lock();
        m_locked = true;
    }
    ~ScopedLock(){
        unlock();
    }
    void lock(){
        if(!m_locked){
            m_mutex.lock();
            m_locked = true;
        }
    }
    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked = false;
};

class Mutex : Noncopyable{
public:
    using Lock = ScopedLock<Mutex>;
    Mutex(){
        pthread_mutex_init(&m_mutex, nullptr);
    }
    ~Mutex(){
        pthread_mutex_destroy(&m_mutex);
    }
    void lock(){
        pthread_mutex_lock(&m_mutex);
    }
    void unlock(){
        pthread_mutex_unlock(&m_mutex);
    }
private:
    pthread_mutex_t m_mutex;
};

class SpinLock : Noncopyable{
public:
    using Lock = ScopedLock<SpinLock>;
    SpinLock(){
        pthread_spin_init(&m_mutex,0);
    }
    ~SpinLock(){
        pthread_spin_destroy(&m_mutex);
    }
    void lock(){
        pthread_spin_lock(&m_mutex);
    }
    void unlock(){
        pthread_spin_unlock(&m_mutex);
    }
private:
    pthread_spinlock_t m_mutex;
};

template<class T>
class ReadScopedLock{
public:
    ReadScopedLock(T& mutex):m_mutex(mutex){
        m_mutex.rlock();
        m_locked = true;
    }
    ~ReadScopedLock(){
        unlock();
    }
    void lock(){
        if(!m_locked){
            m_mutex.rlock();
            m_locked = true;
        }
    }
    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked = false;
};

template<class T>
class WriteScopedLock{
public:
    WriteScopedLock(T& mutex):m_mutex(mutex){
        m_mutex.wlock();
        m_locked = true;
    }
    ~WriteScopedLock(){
        unlock();
    }
    void lock(){
        if(!m_locked){
            m_mutex.wlock();
            m_locked = true;
        }
    }
    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked = false;
};

class RWMutex : Noncopyable{
public:
    using ReadLock = ReadScopedLock<RWMutex>;
    using WriteLock = WriteScopedLock<RWMutex>;
    RWMutex(){
        pthread_rwlock_init(&m_rwmutex,nullptr);
    }
    ~RWMutex(){
        pthread_rwlock_destroy(&m_rwmutex);
    }
    void rlock(){
        pthread_rwlock_rdlock(&m_rwmutex);
    }
    void wlock(){
        pthread_rwlock_wrlock(&m_rwmutex);
    }
    void unlock(){
        pthread_rwlock_unlock(&m_rwmutex);
    }
private:
    pthread_rwlock_t m_rwmutex;
};
}
#endif //ACID_MUTEX_H
