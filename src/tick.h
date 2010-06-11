#ifndef TICK_H_INCLUDED
# define TICK_H_INCLUDED



#include <stdint.h>


/* uint64_t union */

union uint64_union
{
  uint64_t value;
  struct
  {
    uint32_t lo;
    uint32_t hi;
  } sub;
};

typedef union uint64_union uint64_union_t;


/* tick counter */

struct tick_counter
{
  uint64_union_t u;
  unsigned int id;
};

typedef struct tick_counter tick_counter_t;


#define TICK_CPU_COUNT 2


/* exported */

int tick_initialize(unsigned int);
void tick_cleanup(void);
void tick_read_counter(tick_counter_t*);
void tick_sub_counters
(tick_counter_t*, const tick_counter_t*, const tick_counter_t*);
uint64_t tick_get_value(const tick_counter_t*);
void tick_set_value(tick_counter_t*, uint64_t);
int64_t tick_get_diff(unsigned int, unsigned int);



#endif /* ! TICK_H_INCLUDED */
