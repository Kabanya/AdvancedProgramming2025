#pragma once

#define USE_BEHAVIOUR_TREE 1  // else FINITE STATE MACHINE

// choose one options of the following for world update
// if everywhere 0 => base single thread world update
#define USE_MUTEX       0    // FOR_WORLD_UPDATE
#define USE_THREADS     0     // FOR_WORLD_UPDATE
#define USE_THREAD_POOL 1     // FOR_WORLD_UPDATE
#define USE_SPINLOCK    0     // FOR_WORLD_UPDATE

//not best place for this macros
#define OPTICK_SCOPED_MUTEX_LOCK(mutex_var) \
    std::unique_lock<std::mutex> lock(mutex_var, std::defer_lock); \
    { \
        OPTICK_EVENT("MutexWait", Optick::Category::Wait); \
        lock.lock(); \
    } \
    OPTICK_EVENT()
