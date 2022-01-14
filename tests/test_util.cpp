//
// Created by zavier on 2021/11/21.
//
#include "acid/acid.h"
#include <vector>
#include <iostream>
void test( int n){
    //constexpr int c=0;
    ACID_ASSERT2(0,"Not zero");
    //ACID_STATIC_ASSERT(c);
    //acid::Assert(0,"Void test(int)");
}
int main(){
    std::vector<std::string> vec{"aa","bb"};

    std::cout<<vec.size();
}