#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFSIZE 1000

char formatBuffer[10];

struct Builtins { 
    char *name; /* name of function */ 
    int (*f)(); /* function to execute for the built-in command */ 
};

typedef struct Token {
    int start;
    int len;
} Token;

typedef struct Command {
    char *cmd; //cmd to call
    char *args; //args to pass to that call
} Command;

void exitShell(void);
void cdShell(void);

int parse(char *buffer, int buflen, Token *tokens, int *tokensSize, int argc, char **argv)
{
    int i, dQuoteOpen=0, sQuoteOpen=0;
    int start = -1; // start of token, end of token will be i, -1 means need a starting point
    for (i = 0; i < buflen; ++i) 
    {
        if (buffer[i] == '"' && !sQuoteOpen) {
            if (dQuoteOpen) { //closing double quote found, make token
                tokens[*tokensSize].start = start;
                tokens[*tokensSize].len = i - start;
                ++(*tokensSize);
                dQuoteOpen = 0;
                start = -1;
            } else {
                dQuoteOpen = 1;
                start = ++i; // assuming there won't be 2 double quotes back to back, i = '"', i+1=next char
            } 
        } else if (buffer[i] == '\'' && !dQuoteOpen) {
            if (sQuoteOpen) { //closing singe quote found, make token
                tokens[*tokensSize].start = start;
                tokens[*tokensSize].len = i - start;
                ++(*tokensSize);
                sQuoteOpen = 0;
                start = -1;
            } else {
                sQuoteOpen = 1;
                start = ++i; // assuming there won't be 2 double quotes back to back, i = '"', i+1=next char
            }
        } else { //any character or space
            if (sQuoteOpen || dQuoteOpen) 
                continue;
            if (start == -1) {
                if (buffer[i] == ' ') //space, continue onto next char, still looking for start
                    continue;
                start = i; //nonspace, now our starting point
                if (buffer[i] == '|') {
                    tokens[*tokensSize].start = start;
                    tokens[*tokensSize].len = 1;
                    ++(*tokensSize);
                    start = -1;
                }
            } else { //not looking for start, token is already building
                if (buffer[i] == ' ' || buffer[i] == '|') { //space or pipe found, token completed
                    tokens[*tokensSize].start = start;
                    tokens[*tokensSize].len = i - start;
                    ++(*tokensSize);
                    start = -1;
                    if (buffer[i] == '|') //dec i, loop incs and reruns on pipe. (less code)
                        --i;
                }
            }
        }
    }

    if (start > -1) { //loop ended with last token still not added to array
        if (dQuoteOpen || sQuoteOpen)
            return 1; //error
        tokens[*tokensSize].start = start;
        tokens[*tokensSize].len = i - start;
        ++(*tokensSize);
    }


    return 0;
}

char *format(Token *token)
{
    formatBuffer[0] = '%';
    formatBuffer[1] = '.';
    formatBuffer[2] = '\0';
    char len[5];
    sprintf(len, "%d", token->len);
    strcat(formatBuffer, len);
    strcat(formatBuffer, "s\n");
    return formatBuffer;
}

void tokensToCommands(char *buffer, Token *tokens, int tokensSize, Command *cmds, int *cmdsSize)
{
    int start = 0;
    int i;
    for (i = 0; i < tokensSize; ++i) {
        if (tokens[i].len == 1 && buffer[tokens[i].start] == '|') {
            char *tok = (char *)malloc(sizeof(char)*(tokens[start].len + 1));
            tok[0] = '\0';
            strncat(tok, buffer + tokens[start].start, tokens[start].len);
            cmds[*cmdsSize].cmd = tok;
            
            int j, argsize = 0;
            for (j = start+1; j < i; ++j) //go through all tokens up to the pipe which is in cell i, count size to malloc
                argsize += tokens[j].len + 1; //plus 1 for space and/or nul terminator
            tok = (char *)malloc(sizeof(char)*(argsize));
            tok[0] = '\0';
            for (j = start+1; j < i; ++j) { //go through all tokens up to the pipe which is in cell i
                strncat(tok, buffer + tokens[j].start, tokens[j].len);
                if (j < i-1)
                    strncat(tok, " ", 1);
            }

            cmds[*cmdsSize].args = tok;
            ++(*cmdsSize);
            start = ++i;
        }
    }

    //last command, not going  to be "commandized" in loop
    char *tok = (char *)malloc(sizeof(char)*(tokens[start].len + 1));
    tok[0] = '\0';
    strncat(tok, buffer + tokens[start].start, tokens[start].len);
    cmds[*cmdsSize].cmd = tok;

    int j, argsize = 0;
    for (j = start+1; j < i; ++j) //go through all tokens up to the pipe which is in cell i, count size to malloc
        argsize += tokens[j].len + 1; //plus 1 for space and/or nul terminator
    tok = (char *)malloc(sizeof(char)*(argsize + 1));
    tok[0] = '\0';
    for (j = start+1; j < i; ++j) {//go through all tokens up to the pipe which is in cell i
        strncat(tok, buffer + tokens[j].start, tokens[j].len);
        if (j < i-1)
            strncat(tok, " ", 1);
    }

    cmds[*cmdsSize].args = tok;
    ++(*cmdsSize);

}

char *cmd1[] = { "cat", "/etc/passwd", 0 };	/* pk: changed text to the password file */
char *cmd2[] = { "tr", "A-Z", "a-z", 0 };
char *cmd3[] = { "tr", "-C", "a-z", "\n", 0 };
char *cmd4[] = { "sort", 0 };

void runcmd(int in, int out, char **cmd);	/* pk: changed char * to char ** */

int main(int argc, char **argv){

    //create builtins
    struct Builtins builtins[2];
    builtins[0].name = "cd";
    builtins[1].name = "exit";

    //create tokens array
    Token tokens[50];
    int tokensSize = 0; //Number of elements in Token array

    //create commands array
    Command cmds[50];
    int cmdsSize = 0;

    char buffer[BUFSIZE];
    
    int pid, status;
    int fd_a[2];
    int fd_b[2];
    int fd_c[2];

    pipe(fd_a);	/* pk: pipe from cmd1 to cmd2 */

    runcmd(-1, fd_a[1], cmd1);	/* pk: run cmd 1 */

    close(fd_a[1]); /* pk: the parent and future kids don't need the writing end of the pipe */
    pipe(fd_b);	/* pk: pipe from cmd1 to cmd2 */

    runcmd(fd_a[0], fd_b[1], cmd2); /* pk: run cmd 2 */

    close(fd_a[0]); /* pk: the parent doesn't need the reading end of the first pipe */
    close(fd_b[1]); /* pk: the parent and future kids don't need the writing end of the second pipe */

    runcmd(fd_b[0], fd_c[1], cmd3);

    close(fd_b[0]);
    close(fd_c[1]);

    runcmd(fd_c[0], -1, cmd4);	 /* pk: run cmd 3 */

    close(fd_c[0]); /* pk: the parent doesn't need the reading end of the second pipe */

    while ((pid = wait(&status)) != -1) /* pick up all the dead children */
        fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
    exit(0);
}


void
runcmd(int in, int out, char **cmd)  /* run a command */
{
    int pid;

    switch (pid = fork()) {

    case 0: /* child */
        if (in >= 0) dup2(in, 0);	/* pk: change input source */
        if (out >= 0) dup2(out, 1); 	/* pk: change output destination */
	fprintf(stderr, "execvp(\"%s\")\n", cmd[0]);	/* pk: debug */
        execvp(cmd[0], cmd);  /* run the command */
        perror(cmd[0]);    /* it failed! */

    default: /* parent does nothing */
        break;

    case -1:
        perror("fork");
        exit(1);
    }
}
