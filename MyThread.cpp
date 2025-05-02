//
// Created by or.tarazi1 on 4/24/25.
//

#include <cstdlib>
#include <iostream>
#include "MyThread.h"

MyThread::MyThread(int tid, thread_entry_point entry_point){
    this->tid = tid;
    this->entry_point = entry_point;
    this->state = READY;
    this->quantums_left = 0;
    if(tid != 0) {
        try {
            this->stack = new char[STACK_SIZE];
        } catch (const std::bad_alloc &e) {
            std::cerr << "system error: memory allocation failed\n";
            exit(1);
        }
    }
}

int MyThread::get_tid() {
    return this->tid;
}

void MyThread::set_tid(int new_tid) {
    this->tid = new_tid;
}

ThreadState MyThread::get_state() {
    return this->state;
}

void MyThread::set_state(ThreadState new_state) {
    this->state = new_state;
}

void MyThread::set_quantums_left(int quantums) {
    this->quantums_left = quantums;
}

int MyThread::get_quantums_left() {
    return this->quantums_left;
}

void MyThread::set_running_quantums(int added_quantums) {
    this->running_quantums += added_quantums;

}

int MyThread::get_running_quantums() {
    return this->running_quantums;
}

thread_entry_point MyThread::get_entry_point() {
    return this->entry_point;
}

MyThread::~MyThread() {
    if (stack != nullptr && tid != 0) {
        delete[] stack;
        stack = nullptr;
    }
}




