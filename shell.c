#define _POSIX_C_SOURCE 200809L
#define MAX_ARGS 100
#define MAX_COMMANDS 10
#define MAX_COMMAND_LENGTH 100
#define MAX_BACKGROUND_PROCESSES 100
#define MAX_SIGNAL 31
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include "scanner.h"
#include "commands.h"

typedef struct                                                      // struct for managing bg processes
{
    pid_t pid;
    int index;
} BackgroundProcess;
BackgroundProcess backgroundProcesses[MAX_BACKGROUND_PROCESSES];    // array to manage bg processes
int backgroundProcessCount = 0;                                     // keep track of num of bg processes
int nextProcessIndex = 1;                                           // starting at index 1

int exitCode = 0;                                                   // for storing exit code
bool commandNotFound = false;
bool isBackground = false;

pid_t foregroundPID = -1;                                                // keep track of currently executing foreground process

int keeptrack = 0;

/**
 * The function addBackgroundPID adds a new bg process to the
 * backgroundProcesses array
 *
 * @param pid process ID of the newly created bg process.
 */
void addBackgroundPID(pid_t pid)
{
    if (backgroundProcessCount < MAX_BACKGROUND_PROCESSES)
    {
        backgroundProcesses[backgroundProcessCount].pid = pid;
        backgroundProcesses[backgroundProcessCount].index = nextProcessIndex;
        backgroundProcessCount += 1;
    }
} 

/**
 * The function removeBackgroundPID removes a bg process from the
 * backgroundProcesses array
 *
 * @param pid process ID of the to be removed bg process.
 */
void removeBackgroundPID(pid_t pid) 
{
    int i;
    for (i = 0; i < backgroundProcessCount; ++i)
    {
        if (backgroundProcesses[i].pid == pid)
        {
            for (int j = i; j < backgroundProcessCount - 1; ++j)
            {
                backgroundProcesses[j] = backgroundProcesses[j + 1];
            }
            backgroundProcessCount--;
            break;
        }
    }
}

/**
 * The function cleanupBackgroundProcesses cleans any bg process that has
 * terminated.
 */
void cleanupBackgroundProcesses() 
{
    int status;
    pid_t pid;

    for (int i = backgroundProcessCount - 1; i >= 0; --i)
    {
        pid = waitpid(backgroundProcesses[i].pid, &status, WNOHANG);
        if (pid > 0)
        {
            removeBackgroundPID(pid);
        }
    }
}

/**
 * The function backgroundProcessListIsEmpty checks whether there
 * are any bg processes currently being tracked.
 */
bool backgroundProcessListIsEmpty()
{
    return backgroundProcessCount == 0;
}

/**
 * the function sigchld_handler handles SIGCHLD signals, which are
 * sent to a process when one of its child processes terminates/stops.
 * Cleans up child processes that have completed their execution to
 * prevent them from becoming zombie processes.
 * @param sig represents the signal number (SIGCHLD)
 * @param siginfo a pointer to siginfo that provides info such as ID
 * @param context holds the execution context
*/
void sigchld_handler(int sig, siginfo_t *siginfo, void *context)
{
    (void)sig;
    (void)context;

    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        removeBackgroundPID(pid);
    }
}

/**
 * the function sigint_handler handles SIGINT signals, which are
 * sent when the user presses Ctrl+C.
 * Handles interruption requests, specifically targeting the foreground
 * process
 * @param sig represents the signal number
*/
void sigint_handler(int sig)
{
    if (foregroundPID > -1) {
        // Send SIGINT to the process group of the foreground process
        kill(-foregroundPID, SIGINT);        
    }
    else
    {
        if (backgroundProcessListIsEmpty())
            exit(exitCode); 
        else
            printf("Error: there are still background processes running!\n");
    }
}

/**
 * the function setup_signal_handlers configures signals setup
*/
void setup_signal_handlers()
{
    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_flags = SA_RESTART | SA_SIGINFO;
    sa_chld.sa_sigaction = sigchld_handler;
    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1)
    {
        perror("sigaction SIGCHLD");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_flags = SA_RESTART;
    sa_int.sa_handler = sigint_handler;
    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("sigaction SIGINT");
        exit(EXIT_FAILURE);
    }
}

/**
 * The function command_kill is one of the built-in commands of the shell.
 * Terminates and/or send signals to background processes.
 * Allows the user to specify a process by its index, rather than its PID.
 * 
 * @param idxStr represents the index of the background process.
 * @param (optional) parameter represents the signal number to be sent to
 * the background process.
*/
void command_kill(char *idxStr, char *sigStr) {
    if (idxStr == NULL) {
        printf("Error: command requires an index!\n");
        exitCode = 2;
        return;
    }

    char *endptr;
    long idx = strtol(idxStr, &endptr, 10);

    if (*endptr != '\0' || idx <= 0 || idx >= nextProcessIndex) {
        printf("Error: invalid index provided!\n");
        exitCode = 2;
        return;
    }

    int sig = SIGTERM; // Default signal
    if (sigStr != NULL && strlen(sigStr) > 0) {
        long sigNum = strtol(sigStr, &endptr, 10);
        if (*endptr != '\0' || sigNum <= 0) {
            printf("Error: invalid signal provided!\n");
            exitCode = 2;
            return;
        }
        sig = (int)sigNum;
    }

    pid_t pid = -1;
    for (int i = 0; i < backgroundProcessCount; ++i) {
        if (backgroundProcesses[i].index == idx) {
            pid = backgroundProcesses[i].pid;
            break;
        }
    }

    if (pid == -1) {
        printf("Error: this index is not a background process!\n");
        exitCode = 2;
        return;
    }

    if (kill(pid, sig) == -1) {
        perror("Error sending signal");
        exitCode = 2;
    }
}

/**
 * The function command_jobs is one of the built-in commands of the shell.
 * Lists all currently running background processes.
*/
void command_jobs()
{
    if (backgroundProcessCount == 0)
    {
        printf("No background processes!\n");
    }
    else
    {
        for (int i = backgroundProcessCount - 1; i >= 0; --i)
        {
            printf("Process running with index %d\n", backgroundProcesses[i].index);
        }
    }
}

/**
 * one of the built-in commands of the shell.
 * this function is used in parseBuiltIn.
 * this function returns the most recent exit code.
 * @return a bool denoting the sucess of its execution
*/
bool status()
{
    printf("The most recent exit code is: %d\n", exitCode);
    return true;
}

/**
 * The function skipCommand is used for handling Composition.
 * It skips commands when needed. (e.g. in OR cases).
 * @param lp List pointer to the start of the tokenlist.
*/
void skipCommand(List *lp) {
    if (*lp == NULL || (*lp)->next == NULL)
        return;

    // Move to the next token
    *lp = (*lp)->next;

    // Skip tokens until the next operator or end of list is encountered
    while (*lp != NULL && !isOperator((*lp)->t)) {
        *lp = (*lp)->next;
    }

    // Move to the next token if an operator is encountered
    if (*lp != NULL && isOperator((*lp)->t))
        *lp = (*lp)->next;
}

/**
 * The function acceptToken checks whether the current token matches a target identifier,
 * and goes to the next token if this is the case.
 * @param lp List pointer to the start of the tokenlist.
 * @param ident target identifier
 * @return a bool denoting whether the current token matches the target identifier.
 */
bool acceptToken(List *lp, char *ident) {
    if (*lp != NULL && strcmp(((*lp)->t), ident) == 0) 
    {
        *lp = (*lp)->next;
        return true;
    }
    return false;
}

/**
 * Checks whether the input string \param s is an operator.
 * @param s input string.
 * @return a bool denoting whether the current string is an operator.
 */
bool isOperator(char *s) {
    // NULL-terminated array makes it easy to expand this array later
    // without changing the code at other places.
    char *operators[] = {
            "&",
            "&&",
            "||",
            ";",
            "<",
            ">",
            "|",
            NULL
    };

    for (int i = 0; operators[i] != NULL; i++) {
        if (strcmp(s, operators[i]) == 0) return true;
    }
    return false;
}

/**
 * The main function of our shell.
 * It processes the list to handle command execution including piping and 
 * i/o redirection and background processes.
 * 
 * PIPING && REDIRECTION:
 * If the pipe operator is encountered, a pipe is created using pipe(),
 * this fills pipfd array (of file descriptors) in which pipefd[0] is used 
 * for reading from the pipe and pipefd[1] for writing to the pipe.
 * 
 * OUTPUT REDIRECTION:
 * If the function runs into an output redirection operator, it opens the output file. 
 * The file descriptor for the opened file is stored in fd_out.
 * 
 * INPUT REDIRECTION:
 * If the function runs into an input redirection operator, it opens the input file
 * for reading and stores the file descriptor in fd_in
 * 
 * BG PROCESSES:
 * Manages background processes by calling appropriate functions and
 * updating the appropriate variables.
 * 
 * @param lp List pointer to the start of the tokenlist.
 * a bool denoting whether execution was successful 
*/

bool parseExecutable(List *lp) {
    int pipefd[2];
    pid_t pid;
    int status;
    int prev_pipe = STDIN_FILENO;
    // File descriptors for input and output redirection
    int fd_in = -1, fd_out = -1; 
    
    char *inputFile = NULL;
    char *outputFile = NULL;
    bool invalidSyntax = false;
    List backupList = *lp;

    if (keeptrack == 0)
    {
        while (backupList != NULL) {
            if (strcmp(backupList->t, "&") == 0) {
                isBackground = true;
                ++keeptrack;
                break; // Exit the loop if "&" is found.
            }
            backupList = backupList->next; // Move to the next item in the list.
            ++keeptrack;
        }
    }    
    // Iterate through the list until an operator is encountered
    while (*lp != NULL && !isOperator((*lp)->t)) {

        char *args[MAX_ARGS + 1];
        int i = 0;
        bool hasPipe = false;
        
        char *firstFile = (*lp)->t;

        // loop until end of command
        while (*lp != NULL && i < MAX_ARGS) 
        {
            if (strcmp((*lp)->t, "&") == 0)
            {
                isBackground = true;
                break;
            }
            if (strcmp((*lp)->t, "|") == 0) 
            {
                hasPipe = true;
                (*lp) = (*lp)->next;
                break;
            } 
            else if (strcmp((*lp)->t, ">") == 0 || strcmp((*lp)->t, ">>") == 0) 
            {
                // Handle output redirection
                (*lp) = (*lp)->next;
                if ((*lp) != NULL) 
                {
                    fd_out = open((*lp)->t, strcmp((*lp)->t, ">") == 0 ? O_WRONLY | O_CREAT | O_TRUNC : O_WRONLY | O_CREAT | O_APPEND, 0644);
                    outputFile = (*lp)->t;
                    inputFile = firstFile;
                    if (fd_out == -1) 
                    {
                        perror("open");
                        return false;
                    }
                } else {
                    invalidSyntax = true;
                    printf("Error: invalid syntax!\n");
                    break;
                }
                (*lp) = (*lp)->next;
                continue;
            } 
            else if (strcmp((*lp)->t, "<") == 0) 
            {
                // Handle input redirection
                (*lp) = (*lp)->next;
                if ((*lp) != NULL)
                {
                    fd_in = open((*lp)->t, O_RDONLY);
                    inputFile = (*lp)->t;
                    outputFile = firstFile;
                    if (fd_in == -1) 
                    {
                        perror("open");
                        return false;
                    }
                } else{
                    invalidSyntax = true;
                    printf("Error: invalid syntax!\n");
                    break;
                }
                (*lp) = (*lp)->next;
                continue;
            } 
            
            if (isOperator((*lp)->t)) 
                break;
            
            args[i] = (*lp)->t; 
            *lp = (*lp)->next;
            i++;
        }
        if (inputFile != NULL && outputFile != NULL && strcmp(inputFile, outputFile) == 0)
        {
            printf("Error: input and output files cannot be equal!\n");
            exitCode = 2;
            continue;
        }
        args[i] = NULL; // Null-terminate the arguments array

        // Execute the command with its arguments
        if (i > 0) 
        {
            if (hasPipe && pipe(pipefd) == -1) 
            {
                perror("pipe");
                return false;
            }

            pid = fork();
            if (pid == -1) 
            {
                perror("fork");
                return false;
            }
            if (pid > 0)
            {
                foregroundPID = pid;
                if (isBackground)
                {
                    addBackgroundPID(pid);
                    nextProcessIndex += 1;
                }
                else if (!isBackground || backgroundProcessCount <= 0)
                {
                    waitpid(pid, &status, 0);
                    if (WIFEXITED(status)) {
                        exitCode = WEXITSTATUS(status);
                    } else if (WIFSIGNALED(status)) {
                        int signalNum = WTERMSIG(status);
                        exitCode = 128 + signalNum; // Setting special exit code for signal termination
                    }
                    foregroundPID = -1; // Reset after the process completes
                }
            }
            // Child process
            if (pid == 0) 
            {
                if (setpgid(0, 0) == -1)
                {
                    perror("setpgid failed");
                    exit(EXIT_FAILURE);
                }
                if (fd_in != -1) 
                {
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                } 
                else 
                {
                    dup2(prev_pipe, STDIN_FILENO);
                    if (prev_pipe != STDIN_FILENO) 
                        close(prev_pipe);
                }
                if (fd_out != -1) 
                {
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                } 
                else if (hasPipe) 
                {
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }    
                if (!invalidSyntax && execvp(args[0], args) == -1){
                    printf("Error: command not found!\n");
                    exit(127);
                } else {
                    if (!isBackground)
                    {
                        waitpid(pid, &status, 0);
                        exitCode = WEXITSTATUS(status);
                        foregroundPID = -1;
                    } else {
                        addBackgroundPID(pid);
                    }
                }
                exit(EXIT_FAILURE);
            } 
            else 
            {  
                if (fd_in != -1) 
                    close(fd_in);
                if (fd_out != -1) 
                    close(fd_out);
                if (hasPipe) 
                    close(pipefd[1]);
                if (hasPipe) 
                    prev_pipe = pipefd[0];
                else 
                    prev_pipe = STDIN_FILENO;
            }
        }
    }
    if (isBackground)
    {
        isBackground = false;
    }
    else if (!isBackground)
    {
        waitpid(pid, &status, 0);
        foregroundPID = -1;
    }

    return true;
}

bool parseOptions(List *lp) {
    while (*lp != NULL && !isOperator((*lp)->t)) 
    {
        char *option = (*lp)->t;
        (void)option; // Mark as intentionally unused
        (*lp) = (*lp)->next;
    }
    return true;
}


/**
 * The function parseRedirections parses a command according to the grammar:
 *
 * <command>        ::= <executable> <options>
 *
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the command was parsed successfully.
 */
bool parseCommand(List *lp) {
    return parseExecutable(lp) && parseOptions(lp);
}

/**
 * The function parsePipeline parses a pipeline according to the grammar:
 *
 * <pipeline>           ::= <command> "|" <pipeline>
 *                       | <command>
 *
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the pipeline was parsed successfully.
 * We do not use this function for pipeline (please look at parseExecutable!)
 */

bool parsePipeline(List *lp) {
    int pipefd[2];
    pid_t pid;

    // Parse the first command in the pipeline
    if (!parseCommand(lp))
        return false;

    // If there is a pipe operator, create a pipe
    if (acceptToken(lp, "|")) 
    {
        if (pipe(pipefd) == -1) 
        {
            perror("pipe");
            return false;
        }

        pid = fork();
        if (pid == -1) 
        {
            perror("fork");
            return false;
        }

        if (pid == 0) 
        {  // Child process
            close(pipefd[0]);  // Close unused read end
            dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe
            close(pipefd[1]);  // Close write end after redirecting

            // Recursively parse the rest of the pipeline
            if (!parsePipeline(lp)) 
                exit(EXIT_FAILURE); 

            exit(EXIT_SUCCESS); // Exit child process successfully
        } 
        else // Parent process
        { 
            close(pipefd[1]);               // Close unused write end
            dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin to pipe
            close(pipefd[0]);               // Close read end after redirecting

            waitpid(pid, NULL, 0);          // Wait for child process to finish

                                            // Execute the next command in 
                                            // the parent process
            if (!parseCommand(lp))
                return false;
        }
    }
    return true;
}

/**
 * The function parseFileName parses a filename.
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the filename was parsed successfully.
 */
bool parseFileName(List *lp) {
    if (isEmpty(*lp))
        return false;

    char *fileName = (*lp)->t;
    (void)fileName; // Mark as intentionally unused

    (*lp) = (*lp)->next;
    return true;
}

/**
 * The function parseRedirections parses redirections according to the grammar:
 *
 * <redirections>       ::= <pipeline> <redirections>
 *                       |  <builtin> <options>
 *
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the redirections were parsed successfully.
 */
bool parseRedirections(List *lp) {
    if (isEmpty(*lp))
        return true;
    
    if (acceptToken(lp, "<")) 
    {
        if (!parseFileName(lp)) 
            return false;
        if (acceptToken(lp, ">")) 
            return parseFileName(lp);
        else 
            return true;
    } 
    else if (acceptToken(lp, ">")) 
    {
        if (!parseFileName(lp)) 
            return false;
        if (acceptToken(lp, "<")) 
            return parseFileName(lp);
        else 
            return true;
    }
    return true;
}

/**
 * The function parseBuiltIn parses a builtin.
 * BuiltIn commands include status, exit, cd, kill, and jobs.
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the builtin was parsed successfully.
 */
bool parseBuiltIn(List *lp) {

    char *builtIns[] = {
            "exit",
            "status",
            "cd",
            "kill",
            "jobs",
            NULL
    };
    for (int i = 0; builtIns[i] != NULL; i++) {
        if (acceptToken(lp, builtIns[i]))
        {
            if (strcmp(builtIns[i], "exit") == 0)
            {
                cleanupBackgroundProcesses();
                if (isBackground || backgroundProcessCount > 0)
                {
                    printf("Error: there are still background processes running!\n");
                    exitCode = 2;
                    return true;
                }
                else
                {
                    cleanupBackgroundProcesses();
                    exit(0); 
                }
            }
            else if (strcmp(builtIns[i], "status") == 0)
                return status();
            else if (strcmp(builtIns[i], "cd") == 0) 
            {
                exitCode = cd(lp);
                return true;
            }
            else if (strcmp(builtIns[i], "kill") == 0) {
                if (*lp == NULL) {
                    printf("! Error: command requires an index!\n");
                    return false;
                }
                char *idxStr = (*lp)->t;  // Get idx
                *lp = (*lp)->next;        // Move to next token, which could be sig or NULL

                char *sigStr = NULL;      // Initialize sig as NULL (default SIGTERM)
                if (*lp != NULL && isdigit((**lp).t[0])) {  // Check if next token is a signal number
                    sigStr = (*lp)->t;     // Get sig                    
                    *lp = (*lp)->next; 
                }
                command_kill(idxStr, sigStr);
                return true;
            }
            else if (strcmp(builtIns[i], "jobs") == 0)
            {
                command_jobs();
                return true;
            }
        }
    }
    return false;
}

/**
 * The function parseChain parses a chain according to the grammar:
 *
 * <chain>              ::= <pipeline> <redirections>
 *                       |  <builtin> <options>
 *
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the chain was parsed successfully.
 */
bool parseChain(List *lp) {
    if (isEmpty(*lp))
        return false;

    if (parseBuiltIn(lp))
        return parseOptions(lp);

    if (!isEmpty((*lp)->next) && strcmp((*lp)->next->t, "|") != 0)
    {
        if (parseExecutable(lp))
            return parseOptions(lp);
    } 
    if (!isEmpty((*lp)->next))
    {
        if (strcmp((*lp)->next->t, "|") == 0) 
        {
            if (parsePipeline(lp))
                return parseRedirections(lp);
        } 
    }
 
    if (parseExecutable(lp))
        return parseOptions(lp);

    return false;
}

/**
 * The function parseInputLine parses an inputline according to the grammar:
 *
 * <inputline>      ::= <chain> & <inputline>
 *                   | <chain> && <inputline>
 *                   | <chain> || <inputline>
 *                   | <chain> ; <inputline>
 *                   | <chain>
 *                   | <empty>
 *
 * The logic for composition handling is taken care of in this function.
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the inputline was parsed successfully.
 */
bool parseInputLine(List *lp) {

    if (isEmpty(*lp))
        return true;

    if (!parseChain(lp))
        return false;

    if (acceptToken(lp, "&") || acceptToken(lp, "&&")) {
        if (exitCode == 0)
            return parseInputLine(lp);
        else
        {
            skipCommand(lp);
            return parseInputLine(lp);
        }
    } 

    else if (acceptToken(lp, "||")) 
    {
        if (exitCode != 0)
            return parseInputLine(lp);
        else
        {
            skipCommand(lp);
            return parseInputLine(lp);
        }
    } 
    else if (acceptToken(lp, ";"))
        return parseInputLine(lp);
    
    return true;
}