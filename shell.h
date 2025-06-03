#ifndef SHELL_SHELL_H
#define SHELL_SHELL_H

#include <stdbool.h>

void skipCommand(List *lp);
bool acceptToken(List *lp, char *ident);
bool isOperator(char *s);
bool parseExecutable(List *lp);
bool parseOptions(List *lp);
bool parseCommand(List *lp);
bool parsePipeline(List *lp);
bool parseFileName(List *lp);
bool parseRedirections(List *lp);
bool parseBuiltIn(List *lp);
bool parseChain(List *lp);
bool parseInputLine(List *lp);
bool status();
void setup_signal_handlers();
void cleanupBackgroundProcesses();

#endif
