//
//  Random.h
//  TempServer
//
//  Created by 左斯诚 on 2025/7/19.
//

#ifndef Random_h
#define Random_h

namespace Tso {
    class Random{
    public:
        static int RandomInt(const int& min , const int& max);
        static float RandomFloat(const float& min , const float& max);
    };
}


#endif /* Random_h */
