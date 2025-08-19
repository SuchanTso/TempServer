//
//  Random.cpp
//  TempServer
//
//  Created by 左斯诚 on 2025/7/19.
//
#include "Spch.h"
#include "Random.h"

namespace Tso {
int Random::RandomInt(const int& min , const int& max){
    srand(time(NULL));  // 初始化随机数生成器
    int randomInt = rand() % (max - min) + min;  // 生成 0 到 99 之间的随机数
    return randomInt;
}

}
