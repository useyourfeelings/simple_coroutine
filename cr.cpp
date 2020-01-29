// xc 20200129 coroutine cr.cpp
#include <iostream>
#include <chrono>
#include <thread>
#include "cr.h"

void func_1_1()
{
    std::cout<<"func_1_1"<<std::endl;
    hub.yield();
    std::cout<<"func_1_1 over"<<std::endl;
}

void func_1()
{
    std::cout<<"func_1"<<std::endl;
    hub.yield();
    
    for(int i = 0; i < 6; ++ i)
    {
        auto result = hub.make(func_1_1);
        if(result == 0)
            std::cout<<"hub.make ok"<<std::endl;
        else
            std::cout<<"hub.make failed"<<std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout<<"func_1 over"<<std::endl;
}

void func_2()
{
    std::cout<<"func_2"<<std::endl;
    hub.yield();
    hub.yield();
    std::cout<<"func_2 over"<<std::endl;
}

void func_3()
{
    std::cout<<"func_3"<<std::endl;
    hub.yield();
    std::cout<<"func_3 over"<<std::endl;
}


int main()
{
    std::cout<<"main"<<std::endl;
    auto result = hub.make(func_1);
    result = hub.make(func_2);
    result = hub.make(func_3);
    
    hub.run();
    
    return 0;
}