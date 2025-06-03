#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "scanner.h"
#include "shell.h"

int main(int argc, char const *argv[])
{    
    setbuf(stdout, NULL);
    setbuf(stdout, NULL);
    char *inputLine;
    List tokenList, tokenListCopy;

    setup_signal_handlers();

    while (true) {
        cleanupBackgroundProcesses();
        inputLine = readInputLine();

        if (inputLine == NULL || feof(stdin))       // checks EOF
            break;

        tokenList = getTokenList(inputLine);        // getting the tokenList of inputLine
        tokenListCopy = tokenList;                  // making a copy to the start of the tokenList
                                                    // to avoid memory leaks
        bool parse = parseInputLine(&tokenList);    // parsing the input line

        if (tokenList == NULL && parse) 
            printList(tokenList);

        free(inputLine);
        freeTokenList(tokenList); 
        freeTokenList(tokenListCopy);
    }
    
    return 0;
}