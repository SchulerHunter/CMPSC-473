#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

extern void init_scheduler(int);
extern int schedule_me(float, int, int, int); // returns current global time
extern int P(float, int, int); // returns current global time
extern int V(float, int, int); // returns current global time

typedef struct _int_link_list
{
	int id;
	struct _int_link_list* next;
}_tid_list_t;

FILE *fd;

struct _thread_info
{
	int id;
	float arrival_time;
	int initial_burst;
	int priority;
	char semops[1024];
	int semid[1024];
	int burst[1024];
};
typedef struct _thread_info _thread_info_t;

double _global_time;
float _last_event_time;
float last_v_time;
float last_p_time;
int last_v_id;

pthread_mutex_t _time_lock;
pthread_mutex_t _last_event_lock;
pthread_mutex_t force_v_schedule;
sem_t notify_sem_call[100];

void set_last_event(float last_event)
{
	pthread_mutex_lock(&_last_event_lock);
	if (last_event > _last_event_time)
		_last_event_time = last_event;
	pthread_mutex_unlock(&_last_event_lock);
}

float get_global_time()
{
	return _global_time;
} 
void advance_global_time(float next_arrival)
{
	pthread_mutex_lock(&_time_lock);
	float next_ms = floor(_global_time + 1.0);
	if ((next_arrival < next_ms) && (next_arrival > 0))
		_global_time = next_arrival;
	else
		_global_time = next_ms;
  //printf("Advancing time to %f\n", _global_time);
	pthread_mutex_unlock(&_time_lock);
}

float read_next_arrival(_thread_info_t* ti)
{
  char line[1024];
  if(!fgets(line, 1024, fd)) {
    return -1.0;
  }

  char* token;
  char* rest=line;

  if(token=strtok_r(rest, "\t", &rest)) {
    ti->arrival_time = atof(token);
  } else {
    return -1.0;
  }

  if(token=strtok_r(rest, "\t", &rest)) {
    ti->id = atoi(token);
  } else {
    return -1.0;
  }

  if(token=strtok_r(rest, "\t", &rest)) {
    ti->priority = atoi(token);
  } else {
    return -1.0;
  }

  if(token=strtok_r(rest, "\t", &rest)) {
    ti->initial_burst = atoi(token);
  } else {
    return -1.0;
  }

  int iter_op=0;
  while(token=strtok_r(rest, "\t", &rest)) {
    // Carefully parse, P<semaphore-id> or V<semaphore-id>
    ti->semops[iter_op] = token[0];
    char sem_id_string[5];
    // Skip First character (P/V)
    strcpy(sem_id_string, (token+1));
    ti->semid[iter_op] = atoi(sem_id_string);
    token = strtok_r(rest, "\t", &rest);
    ti->burst[iter_op] = atoi(token);
    iter_op++;
  }
  // Padding
  ti->semops[iter_op] = '\0';
  ti->semid[iter_op] = -1;
  ti->burst[iter_op] = 0;

	return ti->arrival_time;
}

int open_file(char *filename)
{
	char mode = 'r';
	fd = fopen(filename, &mode);
	if (fd == NULL)
	{
		printf("Invalid input file specified: %s\n", filename);
		return -1;
	}
	else
	{
		return 0;
	}
}

void close_file()
{
	fclose(fd);
}

void do_work(_thread_info_t* myinfo, int burst_length, bool nudge, bool on_p_call, bool on_v_call)
{
  if(burst_length == 0)
    return;
  float time_remaining = burst_length * 1.0;
  float scheduled_time;
  float nudge_delay = 0.0;

  if(nudge) {
    nudge_delay = 0.2;
  }
  
  if(on_v_call) {
    while(last_v_time > 0) {
        sched_yield();
    }
    pthread_mutex_unlock(&force_v_schedule);
  } else {
    // Make sure V goes through first if active
    pthread_mutex_lock(&force_v_schedule);
    pthread_mutex_unlock(&force_v_schedule);
  }

  if(!on_p_call) { 
    // If V has been called in last cycle and I am not
    // V, I allow others to go
    while(last_v_id != myinfo->id && last_v_id != 0) {
      sched_yield();
    }
  }
  last_v_id = 0;

  do
  {
    if(on_p_call) {
      last_p_time = 0;
    }
    scheduled_time = schedule_me(get_global_time() + nudge_delay, myinfo->id, time_remaining, myinfo->priority);
    // Makes sure P wakes up
    while(last_p_time > 0) {
      sched_yield();
    }
    // reset nudge
    nudge_delay = 0.0;
		set_last_event(scheduled_time);
		printf("%3.0f - %3.0f: T%d\n", scheduled_time, scheduled_time + 1.0, myinfo->id);
		while(get_global_time() < scheduled_time + 1.0)
		{
			sched_yield();
		}
    // Decrement Time
		time_remaining -= 1.0;
  } while(time_remaining > 0);
  // We skip informing the last time-step to do a synch op (if needed at the end)
}

void *worker_thread(void *arg)
{
	_thread_info_t *myinfo = (_thread_info_t *)arg;
	float time_remaining = myinfo->initial_burst * 1.0;
	float scheduled_time;

	set_last_event(myinfo->arrival_time);
  float sched_time = myinfo->arrival_time;
  //do_work(myinfo, myinfo->initial_burst, false, false, false);
  do
  {
    scheduled_time = schedule_me(sched_time, myinfo->id, time_remaining, myinfo->priority);
    set_last_event(scheduled_time);
		printf("%3.0f - %3.0f: T%d\n", scheduled_time, scheduled_time + 1.0, myinfo->id);
		while(get_global_time() < scheduled_time + 1.0)
		{
			sched_yield();
		}
    // Decrement Time
		time_remaining -= 1.0;
    sched_time = get_global_time();
  } while(time_remaining > 0);

	for(int i=0; i < strlen(myinfo->semops); i++)
	{
		if (myinfo->semops[i] == 'P') {
      float call_time = get_global_time();
      sem_post(&notify_sem_call[myinfo->semid[i]]);
      printf("T%d: P on %d\n", myinfo->id, myinfo->semid[i]);
			float current_time = P(call_time, myinfo->id, myinfo->semid[i]);
      sem_wait(&notify_sem_call[myinfo->semid[i]]);
      // Reset last v time to say I have woken up
      last_v_time = 0;
      last_p_time = get_global_time();

      bool nudge = false;
      if(call_time != current_time) {
        // A signal woke me up, so, I have to nudge my start time
        nudge = true;
      } else {
        // Don't need to nudge as my P immediately completed
      }
      do_work(myinfo, myinfo->burst[i], nudge, true, false);
    }
		else if(myinfo->semops[i]=='V') {
      pthread_mutex_lock(&force_v_schedule);
      int sem_val;
      sem_getvalue(&notify_sem_call[myinfo->semid[i]], &sem_val);
      if(sem_val > 0) {
        last_v_time = get_global_time();
        last_v_id = myinfo->id;
      } else {
        last_v_time = 0;
      }
      printf("T%d: V on %d\n", myinfo->id, myinfo->semid[i]);
			float current_time = V(get_global_time(), myinfo->id, myinfo->semid[i]);
      do_work(myinfo, myinfo->burst[i], false, false, true);
    }
	}
  // Inform the that this thread has no more work to do
  schedule_me(get_global_time(), myinfo->id, 0, myinfo->priority);

	free(myinfo);
	pthread_exit(NULL);
}

int _pre_init(int sched_type)
{
	pthread_mutex_init(&_time_lock, NULL);
	pthread_mutex_init(&_last_event_lock, NULL);
	pthread_mutex_init(&force_v_schedule, NULL);
	init_scheduler(sched_type);

	_global_time = 0.0;
	_last_event_time = -1.0;
  last_v_time = 0.0;
  last_v_id=0;
  last_p_time = 0.0;

  for(int i=0;i<100;i++)
    sem_init(&notify_sem_call[i], 0, 0);
}

int main(int argc, char *argv[])
{	
	int inactivity_timeout = 50;
	int i;
	if (argc < 3)
	{
		printf ("Not enough parameters specified.  Usage: a.out <scheduler_type> <input_file>\n");
		printf ("  Scheduler type: 0 - First Come, First Served (Non-preemptive)\n");
		printf ("  Scheduler type: 1 - Shortest Remaining Time First (Preemptive)\n");
		printf ("  Scheduler type: 2 - Priority-based Scheduler (Preemptive)\n");
		printf ("  Scheduler type: 3 - Multi-Level Feedback Queue w/ Aging (Preemptive)\n");
		return -1;
	}

	if (open_file(argv[2]) < 0)
		return -1;

	_pre_init(atoi(argv[1]));

	int this_thread_id = 0;

	_thread_info_t *ti;

	_tid_list_t *tids, *head_tids;
	float next_arrival_time;
	pthread_t pt;

	ti = malloc(sizeof(_thread_info_t));
	tids = malloc(sizeof(_tid_list_t));
	head_tids = tids;
	next_arrival_time = read_next_arrival(ti);
	tids->id = ti->id;
	tids->next = NULL;
	if (next_arrival_time < 0)
		return -1;

	pthread_create(&pt, NULL, worker_thread, ti);

  // Twiddle thumb till ti has arrived
	while (_last_event_time != ti->arrival_time)
		sched_yield();

  int loop_counter = 0;
  while ((_last_event_time < get_global_time()) && (loop_counter < 100000))
  {
    loop_counter++;
    sched_yield();
  }
	
  ti = malloc(sizeof(_thread_info_t));
	tids->next = malloc(sizeof(_tid_list_t));
	tids = tids->next;

	next_arrival_time = read_next_arrival(ti);
	tids->id = ti->id;
	tids->next = NULL;
	while ((get_global_time() - _last_event_time) < inactivity_timeout)
	{
		advance_global_time(next_arrival_time);		// Advance timer to next whole unit, or event
		if (get_global_time() == next_arrival_time)
		{
			pthread_create(&pt, NULL, worker_thread, ti);

      // Has ti arrived yet?
			while (_last_event_time < ti->arrival_time){
				sched_yield();
			}
			ti = malloc(sizeof(_thread_info_t));
			tids->next = malloc(sizeof(_tid_list_t));
			next_arrival_time = read_next_arrival(ti);
			if (next_arrival_time < 0)
			{
				free(ti);
				free(tids->next);
				tids->next = NULL;
			}
			else
			{
				tids = tids->next;
				tids->id = ti->id;
				tids->next = NULL;
			}
		}
		else
		{
			int loop_counter = 0;
			while ((_last_event_time < get_global_time()) && (loop_counter < 100000))
			{
				loop_counter++;
				sched_yield();
			}
		}
	}

	close_file();
	return 0;
}
