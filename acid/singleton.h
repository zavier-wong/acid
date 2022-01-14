//
// Created by zavier on 2021/10/20.
//

#ifndef ACID_SINGLETON_H
#define ACID_SINGLETON_H

#include <memory>
#include "thread.h"
namespace acid{
template<typename T>
class Singleton{
public:
    static T* GetInstance(){
        static T instance;
        return &instance;
    }

};

template<typename T>
class SingletonPtr{
public:
    static std::shared_ptr<T> GetInstance(){
        static std::shared_ptr<T> instance(std::make_shared<T>());
        return instance;
    }
};

}
#endif //ACID_SINGLETON_H
