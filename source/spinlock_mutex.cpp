// ============================================================================
// This code is in the Public Domain - I claim no copyrights over it.
// Use at your own leisure and risk.
//
// Compiled and tested with:
//  c++ -std=c++11 -fno-exceptions -Wall -Wextra -pedantic -O3 spinlock_mutex.cpp
// ============================================================================

#include <cassert>
#include <cstdio>

#include <atomic>
#include <thread>

// ========================================================

template<typename MutexType, typename ThreadLockedFlagType = bool>
class recursive_scoped_lock final
{
public:

    template<typename WhileIdleFunc = void()>
    recursive_scoped_lock(MutexType & mtx, ThreadLockedFlagType & locked_flag, WhileIdleFunc idle_work = []{})
        : m_mutex{ mtx }
        , m_locked_flag{ locked_flag }
        , m_lock_acquired{ false }
    {
        if (!m_locked_flag)
        {
            m_mutex.lock(idle_work);
            m_lock_acquired = true;
            m_locked_flag   = true;
        }
    }

    ~recursive_scoped_lock()
    {
        if (m_lock_acquired)
        {
            m_mutex.unlock();
            m_locked_flag = false;
        }
    }

    recursive_scoped_lock(const recursive_scoped_lock &) = delete;
    recursive_scoped_lock & operator = (const recursive_scoped_lock &) = delete;

private:

    MutexType & m_mutex;                  // Mutex that is conditionally locked for this thread only if not being recursively locked.
    ThreadLockedFlagType & m_locked_flag; // Thread-local flag set when the running thread acquires the lock.
    bool m_lock_acquired;                 // Set if we acquired the lock in the constructor and must unlock in the destructor.
};

// ========================================================

class spinlock_mutex final
{
private:

    std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:

    template<typename WhileIdleFunc = void()>
    void lock(WhileIdleFunc idle_work = []{})
    {
        while (flag.test_and_set(std::memory_order_acquire))
        {
            idle_work();
        }
    }

    void unlock()
    {
        flag.clear(std::memory_order_release);
    }

    spinlock_mutex() = default;
    spinlock_mutex(const spinlock_mutex &) = delete;
    spinlock_mutex & operator = (const spinlock_mutex &) = delete;
};

// ========================================================

namespace spinlock_idle_opts
{

struct yield_thread
{
    void operator()() const
    {
        std::this_thread::yield();
    }
};

template<unsigned Count = 1>
struct noop
{
    void operator()() const
    {
        for (unsigned i = 0; i < Count; ++i)
        {
            asm volatile ("nop");
        }
    }
};

template<unsigned Initial = 8, unsigned Scale = 4>
struct spinner
{
    volatile unsigned m_stopper = Initial;
    void operator()()
    {
        for (volatile unsigned count = 0; count < m_stopper; ++count) { }
        m_stopper *= Scale;
    }
};

} // namespace spinlock_idle_opts {}

// ========================================================

// Simple workaround since Apple's Clang still doesn't support C++11 thread_local.
// __thread is equivalent but works for plain-old data types only!
#if defined(__APPLE__) && (defined(__GNUC__) || defined(__clang__))
    #define TLS_VARIABLE(Type, Name)                   __thread Type Name
    #define TLS_VARIABLE_INIT(Type, Name, Initializer) __thread Type Name = Initializer
#else // !__APPLE__
    #define TLS_VARIABLE(Type, Name)                   thread_local Type Name
    #define TLS_VARIABLE_INIT(Type, Name, Initializer) thread_local Type Name = Initializer
#endif // TLS model

// ========================================================

static TLS_VARIABLE_INIT(bool, spinlock_mtx_locked, false);
static spinlock_mutex spinlock_mtx;

void thread_func(std::size_t thread_idx, std::size_t * count)
{
    recursive_scoped_lock<spinlock_mutex> lock{
        spinlock_mtx,
        spinlock_mtx_locked,
        spinlock_idle_opts::noop<10>{}
    };

    // Broken down into separate calls so we can spot any thread inconsistencies.
    std::printf("[%zu]: ", thread_idx);
    std::printf("Hello world - from a worker thread");
    std::printf("\n");

    (*count)++;
}

// int main()
// {
//     // Try locking multiple times from the same thread:
//     {
//         recursive_scoped_lock<spinlock_mutex> lock1{
//             spinlock_mtx,
//             spinlock_mtx_locked,
//             spinlock_idle_opts::noop<>{}
//         };
//         std::printf("First lock from main...\n");

//         recursive_scoped_lock<spinlock_mutex> lock2{
//             spinlock_mtx,
//             spinlock_mtx_locked,
//             spinlock_idle_opts::noop<>{}
//         };
//         std::printf("Second lock from main...\n");
//     }

//     // Spawn a bunch of threads:
//     {
//         std::size_t count = 0;
//         std::vector<std::thread> threads;

//         for (std::size_t i = 0; i < 10; ++i)
//         {
//             threads.emplace_back(thread_func, i, &count);
//         }

//         for (auto & t : threads)
//         {
//             t.join();
//         }

//         std::printf("Final count is: %zu\n", count);
//         assert(count == threads.size());
//     }
// }
