//
// Created by or.tarazi1 on 4/24/25.
//

#ifndef OS_EX2_MYTHREAD_H
#define OS_EX2_MYTHREAD_H

typedef void (*thread_entry_point)(void);

class MyThread{

    MyThread(int tid, thread_entry_point entry_point);
    int get_tid();
    void set_tid(int tid);

    private:
        thread_entry_point entry_point;
        int tid;
};


#endif //OS_EX2_MYTHREAD_H
