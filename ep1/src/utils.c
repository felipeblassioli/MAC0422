#include <sys/time.h>

double time_diff(struct timeval x, struct timeval y){
	double x_us, y_us, diff;

	x_us = (double)x.tv_sec*1000000 + (double)x.tv_usec;
	y_us = (double)y.tv_sec*1000000 + (double)y.tv_usec;
	diff = (double)y_us - (double)x_us;

	return diff;
}

#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

void sleep_ms(int milliseconds) // cross-platform sleep function
{
	//http://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds
#ifdef WIN32
	Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
#else
	usleep(milliseconds * 1000);
#endif
}

extern struct timeval g_start;

double current_time(){
	double ellapsed_time;
	struct timeval now;
	gettimeofday(&now, NULL);
	ellapsed_time = time_diff(g_start, now);

	return ellapsed_time;
}

double current_time_s(){
	double ellapsed_time;
	struct timeval now;
	gettimeofday(&now, NULL);
	ellapsed_time = time_diff(g_start, now);

	return ellapsed_time/1000000;
}

typedef struct {
	int id;
	void (*thread_func)(void *);
	void *args;
} ThreadArgs;


#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#define MAX_THREADS 128
static pthread_t threads[MAX_THREADS];
static int thread_in_use[MAX_THREADS];

int thread_get_id(){
	int i;
	for(i=0; i<MAX_THREADS; i++)
		if(!thread_in_use[i])
			return i;
	return -1;
}

void thread_free_id(int id){
	thread_in_use[id] = 0;
}

void thread_func_wrapper(void *thread_args){
	ThreadArgs *args = thread_args;

	args->thread_func(args->args);

	thread_free_id(args->id);
}

void thread_manager_init(){
	int i;
	for(i=0; i<MAX_THREADS; i++)
		thread_in_use[i] = 0;
}

void thread_manager_launch(void *thread_func, void *thread_args){
	ThreadArgs *args = malloc(sizeof(*args));
	args->id = thread_get_id();
	assert( args->id != -1 );
	args->thread_func = thread_func;
	args->args = thread_args;

	thread_in_use[args->id] = 1;
	pthread_create(&threads[args->id], NULL, (void *)&thread_func_wrapper, (void *)args);
}
