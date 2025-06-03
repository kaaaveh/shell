#ifndef SCANNER_H
#define SCANNER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>

#define INITIAL_STRING_SIZE 10

typedef struct ListNode *List;

typedef struct ListNode {
    char *t;
    List next;
} ListNode;


char *readInputLine();

List getTokenList(char *s);

bool isEmpty(List l);

void printList(List l);

void freeTokenList(List l);

bool isOperatorCharacter(char c);

List newOperatorNode(char *s, int *start);

List newNode(char *s, int *start);

char *readInputLineFromFile(FILE *file);

#endif
