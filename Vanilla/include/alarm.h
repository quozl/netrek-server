#include <sys/types.h>
void alarm_init();
void alarm_set();
void alarm_ignore();
void alarm_prevent_inheritance();
void alarm_handler(int signum);
void alarm_wait_for();
int alarm_send(pid_t pid);
void alarm_setitimer(int distortion, int fps);
