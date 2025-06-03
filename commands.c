#define _POSIX_C_SOURCE 200809L
#include "commands.h"
#include "shell.h"
#include <stdlib.h> // for setenv
#include <unistd.h> // getcwd

#define MAX_PATH 1024


int cd(List *lp) {
    if (*lp == NULL || (*lp)->t == NULL) {
        printf("Error: cd requires folder to navigate to!\n");
        return 2;
    }
    
    if (chdir((*lp)->t) != 0) {
        printf("Error: cd directory not found!\n");
        return 2;
    }
    
    //Update PWD environment variable
    if (setenv("PWD", (*lp)->t, 1) != 0) {
        perror("setenv failed");
    }
    return 0;
}
