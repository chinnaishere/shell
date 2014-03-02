#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFSIZE 1000

char formatBuffer[10];

char *cmd1[] = { "ls", "-al", "/", 0 };
char *cmd2[] = { "tr", "a-z", "A-Z", 0 };
char *cmd3[] = { "sort", 0 };
char *cmd4[] = { "wc", 0 };

void exitShell(void);
void cdShell(void);
void runsource(int pfd[], char* cmd, char* arguments[]);
void runmiddle(int pfd[], char* cmd, char* arguments[], int pos);
void rundest(int pfd[], char* cmd, char* arguments[], int pos);

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

/*set command and token memory to null for next userinput*/
void reset(Command cmds[50], Token tokens[50]){
    for(int k = 0; k <= 49; k++){
        cmds[k].cmd = NULL;
        cmds[k].args = NULL;
        tokens[k].start = 0;
        tokens[k].len = 0;
    }
}

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
    while (1){
        fgets(buffer, BUFSIZE, stdin);
        int buflen = strlen(buffer)-1;
        buffer[buflen] = '\0';

        if (parse(buffer, buflen, tokens, &tokensSize, argc, argv) > 0) {
            fprintf(stderr, "Failed on parsing arguments, invalid input.\n");
            return 1;
        }

        tokensToCommands(buffer, tokens, tokensSize, cmds, &cmdsSize);
        printf("command size: %d\n",cmdsSize);

        tokensSize = 0;

        //here check builtins cd/exit for, temporary fix for exit
        if(strncmp(cmds[0].cmd,"exit",4) == 0){
            exit(0);
        }

        int pid, status;

        //initialize array of pipes
        int fd[(cmdsSize*2)-2];

        //initialize all pipes, increments of two because 2 FDs
        for(int j = 0; j <=((cmdsSize*2)-2); j = j+2){
            pipe(fd+j);
        }

        int currcmd = 0;
        int curpipe = 0;
        //send every command to source(initial), middle or dest methods to pipe, fork and dup2
        while(currcmd < cmdsSize){
            if(currcmd == 0){
                printf("source: %s\n", cmds[currcmd].cmd);
                runsource(fd, cmds[currcmd].cmd, &cmds[currcmd].args);
            }else if(currcmd != (cmdsSize-1)){
                printf("middle: %s\n", cmds[currcmd].cmd);
                runmiddle(fd,cmds[currcmd].cmd, &cmds[currcmd].args,curpipe);
            }else{
                printf("destination: %s\n", cmds[currcmd].cmd);
                rundest(fd,cmds[currcmd].cmd, &cmds[currcmd].args,curpipe);
            }

            currcmd++;
            curpipe = curpipe+2;
        }

        //close both file descriptors on the pipe
        for(int h =0; h < (cmdsSize*2)-2; h++){
            close(fd[h]);
        }

        //pick up all dead children
        while ((pid = wait(&status)) != -1){
            fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
        }

        reset(cmds,tokens);
        cmdsSize = 0;
        tokensSize = 0;
        }
    exit(0);
}

void runsource(int pfd[], char* cmd, char* arguments[])        /* run the first part of the pipeline, cmd1 */
{
        int pid;        /* we don't use the process ID here, but you may wnat to print it for
debugging */

        switch (pid = fork()) {

        case 0: /* child */
                dup2(pfd[1], 1);        /* this end of the pipe becomes the standard output */
                close(pfd[0]);                 /* this process don't need the other end */
                close(pfd[2]);
                close(pfd[3]);
                execvp(cmd, arguments);        /* run the command */
                perror(cmd);        /* it failed! */

        default: /* parent does nothing */
                break;

        case -1:
                perror("fork");
                exit(1);
        }
}

void runmiddle(int pfd[], char* cmd, char* arguments[], int pos)        /* run the first part of the pipeline, cmd1 */
{
        int pid;        /* we don't use the process ID here, but you may wnat to print it for
debugging */

        switch (pid = fork()) {

        case 0: /* child */
                dup2(pfd[pos+1], 1);        /* this end of the pipe becomes the standard output */
                dup2(pfd[pos-2], 0);                 /* this process don't need the other end */

                for(int i = 1; i != pos; i++){
                    close(pfd[i]);

                }
                execvp(cmd, arguments);        /* run the command */
                perror(cmd);        /* it failed! */

        default: /* parent does nothing */
                break;

        case -1:
                perror("fork");
                exit(1);
        }
}

void rundest(int pfd[], char* cmd, char* arguments[], int pos)        /* run the second part of the pipeline, cmd2 */
{
        int pid;

        switch (pid = fork()) {

        case 0: /* child */
                dup2(pfd[pos-2], 0);        /* this end of the pipe becomes the standard input */

                for(int i = 0; i < pos; i++){
                    if(i != pos-2){
                        close(pfd[i]);
                    }
                }
                execvp(cmd, arguments);        /* run the command */
                perror(cmd);        /* it failed! */

        default: /* parent does nothing */
                break;

        case -1:
                perror("fork");
                exit(1);
        }
}