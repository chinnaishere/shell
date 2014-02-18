#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 1000

struct builtins { 
	char *name; /* name of function */ 
	int (*f)(); /* function to execute for the built-in command */ 
};

int main()
{
	//create builtins
	struct builtins builtins[2];
	builtins[0].name = "cd";
	builtins[1].name = "exit";

	char buffer[BUFSIZE];
	while (1)
	{
		fgets(buffer, BUFSIZE, stdin);
		buffer[strlen(buffer)-1] = '\0'; //remove '\n'
		

		printf("MSG:\"%s\"\n", buffer);
	}
	return 0;
}
