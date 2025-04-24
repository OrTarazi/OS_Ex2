//
// Created by or.tarazi1 on 4/24/25.
//

#include "MyThread.h"

MyThread::MyThread(int tid, thread_entry_point entry_point){
    this->tid = tid;
    this->entry_point = entry_point;
}

int MyThread::get_tid() {
    return this->tid;
}

void MyThread::set_tid(int new_tid) {
    this->tid = tid;
}

