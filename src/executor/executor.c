#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#include "executor.h"
#include "builtin.h"

typedef struct BackgroundProcess
{
    pid_t pid;
    struct BackgroundProcess *next;
} BackgroundProcess;

static BackgroundProcess *background_process = NULL;

static int
apply_redirections(Redirection *redirections, int count)
{
    for (int i = 0; i < count; i++)
    {
        int target_fd;
        int flags;
        switch (redirections[i].type)
        {
        case TOKEN_INPUT:
            target_fd = STDIN_FILENO;
            flags = O_RDONLY;
            break;
        case TOKEN_OUTPUT:
            target_fd = STDOUT_FILENO;
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            break;
        case TOKEN_APPEND:
            target_fd = STDOUT_FILENO;
            flags = O_WRONLY | O_CREAT | O_APPEND;
            break;
        case TOKEN_ERROR:
            target_fd = STDERR_FILENO;
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            break;
        case TOKEN_ERROR_APPEND:
            target_fd = STDERR_FILENO;
            flags = O_WRONLY | O_CREAT | O_APPEND;
            break;
        default:
            fprintf(stderr, "Unsupported redirection\n");
            return -1;
        }
        int fd = open(redirections[i].filename, flags, 0644);
        if (fd == -1)
        {
            perror(redirections[i].filename);
            return -1;
        }
        if (dup2(fd, target_fd) == -1)
        {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}

static int execute_external(char **argv, Redirection *redirections, int redirection_count)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return 1;
    }
    if (pid == 0)
    {
        if (apply_redirections(redirections, redirection_count) == -1)
        {
            _exit(1);
        }

        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }

    int status;

    while (waitpid(pid, &status, 0) == -1)
    {
        if (errno != EINTR)
        {
            perror("waitpid");
            return 1;
        }
    }

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status))
    {
        return 128 + WTERMSIG(status);
    }

    return 1;
}

static void execute_command_in_child(ASTNode *node)
{
    if (node == NULL || node->type != NODE_COMMAND)
    {
        _exit(1);
    }
    char **argv = node->data.command.argv;
    int argc = node->data.command.argc;

    if (argc == 0 || argv == NULL || argv[0] == NULL)
    {
        _exit(1);
    }
    if (apply_redirections(node->data.command.redirections, node->data.command.redirection_count) == -1)
    {
        _exit(1);
    }
    if (is_builtin(argv[0]))
    {
        int status = execute_builtin(argc, argv);
        fflush(NULL);
        _exit(status);
    }
    execvp(argv[0], argv);

    perror(argv[0]);
    _exit(127);
}

static int execute_pipeline(ASTNode *node)
{
    int count = node->data.pipeline.count;
    ASTNode **commands = node->data.pipeline.commands;

    if (count < 1 || commands == NULL)
        return 1;

    if (count == 1)
        return execute_node(commands[0]);
    int pipe_count = count - 1;
    int (*pipes)[2] = calloc(pipe_count, sizeof(int[2]));
    pid_t *pids = calloc(count, sizeof(pid_t));

    if (pipes == NULL || pids == NULL)
    {
        perror("calloc");
        free(pipes);
        free(pids);
        return 1;
    }

    for (int i = 0; i < pipe_count; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("pipe");
            for (int j = 0; j < i; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            free(pipes);
            free(pids);
            return 1;
        }
    }

    int started = 0;
    for (int i = 0; i < count; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");

            for (int j = 0; j < started; j++)
            {
                if (kill(pids[j], SIGTERM) == -1 && errno != ESRCH)
                {
                    perror("kill");
                }
            }

            for (int j = 0; j < pipe_count; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            for (int j = 0; j < started; j++)
            {
                int status;

                while (waitpid(pids[j], &status, 0) == -1)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }

                    perror("waitpid");
                    break;
                }
            }
            free(pipes);
            free(pids);

            return 1;
        }
        else if (pid == 0)
        {
            if (i > 0)
            {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1)
                {
                    perror("dup2 stdin");
                    _exit(1);
                }
            }
            if (i < count - 1)
            {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1)
                {
                    perror("dup2 stdout");
                    _exit(1);
                }
            }
            for (int j = 0; j < pipe_count; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            execute_command_in_child(commands[i]);
        }
        pids[i] = pid;
        started++;
    }

    for (int i = 0; i < pipe_count; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    int last_status = 1;
    for (int i = 0; i < started; i++)
    {
        int status;
        while (waitpid(pids[i], &status, 0) == -1)
        {
            if (errno != EINTR)
            {
                perror("waitpid");
                status = -1;
                break;
            }
        }
        if (i == count - 1)
        {
            if (status == -1)
                last_status = 1;
            else if (WIFEXITED(status))
                last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                last_status = 128 + WTERMSIG(status);
        }
    }
    free(pipes);
    free(pids);
    return last_status;
}

static int execute_background(ASTNode *node)
{

    BackgroundProcess *job = malloc(sizeof(BackgroundProcess));

    if (job == NULL)
    {
        perror("malloc");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        free(job);
        perror("fork");
        return 1;
    }
    if (pid == 0)
    {
        free(job);
        int status = execute_node(node->data.background.command);
        fflush(NULL);
        _exit(status);
    }
    job->pid = pid;
    job->next = background_process;
    background_process = job;
    return 0;
}

void reap_background_processes(void)
{
    BackgroundProcess **current = &background_process;
    while (*current != NULL)
    {
        BackgroundProcess *job = *current;
        int status;
        pid_t result = waitpid(job->pid, &status, WNOHANG);
        if (result == job->pid)
        {
            *current = job->next;
            free(job);
        }
        else if (result == 0)
        {
            current = &job->next;
        }
        else if (result == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if (errno == ECHILD)
            {
                *current = job->next;
                free(job);
            }
            else
            {
                perror("waitpid");
                current = &job->next;
            }
        }
    }
}

int execute_node(ASTNode *node)
{
    if (node == NULL)
    {
        return 1;
    }
    if (node->type == NODE_PIPELINE)
    {
        return execute_pipeline(node);
    }
    if (node->type == NODE_AND)
    {
        int status = execute_node(node->data.binary.left);
        if (status == 0)
        {
            return execute_node(node->data.binary.right);
        }
        return status;
    }
    if (node->type == NODE_OR)
    {
        int status = execute_node(node->data.binary.left);
        if (status != 0)
        {
            return execute_node(node->data.binary.right);
        }
        return status;
    }
    if (node->type == NODE_SEQUENCE)
    {
        execute_node(node->data.binary.left);
        return execute_node(node->data.binary.right);
    }
    if (node->type == NODE_BACKGROUND)
    {
        return execute_background(node);
    }
    if (node->type != NODE_COMMAND)
    {
        fprintf(stderr, "Unsupported AST node\n");
        return 1;
    }
    char **argv = node->data.command.argv;
    int argc = node->data.command.argc;

    if (argc == 0 || argv == NULL || argv[0] == NULL)
    {
        return 1;
    }
    if (is_builtin(argv[0]))
    {
        return execute_builtin(argc, argv);
    }
    return execute_external(argv, node->data.command.redirections, node->data.command.redirection_count);
}
