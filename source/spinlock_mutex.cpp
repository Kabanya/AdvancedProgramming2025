// ============================================================================
// This code is in the Public Domain - I claim no copyrights over it.
// Use at your own leisure and risk.
// ============================================================================

#include "spinlock_mutex.h"

#include <cassert>
#include <cstdio>

// ========================================================
// Test code - static local variables for testing only
// ========================================================

static TLS_VARIABLE_INIT(bool, test_spinlock_locked, false);
static spinlock_mutex test_spinlock;

void thread_func(std::size_t thread_idx, std::size_t * count)
{
    recursive_scoped_lock<spinlock_mutex> lock{
        test_spinlock,
        test_spinlock_locked,
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
//             test_spinlock,
//             test_spinlock_locked,
//             spinlock_idle_opts::noop<>{}
//         };
//         std::printf("First lock from main...\n");

//         recursive_scoped_lock<spinlock_mutex> lock2{
//             test_spinlock,
//             test_spinlock_locked,
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
