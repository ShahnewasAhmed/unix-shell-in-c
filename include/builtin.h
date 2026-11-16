#ifndef BUILTIN_H
#define BUILTIN_H

int is_builtin(const char *name);
int execute_builtin(int argc, char **argv);

#endif