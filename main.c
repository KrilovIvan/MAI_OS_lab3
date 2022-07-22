#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define MAXNAME 256 // max filename length
#define LENGTH 256	// max length of comand

char cwd[MAXNAME];			 // current directory
char command[LENGTH] = {""}; // command buffer
char **args;				 // array with command and arguments

//signal "Ctrl+C" handler
void sig_handler(int sig)
{
	//if there is some copies of terminal no need to write cwd in each
	if(wait3(NULL, WNOHANG, NULL) >= 0)
		return;
	printf("\n%s$ ", cwd);
	fflush(stdout);
}

// split command by spaces
char **split(char *cmd, char **buf)
{
	int cnt = 1;
	int i = 0;
	int bracket = 0;

	// counting words
	while (cmd[i] != '\0' && cmd[i] != '\n')
	{
		if (cmd[i] == '\'' || cmd[i] == '\"')
		{
			if (bracket)
				bracket = 0;
			else
				bracket = 1;
		}

		if (cmd[i] == ' ' && !bracket)
			cnt++;
		i++;
	}

	// reallocate memory for args
	free(buf);
	buf = (char **)malloc((cnt + 1) * sizeof(char *));

	// using strtok for spliting command
	int j = 0;
	i = 0;
	buf[j++] = cmd;
	bracket = 0;
	while (cmd[i] != '\0')
	{
		if (cmd[i] == '\'' || cmd[i] == '\"')
		{
			if (bracket)
				bracket = 0;
			else
				bracket = 1;
		}

		if (cmd[i] == ' ' && !bracket)
		{
			buf[j++] = cmd + i + 1;
			cmd[i] = '\0';
			i++;
		}
		i++;
	}
	buf[j] = NULL;

	return buf;
}

// execute commands with "|"
int pipeCommands(void)
{
	int i = 0, j = 0, cnt = 0;
	//	pipe, temp descriptor,	child process
	int p[2], p_tmp = 0,			proc;
	char ***commands;	//commands to execute

	// counting "|"
	while (args[i] != NULL)
	{
		if (strcmp(args[i], "|") == 0)
			cnt++;
		i++;
	}
	
	// allocating mameory for commands
	commands = (char***)malloc((cnt + 1) * sizeof(char**));
	i = 0;

	commands[0] = args;
	j++;
	while (args[i] != NULL)
	{
		//if argument is "|" replace it with "NULL" to split
		if (strcmp(args[i], "|") == 0)
		{
			args[i] = NULL;
			i++;
			commands[j] = args + i;

			//making pipe btw 1st and 2nd commands
			pipe(p);
			proc = fork();
			if (proc == 0)
			{
				dup2(p_tmp, 0);
				dup2(p[1], 1);
				close(p_tmp);
				close(p[0]);
				close(p[1]);
				execvp(commands[j-1][0], commands[j-1]);
				fprintf(stderr, "Can't run %s\n", commands[j-1][0]);
				exit(3);
			}
			//saving "read" end of pipe
			p_tmp = dup(p[0]);
			close(p[0]);
			close(p[1]);
			j++;
		}
		i++;
	}
	
	//execute last command
	proc = fork();
	if (proc == 0)
	{
		dup2(p_tmp, 0);
		execvp(commands[j-1][0], commands[j-1]);
		fprintf(stderr, "Can't run %s\n", commands[j-1][0]);
		exit(3);
	}

	free(commands);

	return 0;
}

int main()
{
	int err;

	//Handling "Ctrl+C"
	signal(SIGINT, &sig_handler);

	printf("Hello, it's a terminal emulator\n");

	while (1)
	{
		if (getcwd(cwd, MAXNAME) == NULL)
			perror("getcwd() error\n");
		printf("%s$ ", cwd);

		// reading command
		fgets(command, LENGTH, stdin);
		if (command[strlen(command) - 1] == '\n')
			command[strlen(command) - 1] = '\0';
		if (strcmp(command, "exit") == 0)
			break;

		args = (char **)malloc(sizeof(char *));
		args = split(command, args);

		// realizing 'cd' command
		if (strcmp(args[0], "cd") == 0 && args[1] != NULL)
		{
			int err = chdir(args[1]);
			free(args);
			if (err < 0)
				fprintf(stderr, "can't change directory\n");
			continue;
		}
		
		//if command have pipe process this differently
		for (int i = 0; args[i] != NULL; i++)
			if (strcmp(args[i], "|") == 0)
				if ((err = pipeCommands()) == 0)
					break;

		// fork main process for execvp if 1 command
		if (err)
		{
			int p = fork();
			if (p == 0)
			{
				execvp(args[0], args);
				fprintf(stderr, "can't run program \"%s\"\n", args[0]);
				exit(0);
			}
		}
		err = 1;
		free(args);
		// waiting for child processes end otherwise it will corrupt output
		while (wait(NULL) > 0);
	}

	exit(0);
}
