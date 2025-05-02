#include "uthreads.h"
#include <queue>
#include "MyThread.h"
#include <map>
#include <iostream>

#include <sys/time.h>
#include <csignal>


#define MAIN_THREAD_TID 0
#define JB_SP 6
#define JB_PC 7
typedef unsigned long address_t;


// state machine:
std::queue <MyThread*> ready_queue;
std::map <int, MyThread*> threads;
MyThread* running = nullptr;
int total_quantums;
int quantum_length_usecs = 0;

struct sigaction sa = {0};
struct itimerval timer;



sigset_t set;

void block_signals() {
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &set, nullptr);
}

void unblock_signals() {
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
}




/**
 * checks if a thread with given id exists in the system
 * @param tid to check if exists in the system
 * @return 0 if exists, -1 otherwise
 */
int check_tid_exists(int tid){
    if (threads.find(tid) == threads.end()){
//        std::cerr << "thread library error: thead not found\n";
        return -1;
    }
    return 0;
}
void adjust_ready_queue(int tid) {
    std::queue<MyThread*> new_ready_queue;

    while (!ready_queue.empty()) {
        MyThread* t = ready_queue.front();
        ready_queue.pop();

        if (t->get_tid() != tid) {
            new_ready_queue.push(t);
        }
        // else: skip it (this removes the thread with tid)
    }

    ready_queue = std::move(new_ready_queue);
}

int reset_timer(int usec_interval) {
    itimerval timer;
    timer.it_value.tv_sec = usec_interval / 1000000;
    timer.it_value.tv_usec = usec_interval % 1000000;

    // For repeating timer: interval time = initial time
    timer.it_interval = timer.it_value;

    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) == -1) {
        return -1;
    }
    return 0;
}
/**
 * switch threads only inserts a thread from ready to running
 * @param tid
 */
void unload_running_thread() {
    block_signals();

    total_quantums += 1;

    if (!ready_queue.empty()) {
        MyThread* thread = ready_queue.front();
        ready_queue.pop();
        thread->set_state(RUNNING);
        running = thread;
        reset_timer(quantum_length_usecs);
        thread->set_running_quantums(1);
        unblock_signals();
        siglongjmp(running->env, 1);  // always jump
    } else {
        running->set_running_quantums(1);
        unblock_signals();
    }
}

/**
 * @brief Saves the current thread state, and jumps to the other thread.
 */
void yield(bool send_to_queue) {
    block_signals();

    if (send_to_queue) {
        ready_queue.push(running);
    }

    if (sigsetjmp(running->env, 1) == 0) {
        unblock_signals();  // before switching away
        unload_running_thread();
    }

    unblock_signals();  // returning via siglongjmp
}


/** A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
            : "=g" (ret)
            : "0" (addr));
    return ret;
}


void timer_handler(int sig) {
    //sleep mechanism. lowers down quantums left and resumes if reaches 0.
    for(int i = 1; i < MAX_THREAD_NUM; i++){
        if (check_tid_exists(i)==0) {
            if (threads[i]->get_quantums_left() > 0) {
                threads[i]->set_quantums_left(threads[i]->get_quantums_left() - 1);
            }
            if (threads[i]->get_quantums_left() == 0) {
                threads[i]->set_quantums_left(-1);
                uthread_resume(i);
            }
        }
    }
    yield(true);
}

void setup_thread(MyThread* thread)
{
    // initializes env[tid] to use the right stack, and to run from the function 'entry_point', when we'll use
    // siglongjmp to jump into the thread.
    address_t sp = (address_t) thread->stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) thread->get_entry_point();
    sigsetjmp(thread->env, 1);
    (thread->env->__jmpbuf)[JB_SP] = translate_address(sp);
    (thread->env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&thread->env->__saved_mask);
}

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){
    // set the quantum length at the beginning
    quantum_length_usecs = quantum_usecs;
    total_quantums = 0;
    if (uthread_spawn(nullptr)==-1){
        return -1;
    }
    unload_running_thread();

    sa.sa_handler = &timer_handler;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
        return -1;
    }
    // reset the time for the first tume:
    if (reset_timer(quantum_length_usecs) == -1){
        return -1;
    };
    return 0;
}



/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point){

    if (threads.size() >= MAX_THREAD_NUM){
        return -1; // FAIL: cannot add more than maximum number of threads.
    }
    int tid = 200;
    for (int i=0 ; i <= MAX_THREAD_NUM ; i++) {
        if (threads.find(i) == threads.end()) { // if not found, i is free.
            tid = i;
            break;
        }
    }
    try {
        MyThread* new_thread = new MyThread(tid, entry_point);
        if (tid != MAIN_THREAD_TID && entry_point == nullptr){
            return -1;
        }
        threads[tid] = new_thread;
        if(tid != MAIN_THREAD_TID) {
            ready_queue.push(new_thread);
        }
        else{
            running = new_thread;
        }
        setup_thread(new_thread);
    }
    catch (const std::bad_alloc& e) {
        std::cerr << "system error: memory allocation failed\n";
        exit(1);
    }
    return tid;
}


/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
    block_signals();

    if (check_tid_exists(tid)==-1){
        unblock_signals();
        return -1;
    }

    if (tid == 0) {
        for (int i = MAX_THREAD_NUM; i >= 0; i--){
            if (threads.find(i) != threads.end()){
//                delete[] threads[i]->stack;
                delete threads[i];
                threads.erase(i);
            }
        }
        unblock_signals();
        exit(0);
    }

    switch(threads[tid]->get_state()){
        case RUNNING:
            unblock_signals();  // must unblock before jumping
            yield(false);       // we delete after switching
            break;

        case READY:
            adjust_ready_queue(tid);
            break;

        case BLOCKED:
            // nothing to do with queues
            break;
    }

//    delete[] threads[tid]->stack;
    delete threads[tid];
    threads.erase(tid);

    unblock_signals();
    return 0;
}





/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){
    block_signals();

    if (check_tid_exists(tid)==-1){
        unblock_signals();
        return -1;
    }

    if (tid == MAIN_THREAD_TID){
        std::cerr << "thread library error: main thread cannot be blocked\n";
        unblock_signals();
        return -1;
    }

    if (running->get_tid() == tid) {
        running->set_state(BLOCKED);
        unblock_signals();  // unblock before switching
        yield(false);
    } else if (threads[tid]->get_state() == READY) {
        threads[tid]->set_state(BLOCKED);
        adjust_ready_queue(tid);
        unblock_signals();
    } else {
        unblock_signals();
    }

    return 0;
}



/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){
    block_signals();

    if (check_tid_exists(tid)==-1){
        unblock_signals();
        return -1;
    }

    if (threads[tid]->get_state() == BLOCKED){
        threads[tid]->set_state(READY);
        ready_queue.push(threads[tid]);
    }

    unblock_signals();
    return 0;
}



/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums){
    if (running->get_tid() == MAIN_THREAD_TID) {
        return -1;
    }

    block_signals();
    int tid = running->get_tid();
    int success = uthread_block(tid);
    if (success == 0){
        threads[tid]->set_quantums_left(num_quantums);
        unblock_signals();
        return 0;
    } else {
        unblock_signals();
        return -1;
    }
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid(){
    return running->get_tid();
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums(){
    return total_quantums;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){
    if (check_tid_exists(tid)==-1) {
        return -1;
    }
    return threads[tid]->get_running_quantums();
}