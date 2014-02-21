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

void runonecmd(Command *cmd);
void runcmd(int in, int out, Command *cmd);   /* pk: changed char * to char ** */
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
            for (j = start; j < i; ++j) //go through all tokens up to the pipe which is in cell i, count size to malloc
                argsize += tokens[j].len + 1; //plus 1 for space and/or nul terminator
            tok = (char *)malloc(sizeof(char)*(argsize));
            tok[0] = '\0';
            for (j = start; j < i; ++j) { //go through all tokens up to the pipe which is in cell i
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
    for (j = start; j < i; ++j) //go through all tokens up to the pipe which is in cell i, count size to malloc
        argsize += tokens[j].len + 1; //plus 1 for space and/or nul terminator
    tok = (char *)malloc(sizeof(char)*(argsize + 1));
    tok[0] = '\0';
    for (j = start; j < i; ++j) {//go through all tokens up to the pipe which is in cell i
        strncat(tok, buffer + tokens[j].start, tokens[j].len);
        if (j < i-1)
            strncat(tok, " ", 1);
    }

    cmds[*cmdsSize].args = tok;
    ++(*cmdsSize);

}

/*set command and token memory to null for next userinput*/
void reset(Command cmds[50], Token tokens[50]){
      int k;
    for(k = 0; k <= 49; k++){
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
    int pid, status;

    int currcmd = 0;
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
        //pipe array, amount of commands -1, two file descriptors
         int fd[cmdsSize-1][2];
         //initializes pipe array
                int k;
        for(k = 0; k < (cmdsSize-1); k++){
            fd[k][0] = 0;
            fd[k][1] = 0;
        }
        //if there is only one command, no pipes are needed
        if(cmdsSize == 1){
            runonecmd(&cmds[0]);
            break;
        }else{
            //open and close pipes for every command
            while(currcmd < cmdsSize){
                if(currcmd == 0){
                    printf("calling source\n");
                    pipe(fd[currcmd]);
                    runcmd(-1, fd[currcmd][1], &cmds[currcmd]);
                    close(fd[currcmd][1]);
                }else if(currcmd != (cmdsSize-1)){
                    pipe(fd[currcmd]);
                    runcmd(fd[currcmd-1][0], fd[currcmd][1], &cmds[currcmd]);
                    close(fd[currcmd-1][0]);
                    close(fd[currcmd][1]);
                }else{
                    printf("calling destination\n");
                    runcmd(fd[currcmd-1][0], -1, &cmds[currcmd]);
                    close(fd[currcmd-1][0]);
                }
                currcmd++;
            }
            //closes all pipes
                        int j;
            for(j = 0; j < (cmdsSize-1); j++){
                close(fd[j][0]);
                close(fd[j][1]);
            }
        }

        //pick up all dead children and print process status
        while ((pid = wait(&status)) != -1){
            fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
        }
        reset(cmds,tokens);
        cmdsSize = 0;
        tokensSize = 0;
        }
    exit(0);
}

/*
runcmd is sed to run commands with multiple pipes that change the input source and destination.
*/
void runcmd(int in, int out, Command * cmd){
    int pid;

    switch (pid = fork()) {

    case 0: //child
        if (in >= 0) dup2(in, 0);   //change input source
        if (out >= 0) dup2(out, 1);     //change input destination
      fprintf(stderr, "execvp(\"%s\"), args: %s\n", cmd[0].cmd,cmd[0].args); 
        execvp(cmd[0].cmd, &cmd[0].args); 
        perror(cmd[0].cmd);  //something went wrong!

    default: //parent
        break;

    case -1:
        perror("fork");
        exit(1);
    }
}
/*
runonecmd is sed to run single commands, without using any instances of pipes.
*/
void runonecmd(Command * cmd){
    int pid;
   // char* temp[] = {cmd[0].cmd, cmd[0].args, NULL};
    switch (pid = fork()) {

    case 0: 
        //child
        fprintf(stderr, "execvp(\"%s\"), args: %s\n", cmd[0].cmd);
        execvp(cmd[0].cmd, &cmd[0].args);  //run command
        perror(cmd[0].cmd);    //something went wrong!

    default: 
        //parent does nothing
        break;

    case -1:
        perror("fork");
        exit(1);
    }
}