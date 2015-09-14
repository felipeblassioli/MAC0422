#include <sys/time.h>

double time_diff(struct timeval x, struct timeval y);
void sleep_ms(int ms);
double current_time();
double current_time_s();

void thread_manager_launch(void *, void *);
void thread_manager_init();
