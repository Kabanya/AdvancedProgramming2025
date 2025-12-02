#pragma once

#define USE_BEHAVIOUR_TREE 1  // else FINITE STATE MACHINE

// choose one options of the following three for world update
// if everywhere 0 => base single thread no mutex world update
#define USE_THREADS     1     // FOR_WORLD_UPDATE
#define USE_THREAD_POOL 0     // FOR_WORLD_UPDATE
#define USE_MUTEX       0     // FOR_WORLD_UPDATE
