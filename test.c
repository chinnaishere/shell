#include <stdlib.h>
#include <stdio.h>

char *cmd1[] = { "cat", "/etc/passwd", 0 };	/* pk: changed text to the password file */
char *cmd2[] = { "tr", "A-Z", "a-z", 0 };
char *cmd3[] = { "tr", "-C", "a-z", "\n", 0 };
char *cmd4[] = { "sort", 0 };

void runcmd(int in, int out, char **cmd);	/* pk: changed char * to char ** */

int main(int argc, char **argv){
    int pid, status;
    int fd_a[2];
    int fd_b[2];
    int fd_c[2];

    commandsize = 4;
    currcmd = 0;
    while()
    pipe(fd_a);	/* pk: pipe from cmd1 to cmd2 */

    runcmd(-1, fd_a[1], cmd1);	/* pk: run cmd 1 */

    close(fd_a[1]); /* pk: the parent and future kids don't need the writing end of the pipe */
    pipe(fd_b);	/* pk: pipe from cmd1 to cmd2 */

    runcmd(fd_a[0], fd_b[1], cmd2); /* pk: run cmd 2 */

    close(fd_a[0]); /* pk: the parent doesn't need the reading end of the first pipe */
    close(fd_b[1]); /* pk: the parent and future kids don't need the writing end of the second pipe */

    pipe(fd_c);
    runcmd(fd_b[0], fd_c[1], cmd3); /* pk: run cmd 2 */

    close(fd_b[0]); /* pk: the parent doesn't need the reading end of the first pipe */
    close(fd_c[1]); /* pk: the parent and future kids don't need the writing end of the second pipe */

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
