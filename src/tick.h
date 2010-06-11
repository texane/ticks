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


#define TICK_CPU_COUNT 16


/* exported */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int tick_initialize(unsigned int);
void tick_cleanup(void);
void tick_read_counter(tick_counter_t*);
void tick_read_counter2(tick_counter_t*, unsigned int);
void tick_sub_counters
(tick_counter_t*, const tick_counter_t*, const tick_counter_t*);
uint64_t tick_get_value(const tick_counter_t*);
void tick_set_value(tick_counter_t*, uint64_t);
int64_t tick_get_diff(unsigned int, unsigned int);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* ! TICK_H_INCLUDED */
