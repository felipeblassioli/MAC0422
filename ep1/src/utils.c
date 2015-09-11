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
