/*
 * *********************************************
 * * 416 Operating System Design Assignment 3
 * * Authors: Nikolay Feldman, Janelle Barcia
 * *********************************************
 * */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFSIZE 1024
#define MAXARGS 50


typedef struct Token {
	int start;
	int len;
} Token;

typedef struct Command {
	int cmdargc; /*number of arguments*/
	char **cmdargv; /*args to pass to that call*/
	int fd[2];
} Command;

struct Builtins{ 
	char *name; /* name of function */ 
	void (*f)(Command *); /* function to execute for the built-in command */ 
};


inline int aboveTokenMax(int);
void runonecmd(Command *);
void runcmd(int, int, char **);
void shell_cd(Command *);
void shell_exit(Command *);
int checkBuiltins(Command *, struct Builtins *);
void clean(void);
int parse(char *, int, Token *, int *);
void tokensToCommands(char *, Token *, int);
