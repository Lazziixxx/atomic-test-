#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include<linux/types.h>
#include<sys/time.h>

__u64 rdtsc()
{
  __u64 tsc;
  asm volatile("mrs %0,cntvct_el0" : "=r"(tsc));
  return tsc;
}

pthread_mutex_t count_lock = PTHREAD_MUTEX_INITIALIZER;

pid_t gettid( void )
{
    return syscall(__NR_gettid);
}

#define TOTAL_COUNT 6000000
int global_count = 0;

void *thread_func(void *arg)
{
  int i;
  int core_num = (int)(long)arg;
  unsigned long begin,end;
  struct timeval tv_begin,tv_end;
  unsigned long timeinterval;
  cpu_set_t set;
  
  CPU_ZERO(&set);
  CPU_SET(core_num, &set);
  
  //线程绑核
  if(sched_setaffinity(gettid(), sizeof(cpu_set_t), &set))
  {
    perror("sched_setaffinity failed.\n");
    return NULL;
  }
  
  begin = rdtsc();
  gettimeofday(&tv_begin,NULL);
  for(i = 0; i < TOTAL_COUNT; i++)
  {
    #if NO_LOCK_COUNTADD
      #define MODULE_NAME "count add without lock"
      global_count++;
    #elif ATOMIC_COUNTADD
      #define MODULE_NAME "count add with atomic"
      __sync_fetch_and_add(&global_count, 1);
    #elif THREAD_LOCK_COUNTADD
      #define MODULE_NAME "count add with pthread lock"
      pthread_mutex_lock(&count_lock);
      global_count++;
      pthread_mutex_unlock(&count_lock);
    #else
    #error "unknown input type"
    #endif
  }
  gettimeofday(&tv_end, NULL);
  end = rdtsc();


  timeinterval = (tv_end.tv_sec - tv_begin.tv_sec)*1000000 + (tv_end.tv_usec - tv_begin.tv_usec);
  fprintf(stderr, "core_num : %d, test module name : %s, cost %llu cpu cycle, cost %llu us.\n", core_num, MODULE_NAME, end-begin,timeinterval);
}

int main()
{
  int i;
  int cpunums;
  pthread_t *thread;
  
  cpunums = (int)sysconf(_SC_NPROCESSORS_ONLN);//获取环境core num
  if( cpunums <= 0 )
  {
    perror("call sysconf failed;\n");
    return -1;
  }
  
  thread = malloc(sizeof(pthread_t) * cpunums);
  if( !thread )
  {
    perror("malloc failed;\n");
    return -1;
  }
  
  printf("Totally starting %d threads for testing;\n", cpunums);
  
  for(i = 0; i < cpunums; i++)
  {
    if(pthread_create(&thread[i], NULL, thread_func, (void *)(long)i))
    {
      perror("pthread create failed;\n");
      return -1;
    }
  }
  
  for( i = 0; i < cpunums; i++)
  {
    pthread_join(thread[i], NULL);
  }
  
  free(thread);
  printf("The Global_count value is : [%d]. Expected value: [%d]. \n", global_count, TOTAL_COUNT*cpunums);
}
