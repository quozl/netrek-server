int sigpipe_create();
void sigpipe_assign(int signum);
int sigpipe_fd();
int sigpipe_read();
void sigpipe_close();
void sigpipe_suspend(int signum);
void sigpipe_resume(int signum);
