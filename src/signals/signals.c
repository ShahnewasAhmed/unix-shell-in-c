#include "signals.h"
#include <stddef.h>

volatile sig_atomic_t child_exited = 0;

static void sigchld_handler(int signum)
{
    (void)signum;
    child_exited = 1;
}

void setup_signal_handlers(void)
{
    struct sigaction sa = {0};

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    sigaction(SIGCHLD, &sa, NULL);
}