#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>

extern volatile sig_atomic_t child_exited;

void setup_signal_handlers(void);

#endif