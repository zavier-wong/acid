//
// Created by zavier on 2021/12/5.
//

#ifndef ACID_NONCOPYABLE_H
#define ACID_NONCOPYABLE_H

class Noncopyable{
public:
    Noncopyable() = default;
    ~Noncopyable() = default;
    Noncopyable(const Noncopyable& noncopyable) = delete;
    Noncopyable& operator=(const Noncopyable& noncopyable) = delete;
};
#endif //ACID_NONCOPYABLE_H
