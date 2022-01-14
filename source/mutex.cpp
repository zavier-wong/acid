//
// Created by zavier on 2021/11/25.
//
#include <pthread.h>
#include <semaphore.h>
#include <stdexcept>
#include "acid/mutex.h"

namespace acid{

Semaphore::Semaphore(uint32_t count) {
    if(sem_init(&m_sem,0,count)){
        throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore() {
    sem_destroy(&m_sem);
}

void Semaphore::wait() {
    if(sem_wait(&m_sem)){
        throw std::logic_error("sem_wait error");
    }
}

void Semaphore::notify() {
    if(sem_post(&m_sem)){
        throw std::logic_error("sem_notify error");
    }
}

}