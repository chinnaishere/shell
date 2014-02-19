#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char **argv)
{
	//create builtins
	struct Builtins builtins[2];
	builtins[0].name = "cd";
	builtins[1].name = "exit";

	//create tokens array
	Token tokens[50];
	int tokensSize = 0; //Number of elements in Token array

	char buffer[BUFSIZE];
	while (1)
	{
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

		tokensSize = 0;

	//	printf("MSG:\"%s\"\n", buffer);
	}
	return 0;
}
