#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define MAX_BG_COUNT 70
#define MAX_FG_COUNT 70
#define FG_PGRP_ID 1000
#define BG_PGRP_ID 2000

/* Splits the string by space and returns the array of tokens */


char **tokenize(char *line, int* number_of_tokens, int *multiple)
{
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
  	int i, tokenIndex = 0, tokenNo = 0;

  	for(i =0; i < strlen(line); i++){

    	char readChar = line[i];

    	if (readChar == ' ' || readChar == '\n' || readChar == '\t'){
      		token[tokenIndex] = '\0';
      		if (tokenIndex != 0){
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				if(strcmp(token, "&&") == 0){
					*multiple = 1;
				}
				if(strcmp(token, "&&&") == 0){
					*multiple = 2;
				}
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0; 
        	}
    	}
		else {
      		token[tokenIndex++] = readChar;
    	}
  	}

	*number_of_tokens = tokenNo;
  	free(token);
  	tokens[tokenNo] = NULL ;
  	return tokens;
}


char **background_responses;
int *background_pid;
int *foreground_pid;
int response_count = 0;

int front_id = -2;
void catch_dead(int a){
	for (int i = 0; i < MAX_BG_COUNT; i++) {
		int pid = 0, status;
		if(background_pid[i] == 0) continue;
		pid = waitpid(background_pid[i], &status, WNOHANG);
		if(pid >0){
			background_responses[response_count] = (char *)malloc(100 * sizeof(char));
			snprintf(background_responses[response_count], 100, "Infinity: Background process with pid: [%d] ended with exit code: %d.\n", pid, WEXITSTATUS(status));
			response_count++;
			background_pid[i] = 0;
		}
	}

	for(int i = 0; i < MAX_FG_COUNT; i++){
		int pid = 0, status;
		if(foreground_pid[i] == 0) continue;
		pid = waitpid(foreground_pid[i], &status, WNOHANG);
		if(pid > 0){
			foreground_pid[i] = 0;
		}
	}

	signal(SIGCHLD, catch_dead);
}

void background_process(char **tokens, int n){
	char **temp_tokens = (char **)malloc(n*sizeof(char *));
	for(int i = 0; tokens[i] != NULL; i++){
		temp_tokens[i] = strdup(tokens[i]);
	}
	temp_tokens[n-1] = NULL;
	
	int child_pid = fork();
	if(child_pid < 0){
		perror("Fork Failed.");
	}
	else if(child_pid == 0){
		// Child Process.
		if(setpgid(0, 0)){
			perror("Infinity");
		}
		
		int flag = execvp(temp_tokens[0], temp_tokens);
		if(flag < 0){
			perror("Infinity");
		}
	}
	else{
		printf("Background Process with pid: [%d] started.\n", child_pid);
		for(int i = 0; i < MAX_BG_COUNT; i++){
			if(background_pid[i] == 0){
				background_pid[i] = child_pid;
				break;
			}
		}
	}
}


void controlC_handler(int a){
	int check = 0;
	for(int i = 0; i < MAX_FG_COUNT; i++){
		if(foreground_pid[i] > 0){
			kill(foreground_pid[i], SIGKILL);
			check = 1;
		}
	}
	
	if(check == 1) printf("\n");
}

void multiple_commands(char **tokens, int multiple_check){
	char *str = (char *)malloc(5*sizeof(char));
	if(multiple_check == 1) str = strdup("&&");
	else str = strdup("&&&");

	char ***commands = (char ***)malloc(65*sizeof(char **));
	char **temp_token = (char **)malloc(MAX_NUM_TOKENS*sizeof(char *));
	int id = 0, cid = 0;
	for(int i = 0; tokens[i] != NULL; i++){
		if(strcmp(tokens[i], str) == 0){
			temp_token[id] = NULL;
			commands[cid++] = temp_token;
			temp_token = (char **)malloc(MAX_NUM_TOKENS*sizeof(char *));
			id = 0;
			continue;
		}
		temp_token[id] = strdup(tokens[i]);
		id++;
	}
	if(id != 0){
		temp_token[id] = NULL;
		commands[cid++] = temp_token;
		id = 0;
	}

	if(multiple_check == 1){
		// serial execution
		for(int i = 0; i < cid; i++){
			int child_pid = fork();
			if(child_pid < 0){
				perror("Infinity - Fork Failed");
			}
			if(child_pid == 0){
				for(int j = 0; j < MAX_FG_COUNT; j++){
					if(foreground_pid[j] == 0){
						foreground_pid[j] = getpid();
						break;
					}
				}
				int flag = execvp(commands[i][0], commands[i]);
				if(flag < 0){
					perror("Infinity");
					kill(getpid(), SIGKILL);
				}
			}
			else{
				waitpid(child_pid, NULL, 0);
				for(int j = 0; j < MAX_FG_COUNT; j++){
					if(foreground_pid[j] == child_pid){
						foreground_pid[j] = 0;
						break;
					}
				}
			}
		}
	}
	else{
		// parallel execution
		for(int i = 0; i < cid; i++){
			int child_pid = fork();
			if(child_pid == 0){
				int flag = execvp(commands[i][0], commands[i]);
				if(flag < 0){
					perror("Infinity");
					kill(getpid(), SIGKILL);
				}
			}
			else{
				for(int j = 0; j < MAX_FG_COUNT; j++){
					if(foreground_pid[j] == 0){
						foreground_pid[j] = child_pid;
						break;
					}
				}
			}
		}

		for(int i = 0; i < MAX_FG_COUNT; i++){
			if(foreground_pid[i] == 0) continue;
			waitpid(foreground_pid[i], NULL, 0);
			foreground_pid[i] = 0;
		}
		
	}

	for(int i = 0; i < cid; i++){
		for(int j = 0; commands[i][j] != NULL; j++){
			free(commands[i][j]);
		}
		free(commands[i]);
	}
	free(commands);
}

int main(int argc, char* argv[]) {
	signal(SIGCHLD, catch_dead);
	signal(SIGINT,controlC_handler);
	char  line[MAX_INPUT_SIZE];            
	char  **tokens;   
	background_responses = (char **)malloc(MAX_BG_COUNT*(sizeof(char *)));
	background_pid = (int *)malloc(MAX_BG_COUNT*(sizeof(int)));
	foreground_pid = (int *)malloc(MAX_FG_COUNT*(sizeof(int)));

	while(1) {			
		/* BEGIN: TAKING INPUT */
		bzero(line, sizeof(line));
		printf("$ ");
		scanf("%[^\n]", line);
		getchar();
		line[strlen(line)] = '\n'; 

	

		int n, multiple_check = 0;
		tokens = tokenize(line, &n, &multiple_check);
		if(n == 0) continue;
		if(strcmp(tokens[0], "exit") == 0){
			for(int i = 0; i < MAX_BG_COUNT; i++){
				kill(background_pid[i], SIGKILL);
				wait(NULL);
			}
			free(tokens[0]);
			free(tokens);
			return 0;
		}
		else if(multiple_check > 0){

			multiple_commands(tokens, multiple_check);			
			
		}
		else if(n-1 >= 0 && strcmp(tokens[n-1], "&") == 0){
			// now background process needs to implemented.
			//printf("Now in back\n");
			
			background_process(tokens, n);
		}
		else if(strcmp(tokens[0], "cd") == 0){
			// we need to change working directory
			int flag = chdir(tokens[1]);
			if(flag < 0){
				perror("Infinity");
			}
		}
		else{
			int child_id = fork();
			if (child_id < 0){
				perror("Fork Failed.\n");
			}
			else if (child_id == 0){
				// Now in the child process, so exec.
				if(setpgid(0, 0)){
					perror("Infinity Fore:");
				}
				int flag = execvp(tokens[0], tokens);
				if (flag < 0)
				{
					perror("Infinity");
					kill(getpid(), SIGKILL);
				}
			}
			else{
				for(int i = 0; i < MAX_FG_COUNT; i++){
					if(foreground_pid[i] == 0){
						foreground_pid[i] = child_id;
						break;
					}
				}
				waitpid(child_id, NULL, 0);
				for(int i = 0; i < MAX_FG_COUNT; i++){
					if(foreground_pid[i] == child_id){
						foreground_pid[i] = 0;
						break;
					}
				}
				// Wait for child to be terminated.
				// if (waitpid(child_id, NULL, 0) < 0){
				// 	perror("Wait For Termination Failed.\n");
				// }
			}
		}

		for(int i = 0; i < response_count; i++){
			printf("%s", background_responses[i]);
			free(background_responses[i]);
		}
		response_count = 0;

		// Freeing the allocated memory	
		for(int i=0;tokens[i]!=NULL;i++){
			free(tokens[i]);
		}
		free(tokens);
	}

	return 0;
}
