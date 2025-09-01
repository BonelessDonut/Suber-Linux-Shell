#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void printPrompt();
void removeNewLine(char*);
void mainLoop();
char** tokenParser(char*);
int tokenCounter(char*);
void programCaller(char**, int*, int, int, int, int**, int);
void redirectAndParse(char*, int*, int, int, int**, int);
void checkPipes(char*, int*);

int main(){
	mainLoop();	
	return 0;
}

int checkStringEmpty(char* s){
	int i;
	i = 0;
	while (s[i] != '\0' && s[i] != '\n'){
		i++;
	}
	return (i == 0) ? 1 : 0;
}

void mainLoop(){
	char buf[1024];
	int running;
	int i;
	running = 1;
	while (running){
		printPrompt();
		fgets(buf, 1024, stdin);
		i = checkStringEmpty(buf);
		if (i == 1){
			continue;
		}
		checkPipes(buf, &running);
	}
}

void checkPipes(char* buf, int* running){
	int i;
	int *pfd;
	int n;
	int **pipeFds;
	char **commands;
	char *s;
	n = 0;
	for (i = 0; buf[i] != '\0' && buf[i] != '\n'; i++)
		if (buf[i] == '|')
			n++;
	commands = calloc((n+2), sizeof(char*));
	commands[0] = buf;
	pipeFds = calloc((n+2), sizeof(int*));
	for (i = 1; i < n+1; i++){
		pfd = calloc(2, sizeof(int));
		pipe(pfd);
		if (pipe(pfd) == -1){
			perror("pipe");
			exit(1);
		}
		pipeFds[i] = pfd;
	}
	pfd = calloc(2, sizeof(int));
	if (pipe(pfd) == -1){
		perror("pipe");
		exit(1);
	}
	pipeFds[0] = pfd;
	close(pipeFds[0][1]);
	dup2(STDIN_FILENO, pipeFds[0][0]);
	pfd = calloc(2, sizeof(int));
	pipe(pfd);
	if (pipe(pfd) == -1){
		perror("pipe");
		exit(1);
	}
	pipeFds[n+1] = pfd;
	close(pipeFds[n+1][0]);
	dup2(STDOUT_FILENO, pipeFds[n+1][1]);
	i = 0;
	s = strtok(buf, "|");
	while (s != NULL){
		commands[i] = s;
		i++;
		s = strtok(NULL, "|");
	}
	if (i < n+1){
		commands[i] = NULL;
	}
	for (i = 0; commands[i] != NULL; i++){
		redirectAndParse(commands[i], running, pipeFds[i][0], pipeFds[i+1][1], pipeFds, n);
	}
	for (i = 0; i < n+2; i++){
		close(pipeFds[i][0]);
		close(pipeFds[i][1]);
	}
	for (i = 0; commands[i] != NULL; i++){	
		wait(NULL);
	}
	free(commands);
	for (int i = 0; i < n+2; i++){
		free(pipeFds[i]);
	}
	free(pipeFds);
}

void redirectAndParse(char* buf, int* running, int inFd, int outFd, int **pipeFds, int n){	
	char** tokens;
	int i;
	char *input;
	char *output;
	int appending;
	int cur;
	int start;
	tokens = NULL;
	appending = 0;
	input = malloc(sizeof(char) * 64);
	output = malloc(sizeof(char) * 64);
	input[0] = '\0';
	output[0] = '\0';
	for (i = 0; buf[i] != '\0'; i++){
		if (buf[i] == '<'){
			buf[i] = ' ';
			cur = i+1;
			while (buf[cur] == ' '){
				cur++;
			}
			start = cur;
			while (buf[cur] != ' ' && buf[cur] != '\0' && buf[cur] != '\n' && buf[cur] != '>' && buf[cur] != '<' &&
			buf[cur] != ';'){
				cur++;
			}
			strncpy(input, buf+start, cur-start);
			input[cur-start] = '\0';
			inFd = open(input, O_RDONLY, 0);
			if (inFd < 0){
				perror("open");
				fprintf(stderr, "Filename: %s\n", input);
				return;
			}
			while(start < cur){
				buf[start] = ' ';
				start++;
			}
		}
		if (buf[i] == '>'){
			if (buf[i+1] == '>'){
				buf[i+1] = ' ';
				appending = 1;
			} else {
				appending = 0;
			}
			buf[i] = ' ';
			cur = i+1;
			while (buf[cur] == ' '){
				cur++;
			}
			start = cur;
			while (buf[cur] != ' ' && buf[cur] != '\0' && buf[cur] != '\n' && buf[cur] != '<' && buf[cur] != '>' &&
			buf[cur] != ';'){
				cur++;
			}
			strncpy(output, buf+start, cur-start);
			output[cur-start] = '\0';
			if (appending == 1){
				outFd = open(output, O_APPEND | O_WRONLY | O_CREAT, 0644);
			} else {
				outFd = creat(output, 0644);
			}
			if (outFd < 0){
				perror("open");
				fprintf(stderr, "Filename: %s\n", output);
				return;
			}
			while(start < cur){
				buf[start] = ' ';
				start++;
			}
		}
	}
	tokens = tokenParser(buf);
	programCaller(tokens, running, inFd, outFd, appending, pipeFds, n);	
	i = 0;
	while (tokens[i] != NULL){
		free(tokens[i]);
		tokens[i] = NULL;
		i++;
	}
	free(input);
	free(output);
	free(tokens);
}

char** tokenParser(char *buf){	
	char **tokens;
	char *token;
	int bufLimit;
	int bufProgress;
	int numTokens;
	int n;
	int args;
	bufProgress = 0;
	numTokens = 0;
	token = malloc(sizeof(char) * (strlen(buf) + 1));
	strcpy(token, buf);
	args = tokenCounter(token);
	free(token);
	token = NULL;
	tokens = malloc(sizeof(char*) * (args + 1));
	for (n = 0; n < args + 1; n++){
		tokens[n] = NULL;
	}
	token = strtok(buf, " \t\n");
	while (token != NULL && numTokens <= args){	
		removeNewLine(token);
		n = strlen(token);
		if (bufProgress + n >= 1024)
			break;
		tokens[numTokens] = malloc(sizeof(char) * (n + 1));
		strcpy(tokens[numTokens], token);
		numTokens++;
		bufProgress += n;	
		token = strtok(NULL, " \t\n");
	}

	return tokens;
}

int tokenCounter(char *buf){
	int counter;
	char* token;

	counter = 0;
	token = strtok(buf, " \t\n");
	if (token == NULL || (strlen(token) == 0)){
		return 0;
	}
	while (token != NULL){
		counter++;
		token = strtok(NULL, " \t\n");
	}
	return counter;
}

void programCaller(char** tokens, int *running, int input, int output, int appending, int **pipeFds, int n){
	int pid, i;
	char *path;

	if( strcmp(tokens[0], "exit") == 0){
		*running = 0;
		return;
	} else if (strcmp(tokens[0] ,"cd") == 0){
		if (tokens[1] == NULL){
			path = getenv("HOME");
		} else {
			path = tokens[1];
		}
		chdir(path);
	} else if (strcmp(tokens[0], "help") == 0){
			printf("Use \"exit\" to leave. \"cd\" to change directories, and other program names as usual to run them.\n");	
	} else {
		pid = fork();
		if (pid == 0){
			if (input >= 0){
				if (dup2(input, 0) == -1){
					perror("dup2");
					exit(1);
				}
			}
			if (output >= 0){
				if (dup2(output, 1) == -1){
					perror("dup2");
					exit(1);
				}
			}

			for (i = 0; i < n+2; i++){
				close(pipeFds[i][0]);
				close(pipeFds[i][1]);
			}
			
			if (execvp(tokens[0], tokens) < 0){
				printf("\nCould not execute command: %s\n", tokens[0]);
				exit(1);
			}
		}
	}
}

void removeNewLine(char* s){
	int i;
	i = 0;
	while (s[i] != '\0'){
		if (s[i] == '\n'){
			s[i] = '\0';
			break;
		}
		i++;
	}
}

void printPrompt(){
	char cwd[1024];	
	getcwd(cwd, sizeof(cwd));
	printf("%s--> ", cwd);	
}
