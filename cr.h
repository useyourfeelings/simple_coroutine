// xc 20200129 coroutine cr.h
#include <iostream>
#include <ucontext.h>
#include <list>
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <cassert>

// EMPTY-可创建新协程 NEW-新的协程 RUNNING-运行中 SUSPEND-挂起 DONE-完成
enum CoroutineStatus {EMPTY, NEW, RUNNING, SUSPEND, DONE};

const int STACK_SIZE = 1024 * 32; // 每个协程的栈大小
const int MAX_COROUTINE_COUNT = 5; // 协程总数限制

class Coroutine // 协程类。包含状态、上下文、函数、各种标志位。
{
public:
    int id;
    CoroutineStatus status;
    ucontext_t context;
    char stack[STACK_SIZE]; // 栈空间
    void (*func)(void); // 要执行的函数。为了简便，不处理参数了。
    int yielded; // 区分协程是函数执行完毕，还是切换出来。如果是执行完毕，状态改为EMPTY腾出位置。
    
    Coroutine()
    {
        status = EMPTY; // 默认EMPTY
    }
};

class Hub // hub类。即调度者，每次协程切换都会回到hub的context进行调度。
{
private:
    ucontext_t hub_context; // hub的上下文。每次协程切换都会回这里进行调度。

public:
    int max_size; // 协程总数限制
    int current_index; // 当前位置
    int coroutine_count; // 当前协程数
    
    std::vector<Coroutine> coroutine_vector; // 协程vector。简化为vector，一次创建，不动态申请。

public:
    Hub()
    {
        coroutine_count = 0;
        current_index = -1;
        max_size = MAX_COROUTINE_COUNT;
        coroutine_vector = std::vector<Coroutine>(MAX_COROUTINE_COUNT); // 直接创建所有。不折腾动态申请内存。
    }
    
    int print_status()
    {
        std::cout<<"--    coroutine_count = "<<this->coroutine_count<<"/"<<this->max_size<<std::endl;
        std::cout<<"--    current coroutine id = "<<this->current_index<<std::endl;
    }
    
    int make(void (*func)(void)) // 创建协程
    {
        if(coroutine_count >= max_size) // 超出限定数量
            return 1;
        
        // 找一个EMPTY的位置
        int pos = -1;
        auto coroutine_vector_size = coroutine_vector.size();
        for(int i = current_index + 1; i < coroutine_vector_size; ++ i) // 往后找
        {
            if(EMPTY == coroutine_vector[i].status)
            {
                pos = i;
                break;
            }
        }
        
        if(pos == -1) // 没找到。再从头找。
        {
            for(int i = 0; i < current_index; ++ i)
            {
                if(EMPTY == coroutine_vector[i].status)
                {
                    pos = i;
                    break;
                }
            }
        }
        
        assert(pos != -1);
        
        coroutine_vector[pos].id = pos;
        coroutine_vector[pos].status = NEW; // 标志为新协程
        coroutine_vector[pos].func = func;
        
        ++ coroutine_count;
        
        return 0;
    }

    int schedule() // 调度
    {
        // 选出要执行的协程。NEW或SUSPEND状态。
        int pos = -1;
        auto coroutine_vector_size = coroutine_vector.size();
        for(int i = current_index + 1; i < coroutine_vector_size; ++ i) // 往后找
        {
            if(NEW == coroutine_vector[i].status || SUSPEND == coroutine_vector[i].status)
            {
                pos = i;
                break;
            }
        }
        
        if(pos == -1) // 从头找
        {
            for(int i = 0; i < current_index; ++ i)
            {
                if(NEW == coroutine_vector[i].status || SUSPEND == coroutine_vector[i].status)
                {
                    pos = i;
                    break;
                }
            }
        }
        
        if(pos == -1) // 没找到可执行的
        {
            if(coroutine_count == 1 && coroutine_vector[current_index].yielded == 0)
            {
                return 1; // 如果只存在一个协程，那么就是当前的协程，并且yielded == 0，代表函数正常结束，那么代表所有协程都执行完毕。
            }
            
            std::cout<<"no runnable coroutine"<<std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return 0;
        }
        
        std::cout<<"schedule pick "<<pos<<std::endl; // 找到了pos这个位置
        
        // 更新状态
        if(current_index != -1) // 初始状态的话，不用更新。
        {
            if(coroutine_vector[current_index].yielded == 0)
            {
                // 如果是函数自己执行完毕，设为EMPTY，减计数。即销毁。
                coroutine_vector[current_index].status = EMPTY;
                -- coroutine_count;
            }
            else // 是切换出来的，设为挂起。
                coroutine_vector[current_index].status = SUSPEND;
        }
        
        current_index = pos; // 当前协程id设为pos
        
        print_status();
        
        auto coroutine = &coroutine_vector[pos];
        
        // 切换
        if(coroutine->status == NEW) // 新的协程。需要配置一下。
        {
            getcontext(&(coroutine->context)); // 获取当前contenxt
            
            // 配置
            std::fill_n(coroutine->stack, STACK_SIZE, 0); // 清理stack
            coroutine->context.uc_stack.ss_sp = coroutine->stack;
            coroutine->context.uc_stack.ss_size = STACK_SIZE;
            coroutine->context.uc_stack.ss_flags = 0;
            coroutine->context.uc_link = &hub_context; // 此协程执行完跳回hub_context
            
            makecontext(&(coroutine->context), coroutine->func, 0); // 设置要执行的函数
            
        }
        
        coroutine->status = RUNNING;
        coroutine->yielded = 0;
        
        // 切换。当前上下文存hub_context
        swapcontext(&hub_context, &(coroutine->context));
        
        // 因为uc_link设置为了&hub_context，所以下次切换回hub时会走到这里。
        // 这样就实现了每次协程切换都会回到hub_context进行下一次调度。
        
        return 0;
    }
    
    int yield() // 切换出去，交出控制权。
    {
        // 切换回hub
        coroutine_vector[current_index].yielded = 1;
        swapcontext(&(coroutine_vector[current_index].context), &hub_context);
        
        // 在用户端的函数内调用yield()
        // 当前上下文存到对应的协程内。下次切换回这个函数时，会从这里继续执行。
    }
    
    int run() // 启动
    {
        std::cout<<"run"<<std::endl;
        print_status();
        for(;;) // 不停地调度，直到所有协程都执行完毕。
        {
            std::cout<<"schedule ..."<<std::endl;
            auto result = schedule();
            
            if(result == 1)
            {
                std::cout<<"all over"<<std::endl;
                break;
            }
        }
    }
};

Hub hub;


