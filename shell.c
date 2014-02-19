#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFSIZE 1000

char formatBuffer[10];

void exitShell(void);
void cdShell(void);

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

int main(int argc, char **argv){

    //pid and status for forking
    int pid, status;

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

    //needed for path to pass to exec
    char* path = "/bin/";

    char buffer[BUFSIZE];
    while (1){

        fgets(buffer, BUFSIZE, stdin);
        int buflen = strlen(buffer)-1;
        buffer[buflen] = '\0'; //remove '\n'
        if (parse(buffer, buflen, tokens, &tokensSize, argc, argv) > 0) {
            fprintf(stderr, "Failed on parsing arguments, invalid input.\n");
            return 1;
        }
        
        printf("number of tokens: %d\n\nTokens are:\n", tokensSize);
        int i;
        for (i = 0; i < tokensSize; ++i) {
            printf(format(&tokens[i]), buffer + tokens[i].start);
        }

        printf("number of commands: %d\n\nCommands are:\n", cmdsSize);
        tokensToCommands(buffer, tokens, tokensSize, cmds, &cmdsSize);
        for (i = 0; i < cmdsSize; ++i) {
            printf("%s - %s\n", cmds[i].cmd, cmds[i].args);
        }

        tokensSize = 0;
        cmdsSize = 0;

        //here check builtins for 

        //first try pipe for two commands
        int pipefd[2];

        pid = fork();

        if(pid < 0){
            //error
            fprintf(stderr, "Fork failure.\n");
        }else if(pid == 0){
             //in child process
            //another command means need to fork again
            pid = fork();

            if(pid < 0){
                fprintf(stderr, "Fork failure.\n");
            }else if(pid == 0){
                //in child process

                //two pipes between two processes
                close(pipefd[0]); //child does not need the end of this pipe
                dup2(pipefd[1], 1); 
                close(pipefd[1]);

                //temporary path used to cat command after to pass to exec
                char binpath[1024];

                strcpy(binpath, path);
                strcat(binpath, cmds[0].cmd);

                execl(binpath, (char *)cmds[0].args);

            }else{
                //wait for child processes to die, this is the parent
                close(pipefd[1]); //parent does not need the end of this pipe
                dup2(pipefd[0], 0);
                close(pipefd[0]);

                //temporary path used to cat command after to pass to exec
                char binpath[1024];
                strcpy(binpath, path);
                strcat(binpath, cmds[0].cmd);

                execl(binpath, (char *)cmds[0].args);
            }

        }else{
            //wait for child processes to die, this is the parent
            //print status
            pid = wait(&status);
        }
    }
    return(0);
}
