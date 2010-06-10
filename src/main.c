#define _GNU_SOURCE
#include <sched.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>


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


/* uint64_t vector */

struct uint64_vector
{
  unsigned int dim;
  uint64_t values[1];
};

typedef struct uint64_vector uint64_vector_t;

static uint64_vector_t* uint64_vector_alloc(unsigned int dim)
{
  const size_t size =
    offsetof(uint64_vector_t, values) + dim * sizeof(uint64_t);
  uint64_vector_t* const v = malloc(size);

  if (v == NULL)
    return NULL;

  v->dim = dim;

  return v;
}

static inline void uint64_vector_free(uint64_vector_t* v)
{
  free(v);
}

static inline uint64_t* uint64_vector_at(uint64_vector_t* v, unsigned int i)
{
  return &v->values[i];
}


/* int64 square matrix */

struct int64_matrix
{
  unsigned int dim;
  int64_t values[1];
};

typedef struct int64_matrix int64_matrix_t;

static int64_matrix_t* int64_matrix_alloc(unsigned int dim)
{
  const size_t size =
    offsetof(int64_matrix_t, values) + (dim * dim) * sizeof(int64_t);
  int64_matrix_t* const m = malloc(size);

  if (m == NULL)
    return NULL;

  m->dim = dim;

  return m;
}

static inline void int64_matrix_free(int64_matrix_t* m)
{
  free(m);
}

static inline int64_t* int64_matrix_at
(int64_matrix_t* m, unsigned int i, unsigned int j)
{
  return &m->values[i * m->dim + j];
}


static inline void read_ticks(uint64_union_t* u)
{
  __asm__ __volatile__("rdtsc" : "=a" (u->sub.lo), "=d" (u->sub.hi));
}

/* tick adjustment threads */

struct thread_arg
{
  unsigned int id;
  pthread_t thread;
  pthread_barrier_t* ready_barrier;
  pthread_barrier_t* done_barrier;
  int64_matrix_t* matrix;
  uint64_vector_t* vector;
};

typedef struct thread_arg thread_arg_t;

static void adjust_matrix
(int64_matrix_t* m, uint64_vector_t* v, unsigned int niter)
{
  uint64_t ticks[2];
  unsigned int i;
  unsigned int j;

  for (i = 0; i < m->dim; ++i)
  {
    ticks[0] = *uint64_vector_at(v, i);

    for (j = 0; j < m->dim; ++j)
    {
      int64_t* ptr;

      ticks[1] = *uint64_vector_at(v, j);

      ptr = int64_matrix_at(m, i, j);
      *ptr = ((*ptr) * niter + (ticks[0] - ticks[1])) / (niter + 1);

      ptr = int64_matrix_at(m, j, i);
      *ptr = ((*ptr) * niter + (ticks[1] - ticks[0])) / (niter + 1);
    }
  }
}

static void* thread_entry(void* p)
{
  thread_arg_t* const arg = (thread_arg_t*)p;
  uint64_union_t ticks;
  unsigned int i;

#define TICK_ITER_COUNT 50
  for (i = 0; i < TICK_ITER_COUNT; ++i)
  {
    pthread_barrier_wait(arg->ready_barrier);
    read_ticks(&ticks);
    *uint64_vector_at(arg->vector, arg->id) = ticks.value;
    pthread_barrier_wait(arg->done_barrier);

    if (arg->id == 0)
      adjust_matrix(arg->matrix, arg->vector, i);
  }

  return NULL;
}


/* difference matrix */

static int64_matrix_t* diff_matrix = NULL;


/* get the current cpu id */

static inline unsigned int get_self_id(void)
{
  cpu_set_t cpuset;
  unsigned int i;

  pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  for (i = 0; i < CPU_SETSIZE; ++i)
    if (CPU_ISSET(i, &cpuset))
      return i;

  return 0;
}


/* exported */

int tick_initialize(unsigned int cpu_count)
{
#define TICK_CPU_COUNT 16

  int error = -1;
  unsigned int i ;
  unsigned int j;
  uint64_vector_t* vector = NULL;
  void* res;
  cpu_set_t cpuset;
  pthread_attr_t attr;
  pthread_barrier_t ready_barrier;
  pthread_barrier_t done_barrier;
  thread_arg_t args[TICK_CPU_COUNT];

  cpu_count = TICK_CPU_COUNT;

  pthread_attr_init(&attr);

  diff_matrix = int64_matrix_alloc(cpu_count);
  if (diff_matrix == NULL)
    goto on_error;

  for (i = 0; i < cpu_count; ++i)
  {
    for (j = 0; j < cpu_count; ++j)
    {
      *int64_matrix_at(diff_matrix, i, j) = 1;
      *int64_matrix_at(diff_matrix, j, i) = 1;
    }
  }

  vector = uint64_vector_alloc(cpu_count);
  if (vector == NULL)
    goto on_error;

  pthread_barrier_init(&ready_barrier, NULL, cpu_count);
  pthread_barrier_init(&done_barrier, NULL, cpu_count);

  for (i = 0; i < cpu_count; ++i)
  {
    thread_arg_t* const arg = &args[i];

    arg->id = i;

    arg->ready_barrier = &ready_barrier;
    arg->done_barrier = &done_barrier;
    arg->matrix = diff_matrix;
    arg->vector = vector;

    CPU_ZERO(&cpuset);
    CPU_SET(i, &cpuset);

    if (i != (cpu_count - 1))
    {
      pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
      if (pthread_create(&arg->thread, &attr, thread_entry, arg))
	goto on_error;
    }
    else
    {
      pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
      pthread_yield();
      thread_entry((void*)arg);
    }
  }

  /* dont join the last */
  i = i - 1;

  /* success */
  error = 0;

 on_error:

  pthread_attr_destroy(&attr);

  for (j = 0; j < i; ++j)
    pthread_join(args[j].thread, &res);

  pthread_barrier_destroy(&ready_barrier);
  pthread_barrier_destroy(&done_barrier);

  if (vector != NULL)
    uint64_vector_free(vector);

  if (error && (diff_matrix != NULL))
  {
    int64_matrix_free(diff_matrix);
    diff_matrix = NULL;
  }

  return error;
}


void tick_cleanup(void)
{
  int64_matrix_free(diff_matrix);
  diff_matrix = NULL;
}


void tick_read_counter(tick_counter_t* c)
{
  read_ticks(&c->u);
  c->id = get_self_id();
}


void tick_sub_counters
(tick_counter_t* res, const tick_counter_t* lhs, const tick_counter_t* rhs)
{
  const int64_t diff = *int64_matrix_at(diff_matrix, lhs->id, rhs->id);
  res->u.value = lhs->u.value - rhs->u.value - diff;
  res->id = lhs->id;
}


uint64_t tick_get_value(const tick_counter_t* c)
{
  return c->u.value;
}


/* unit */

#include <stdio.h>


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


int main(int ac, char** av)
{
  tick_counter_t ticks[3];

  tick_initialize(0);

  read_ticks_on(&ticks[0], 1);
  read_ticks_on(&ticks[1], 2);

  tick_sub_counters(&ticks[2], &ticks[1], &ticks[0]);

  printf("%lu(%u) - %lu(%u) = %lu\n",
	 tick_get_value(&ticks[1]), ticks[1].id,
	 tick_get_value(&ticks[0]), ticks[0].id,
	 tick_get_value(&ticks[2]));

  tick_cleanup();

  return 0;
}
