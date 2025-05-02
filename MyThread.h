//
// Created by or.tarazi1 on 4/24/25.
//

#ifndef OS_EX2_MYTHREAD_H
#define OS_EX2_MYTHREAD_H
#include "uthreads.h"
#include <setjmp.h>

enum ThreadState {
    READY,
    BLOCKED,
    RUNNING
};

typedef void (*thread_entry_point)(void);

class MyThread{
    public:
        MyThread(int tid, thread_entry_point entry_point);
        int get_tid();
        void set_tid(int tid);
        int get_quantums_left();
        void set_quantums_left(int quantums);
        int get_running_quantums();
        void set_running_quantums(int added_quantums);
        ThreadState get_state();
        void set_state(ThreadState);
        char* stack;
        sigjmp_buf env;
        thread_entry_point get_entry_point();
        ~MyThread();


    private:
        thread_entry_point entry_point;
        int tid;
        ThreadState state;
        int quantums_left;
        int running_quantums;
};


#endif //OS_EX2_MYTHREAD_H
