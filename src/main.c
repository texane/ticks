#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "tick.h"


static void read_ticks_on(tick_counter_t* ticks, unsigned int id)
{
  pthread_attr_t attr;
  cpu_set_t cpuset;

  pthread_attr_init(&attr);
  CPU_ZERO(&cpuset);
  CPU_SET(id, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  pthread_yield();
  pthread_attr_destroy(&attr);

  tick_read_counter(ticks);
}


static inline void fubar(void)
{
  usleep(1000);
}


int main(int ac, char** av)
{
  tick_counter_t ticks[4];
  uint64_t sum = 0;
  unsigned int j;

  tick_initialize(0);

  srand(getpid() * time(NULL));

#define ITER_COUNT 100
  for (j = 0; j < ITER_COUNT; ++j)
  {
    const unsigned int id = rand() % TICK_CPU_COUNT;
    read_ticks_on(&ticks[0], id);
    fubar();
    read_ticks_on(&ticks[1], id);
    tick_sub_counters(&ticks[2], &ticks[1], &ticks[0]);
    sum += tick_get_value(&ticks[2]);
  }

  tick_set_value(&ticks[3], sum / ITER_COUNT);

  for (j = 0; j < 1000; ++j)
  {
    unsigned int ids[2];

    ids[0] = rand() % TICK_CPU_COUNT;
    ids[1] = ids[0];
    while (ids[0] == ids[1])
      ids[1] = rand() % TICK_CPU_COUNT;

    read_ticks_on(&ticks[0], ids[0]);
    fubar();
    read_ticks_on(&ticks[1], ids[1]);
    tick_sub_counters(&ticks[2], &ticks[1], &ticks[0]);

#ifndef PRId64
# define PRId64 "ld"
#endif

#ifndef PRIu64
# define PRIu64 "lu"
#endif
    printf("[%u:%u] (%" PRId64 " %" PRIu64 " - %" PRIu64\
	   " = %" PRIu64 " (%" PRIu64 ")\n",
	   ids[0], ids[1],
	   tick_get_diff(ids[0], ids[1]),
	   tick_get_value(&ticks[1]),
	   tick_get_value(&ticks[0]),
	   tick_get_value(&ticks[2]),
	   tick_get_value(&ticks[3]));
  }

  tick_cleanup();

  return 0;
}
