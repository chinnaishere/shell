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

#define BUFSIZE 1000


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

char formatBuffer[10];
Command cmds[50];
int cmdsSize = 0;

void runonecmd(Command * cmd);
void runcmd(int in, int out, char **cmd);
void exitShell(void);
void cdShell(void);
void shell_cd(Command *);
void shell_exit(Command *);
int checkBuiltins(Command *, struct Builtins *);
void clean();

int parse(char *buffer, int buflen, Token *tokens, int *tokensSize, int argc, char **argv){
	int i, dQuoteOpen=0, sQuoteOpen=0;
	int start = -1; /* start of token, end of token will be i, -1 means need a starting point*/
	for (i = 0; i < buflen; ++i) 
	{
		if (buffer[i] == '"' && !sQuoteOpen) {
			if (dQuoteOpen) { /*closing double quote found, make token*/
				tokens[*tokensSize].start = start;
				tokens[*tokensSize].len = i - start;
				++(*tokensSize);
				dQuoteOpen = 0;
				start = -1;
			} else {
				dQuoteOpen = 1;
				start = ++i; /* assuming there won't be 2 double quotes back to back, i = '"', i+1=next char*/
			} 
		} else if (buffer[i] == '\'' && !dQuoteOpen) {
			if (sQuoteOpen) { /*closing singe quote found, make token*/
				tokens[*tokensSize].start = start;
				tokens[*tokensSize].len = i - start;
				++(*tokensSize);
				sQuoteOpen = 0;
				start = -1;
			} else {
				sQuoteOpen = 1;
				start = ++i; /* assuming there won't be 2 double quotes back to back, i = '"', i+1=next char*/
			}
		} else { /*any character or space*/
			if (sQuoteOpen || dQuoteOpen) 
				continue;
			if (start == -1) {
				if (buffer[i] == ' ') /*space, continue onto next char, still looking for start*/
					continue;
				start = i; /*nonspace, now our starting point*/
				if (buffer[i] == '|') {
					tokens[*tokensSize].start = start;
					tokens[*tokensSize].len = 1;
					++(*tokensSize);
					start = -1;
				}
			} else { /*not looking for start, token is already building*/
				if (buffer[i] == ' ' || buffer[i] == '|') { /*space or pipe found, token completed*/
					tokens[*tokensSize].start = start;
					tokens[*tokensSize].len = i - start;
					++(*tokensSize);
					start = -1;
					if (buffer[i] == '|') /*dec i, loop incs and reruns on pipe. (less code)*/
						--i;
				}
			}
		}
	}

	if (start > -1) { /*loop ended with last token still not added to array*/
		if (dQuoteOpen || sQuoteOpen)
			return 1; /*error*/
		tokens[*tokensSize].start = start;
		tokens[*tokensSize].len = i - start;
		++(*tokensSize);
	}

	return 0;
}

char *format(Token *token){
	formatBuffer[0] = '%';
	formatBuffer[1] = '.';
	formatBuffer[2] = '\0';
	char len[5];
	sprintf(len, "%d", token->len);
	strcat(formatBuffer, len);
	strcat(formatBuffer, "s\n");
	return formatBuffer;
}

void tokensToCommands(char *buffer, Token *tokens, int tokensSize){
	int start = 0;
	int i;
	for (i = 0; i < tokensSize; ++i) {
		/* i will hold the index of a pipe token*/
		if (tokens[i].len == 1 && buffer[tokens[i].start] == '|') {
			int j, a, argsize = i-start;
			cmds[cmdsSize].cmdargv = (char **)malloc(sizeof(char *) * (argsize+1));
			cmds[cmdsSize].cmdargv[argsize] = 0;
			for (j = start, a = 0; j < i; ++j, ++a) { /*go through all tokens up to the pipe which is in cell i*/
				char *tok = (char *)malloc(sizeof(char)*(tokens[j].len + 1));
				tok[0] = '\0';
				strncat(tok, buffer + tokens[j].start, tokens[j].len);
				cmds[cmdsSize].cmdargv[a] = tok;
			}

			cmds[cmdsSize].cmdargc = argsize;
			++cmdsSize;
			start = ++i;
		}
	}

	/*last command, not going  to be "commandized" in loop*/
	int j, a, argsize = i-start;
	cmds[cmdsSize].cmdargv = (char **)malloc(sizeof(char *) * (argsize+1));
	cmds[cmdsSize].cmdargv[argsize] = 0;
	for (j = start, a = 0; j < i; ++j, ++a) { /*go through all tokens up to the pipe which is in cell i*/
		char *tok = (char *)malloc(sizeof(char)*(tokens[j].len + 1));
		tok[0] = '\0';
		strncat(tok, buffer + tokens[j].start, tokens[j].len);
		cmds[cmdsSize].cmdargv[a] = tok;
	}

	cmds[cmdsSize].cmdargc = argsize;
	++cmdsSize;
}

int main(int argc, char **argv){

	/*create builtins*/
	struct Builtins builtins[2];
	builtins[0].name = "cd";
	builtins[0].f = &shell_cd;
	builtins[1].name = "exit";
	builtins[1].f = &shell_exit;

	/*create tokens array*/
	Token tokens[50];
	int tokensSize = 0; /*Number of elements in Token array*/

	char buffer[BUFSIZE];
	int pid, status;


	while (1){
		printf("\n$ ");
		fgets(buffer, BUFSIZE, stdin);
		int buflen = strlen(buffer)-1;
		buffer[buflen] = '\0';

		if (parse(buffer, buflen, tokens, &tokensSize, argc, argv) > 0) {
			fprintf(stderr, "Failed on parsing arguments, invalid input.\n");
			return 1;
		}

		tokensToCommands(buffer, tokens, tokensSize);

		/*when there is only one command, no pipes are needed*/
		if(cmdsSize == 1){
			if (checkBuiltins(&cmds[0], builtins)) {
				clean();
				cmdsSize = 0;
				tokensSize = 0;
				continue;
			}
			runonecmd(&cmds[0]);
		}else{
			/*show when you try any commands that have at least one pipe.*/
			char** allcommands[cmdsSize];
			int i;
			for (i = 0; i < cmdsSize; ++i)
				allcommands[i] = cmds[i].cmdargv;


			int currcmd = 0;
			/*runs the amount of commands inputted*/
			while(currcmd < cmdsSize){

				/*for the first command only one end of the pipe has to be closed*/
				if(currcmd == 0){
					pipe(cmds[currcmd].fd);
					runcmd(-1, cmds[currcmd].fd[1], allcommands[currcmd]);
					close(cmds[currcmd].fd[1]);
				}else if(currcmd == (cmdsSize-1)){
					/*for the last command the last pipe end has to be closed as well as printing out the process status*/
					runcmd(cmds[currcmd-1].fd[0], -1, allcommands[currcmd]);
					close(cmds[currcmd-1].fd[0]);

					/*this is the last command*/
					break;
				}else{
					/*for all commands in the middle the std in and out of the pipes must be closed*/
					pipe(cmds[currcmd].fd);
					runcmd(cmds[currcmd-1].fd[0], cmds[currcmd].fd[1], allcommands[currcmd]);
					close(cmds[currcmd-1].fd[0]); 
					close(cmds[currcmd].fd[1]); 
				}
				currcmd++;
			}

		}
		while ((pid = wait(&status)) != -1){
			fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
		}

		clean();
		cmdsSize = 0;
		tokensSize = 0;
	}
	exit(0);
}

/* runcmd takes in an array of commands terminated by null.
*/
void runcmd(int in, int out, char **cmd){
	int pid;

	switch (pid = fork()) {

		case 0: /* child */
			if (in >= 0){
				dup2(in, 0);   /* change input source */
			}
			if (out >= 0){
				dup2(out, 1);     /* change output destination */
			}
			execvp(cmd[0], cmd);  /* run the command */
			perror(cmd[0]);    /* it failed! */

		default: /* parent does nothing */
			break;

		case -1:
			perror("fork");
			exit(1);
	}
}

/*
	 runonecmd is set to run single commands, without using any instances of pipes.
	 */
void runonecmd(Command * cmd){
	int pid;
	/* char* temp[] = {cmd[0].cmd, cmd[0].args, NULL};*/
	switch (pid = fork()) {

		case 0: 
			/*child*/
			execvp(cmd->cmdargv[0], cmd->cmdargv);  /*run command*/
			perror(cmd->cmdargv[0]);    /*something went wrong!*/

		default: 
			/*parent does nothing*/
			break;

		case -1:
			perror("fork");
			exit(1);
	}
}

void shell_cd(Command *cmd)
{
	if (cmd->cmdargc == 1) { /*change to home dir*/
		if (chdir(getenv("HOME")) == -1) {
			perror("ERROR: Failed to change to HOME dir.");
			return;
		}
	} else if (cmd->cmdargc == 2) { /*change to path*/
		if (chdir(cmd->cmdargv[1]) == -1) {
			perror("ERROR: Failed to change dir.");
			return;
		}
	} else {
		fprintf(stderr, "ERROR:Invalid number of arguments for cd\n");
		return;
	}
	/*print current working dir*/
	char *dir = getcwd(NULL, 200);
	printf("\n%s", dir);
	free(dir);
}

void shell_exit(Command *cmd)
{
	if (cmd->cmdargc == 1) {
		clean();
		exit(0);
	}
	int code = atoi(cmd->cmdargv[1]);
	clean();
	exit(code);
}

int checkBuiltins(Command *cmd, struct Builtins builtins[])
{
	int i;
	for (i = 0; i < 2; ++i)
		if (strcmp(cmd->cmdargv[0], builtins[i].name) == 0) {
			(*builtins[i].f)(cmd);
			return 1;
		}
	return 0;
}

void clean()
{
	int i;
	for (i = 0; i < cmdsSize; ++i) {
		int j, cmdsargc = cmds[i].cmdargc;
		for (j = 0; j < cmdsargc; ++j)
			free(cmds[i].cmdargv[j]);
		free(cmds[i].cmdargv);
	}
}
