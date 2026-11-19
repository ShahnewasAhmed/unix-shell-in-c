#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"
int execute_node(ASTNode *node);
void reap_background_processes(void);
void print_background_jobs(void);
#endif