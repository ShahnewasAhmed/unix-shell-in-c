#include <stdio.h>
#include "signals.h"

void shell_loop(void);

int main(void)
{
    printf("Welcome to CSH!\n");
    setup_signal_handlers();
    shell_loop();
    return 0;
}