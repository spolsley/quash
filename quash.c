/* quash - quite a shell
 * --------------------
 * Author: Seth Polsley
 * Date: March 17, 2014
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signal.h>

#define BSIZE 256

typedef int bool;
#define true 1
#define false 0

typedef struct {
	int commands;
	int arguments;
	int argumentLength;
	bool background;
	bool toFile;
	bool fromFile;
	char* data;
} Command;

typedef struct {
	bool active;
	int jid;
	pid_t pid;
	char* command;
} Job;

char getRef(Command com, int x, int y, int z)
{
	return com.data[x * (com.arguments * com.argumentLength) + y * com.argumentLength + z];
}

char * getString(Command com, int x, int y)
{
	char *new_arr = malloc(com.argumentLength * sizeof *com.data);
	int start = x * (com.arguments * com.argumentLength) + y * com.argumentLength;
	int end = start + com.argumentLength;
	for (int i = 0, j = start; j < end; i++, j++)
	{
		new_arr[i] = com.data[j];
		if (com.data[j] == '\0')
			break;
	}
	return new_arr;
}

void setRef(Command com, int x, int y, int z, char value)
{
	com.data[x * (com.arguments * com.argumentLength) + y * com.argumentLength + z] = value;
	return;
}

Command parseCommand(char command[], int length);
char * getDirectory();

int main(int argc, char **argv, char **envp)
{
	bool reading;
	if (isatty(fileno(stdin)))
		reading = false;
	else
		reading = true;
	if (!reading)
	{
		printf("###################################################\n");
		printf("#  ______   __  __   ______   ______   __  __     #\n");
		printf("# /\\  __ \\ /\\ \\/\\ \\ /\\  __ \\ /\\  ___\\ /\\ \\_\\ \\    #\n");
		printf("# \\ \\ \\/\\_\\\\ \\ \\_\\ \\\\ \\  __ \\\\ \\___  \\\\ \\  __ \\   #\n");
		printf("#  \\ \\___\\_\\\\ \\_____\\\\ \\_\\ \\_\\\\/\\_____\\\\ \\_\\ \\_\\  #\n");
		printf("#   \\/___/_/ \\/_____/ \\/_/\\/_/ \\/_____/ \\/_/\\/_/  #\n");
		printf("#                                                 #\n");
		printf("#               (qu)ite (a) (sh)ell               #\n");
		printf("#   A simple shell written in C by Seth Polsley   #\n");
		printf("###################################################\n");
	}
	char *user = getenv("USER");
	char hostname[BSIZE];
	gethostname(hostname,BSIZE);
	char *directory = getDirectory();
	char command[BSIZE];
	size_t length = BSIZE;
	Job jobs[BSIZE];
	for (int l = 0; l < BSIZE; l++)
	{
		jobs[l].active = false;
		jobs[l].command = malloc(10);
	}
	int numJobs = 0;
	pid_t return_pid;
	int pid;
	int status;
	int pipes = 0;
	int *pipefds;
	int args = 0;
	while (1)
	{
		if (!reading)
			printf("%s:%s %s$ ",hostname,directory,user);
		char *command = malloc((int)length);
		if (getline(&command,&length,stdin) > 0)
		{
			for (int l = 0; l < BSIZE; l++)
			{
				if (jobs[l].active)
				{
					return_pid = waitpid(jobs[l].pid,&status, WNOHANG);
					if (return_pid < 0)
					{
						fprintf(stderr,"[%d] %d failed with error %s\n",jobs[l].jid,jobs[l].pid,strerror(errno));
						jobs[l].active = false;
					}
					else if (return_pid == jobs[l].pid)
					{
						printf("[%d] %d finished %s\n",jobs[l].jid,jobs[l].pid,jobs[l].command);
						jobs[l].active = false;
					}
					else
						continue;
				}
			}
			Command com = parseCommand(command,length);
			if (com.commands > 1)
			{
				pipes = (com.commands - 1) * 2;
				pipefds = malloc(pipes);
				if ((com.toFile)||(com.fromFile))
				{
					char *filename = getString(com,1,0);
					FILE *stream;
					pipe(pipefds);
					if (com.toFile)
					{
						stream = fopen(filename, "w");
						pipefds[1] = fileno(stream);
					}
					else
					{
						stream = fopen(filename, "r");
						pipefds[0] = fileno(stream);
					}
					com.commands = 1;
					free(filename);
				}
				else
				{	
					for (int k = 0; k < pipes; k = k + 2)
						pipe(&pipefds[k]);
				}
			}
			for (int i = 0; i < com.commands; i++)
			{
				char **commandArr = malloc(com.arguments + 1);
				for (int j = 0; j < com.arguments; j++)
				{
					commandArr[j] = malloc(com.argumentLength);
					commandArr[j] = getString(com,i,j);
					if (commandArr[j][0] == '$') //Replace variable with value
					{
						char *var = getenv(commandArr[j] + 1);
						commandArr[j] = realloc(commandArr[j],strlen(var));
						strcpy(commandArr[j],var);
					}
					if (commandArr[j][0] == '\0')
						free(commandArr[j]);
					else
						args = j + 1;
				}
				if (args < com.arguments)
				{
					commandArr = realloc(commandArr,args + 1);
					commandArr[args] = (char *) 0;
				}
				else
					commandArr[com.arguments] = (char *) 0;
				if ((strcmp("exit",commandArr[0]) == 0)||(strcmp("quit",commandArr[0]) == 0))
				{
					for (int j = 0; j < com.arguments; j++)
						free(commandArr[j]);
					free(commandArr);
					free(com.data);
					free(command);
					printf("\nlogout\n");
					return 0;
				}
				else if (strcmp("set",commandArr[0]) == 0)
				{
					if (com.arguments < 2)
					{
						fprintf(stderr, "\nquash: set: Usage set VAR=VALUE\n");
					}
					else
					{
						int separator = -1;
						for (int k = 0; k < strlen(commandArr[1]); k++)
						{
							if (commandArr[1][k] == '=')
							{
								separator = k;
								break;
							}
						}
						if (separator == -1)
						{
							if (com.arguments > 3)
								setenv(commandArr[1],commandArr[3],1);
							else
								fprintf(stderr, "\nquash: set: Unable to set %s\n", commandArr[1]);
						}
						else
						{
							char *name = malloc(separator + 1);
							char *value = malloc(strlen(commandArr[1]) - separator);
							memcpy(name,commandArr[1],separator);
							name[separator] = '\0';
							memcpy(value,commandArr[1] + separator + 1,strlen(commandArr[1]) - separator);
							setenv(name,value,1);
							free(name);
							free(value);
						}
					}
					for (int j = 0; j < com.arguments; j++)
						free(commandArr[j]);
					free(commandArr);
					free(com.data);
				}
				else if (strcmp("get",commandArr[0]) == 0)
				{
					printf("%s\n",getenv(commandArr[1]));
					for (int j = 0; j < com.arguments; j++)
						free(commandArr[j]);
					free(commandArr);
					free(com.data);
				}
				else if (strcmp("cd",commandArr[0]) == 0)
				{
					if (com.arguments < 2)
					{
						chdir(getenv("HOME"));
					}
					else
					{
						if (commandArr[1][0] == '~')
						{
							char *dir = getenv("HOME");
							commandArr[1] = realloc(commandArr[1],strlen(commandArr[1])-1+strlen(dir));
							memcpy(commandArr[1]+strlen(dir),commandArr[1]+1,strlen(commandArr[1])-1);
							memcpy(commandArr[1],dir,strlen(dir));
						}
						if (chdir(commandArr[1]) < 0)
						{
							fprintf(stderr, "\nquash: cd: No such file or directory\n");
						}
					}
					char cwd[BSIZE];
					getcwd(cwd,BSIZE);
					setenv("PWD",cwd,1);
					directory = getDirectory();
				}
				else if (strcmp("jobs",commandArr[0]) == 0)
				{
					printf("[JOBID]\tPID\tCOMMAND\n");
					for (int l = 0; l < BSIZE; l++)
					{
						if (jobs[l].active)
						{
							printf("[%d]\t%d\t%s\n",jobs[l].jid,jobs[l].pid,jobs[l].command);
						}
					}
				}
				else if (strcmp("kill",commandArr[0]) == 0)
				{
					if (args < 3) //Assume that the argument is pid if there is only one
						kill((pid_t)atoi(commandArr[1]),SIGTERM);
					else
						kill((pid_t)atoi(commandArr[2]),atoi(commandArr[1]));
				}
				else
				{
					pid = fork();
					if (pid == 0) {
						if (com.commands > 1)
						{
							if (i == 0)
								dup2(pipefds[1], fileno(stdout));
							else if ((i > 0) && (i < com.commands - 1))
							{
								dup2(pipefds[(i+1)*2-4], fileno(stdin));
								dup2(pipefds[(i+1)*2-1], fileno(stdout));
							}
							else
								dup2(pipefds[(i+1)*2-4], fileno(stdin));
							for (int j = 0; j < pipes; j++)
								close(pipefds[j]);
						}
						if (com.toFile)
						{
							dup2(pipefds[1], fileno(stdout));
							close(pipefds[0]); close(pipefds[1]);
						}
						if (com.fromFile)
						{
							dup2(pipefds[0], fileno(stdin));
							close(pipefds[0]); close(pipefds[1]);
						}
						if (((i+1) == com.commands) && (com.background))
						{
							int p[2];
							pipe(p);
							dup2(p[1],fileno(stdout));
						}
						if( (execvp(commandArr[0], commandArr) < 0) )
						{
							fprintf(stderr, "\nquash: %s: %s\n", commandArr[0], strerror(errno));
							return EXIT_FAILURE;
						}
					}
					else {
						if ((com.commands > 1)&&((i+1) != com.commands))
							continue;
						else
						{
							if (com.commands > 1)
							{
								for (int j = 0; j < pipes; j++)
									close(pipefds[j]);
							}
							if ((com.toFile)||(com.fromFile))
							{
								close(pipefds[0]); close(pipefds[1]);
							}
							if (!com.background)
							{				
								waitpid(pid,&status,0);
							}
							else
							{
								printf("[%d] %d running in background\n",numJobs,pid);
								for (int l = 0; l < BSIZE; l++)
								{
									if (!jobs[l].active)
									{
										jobs[l].jid = numJobs;
										jobs[l].pid = pid;
										jobs[l].command = realloc(jobs[l].command,strlen(commandArr[0]));
										strcpy(jobs[l].command,commandArr[0]);
										jobs[l].active = true;
										numJobs++;
										break;
									}
								}
							}
						}
						for (int j = 0; j < args; j++)
							free(commandArr[j]);
						free(commandArr);
					}
					free(com.data);
				}
			}
			if (com.commands > 1)
				free(pipefds);
		}
		else
		{
			free(command);
			break;
		}
		free(command);
	}
	return 0;
}

Command parseCommand(char command[], int length)
{
	bool lastSpace = false;
	bool seenLetter = false;
	bool background = false;
	bool toFile = false;
	bool fromFile = false;
	int arguments = 0;
	int argumentLength = 0;
	int commands = 0;
	for (int i = 0, arg = 0, argLen = 0; i < length; i++)
	{
		if ((command[i] == ' ')||(command[i] == '\t')||(command[i] == '\n'))
		{
			argLen = 0;
			if ((command[i] == '\n') && !seenLetter)
				arg = 0;
			if (!lastSpace && seenLetter)
			{
				lastSpace = true;
				arg++;
				if (arg > arguments)
					arguments = arg;
			}
		}
		else if (command[i] == '&')
		{
			background = true;
		}
		else if ((command[i] == '|')||(command[i] == '<')||(command[i] == '>'))
		{
			commands++;
			arg = 0;
			argLen = 0;
			seenLetter = false;
			background = false;
			if (command[i] == '<') fromFile = true;
			else if (command[i] == '>') toFile = true;
		}
		else if (command[i] == '\0')
		{
			break;
		}
		else
		{
			lastSpace = false;
			seenLetter = true;
			background = false;
			argLen++;
			if (argLen > argumentLength)
				argumentLength = argLen;
		}
	}
	if ((commands == 0)&&(arguments == 0)&&(argumentLength == 0))
	{
		Command com = {0,0,0};
		return com;
	}
	Command com = {commands + 1, arguments, argumentLength + 1};
	com.background = background;
	com.toFile = toFile;
	com.fromFile = fromFile;
	com.data = malloc(com.commands * com.arguments * com.argumentLength * sizeof *com.data);
	for (int i = 0, comnum = 0, argnum = 0, arglennum = 0; i < length; i++)
	{
		if ((command[i] == ' ')||(command[i] == '\t')||(command[i] == '\n'))
		{
			if (!lastSpace && seenLetter)
			{
				for( ; arglennum < com.argumentLength; arglennum++)
					setRef(com, comnum, argnum, arglennum, '\0');
				lastSpace = true;
				argnum++;
				arglennum = 0;
			}
		}
		else if (command[i] == '&')
		{
			continue;
		}
		else if ((command[i] == '|')||(command[i] == '<')||(command[i] == '>'))
		{
			if (!lastSpace && seenLetter)
			{
				for( ; arglennum < com.argumentLength; arglennum++)
					setRef(com, comnum, argnum, arglennum, '\0');
				lastSpace = true;
				argnum++;
				arglennum = 0;
			}
			comnum++;
			argnum = 0;
			seenLetter = false;
		}
		else if (command[i] == '\0')
		{
			break;
		}
		else
		{
			lastSpace = false;
			seenLetter = true;
			setRef(com, comnum, argnum, arglennum, command[i]);
			arglennum++;
		}
	}
	return com;
}

char * getDirectory()
{
	char cwd[BSIZE];
	char *result = malloc(BSIZE);
	getcwd(cwd,BSIZE);
	int lastSlash = 0;
	for (int i = 0; i < BSIZE; i++)
	{
		if (cwd[i] == '/')
			lastSlash = i;
	}
	strcpy(result,cwd + lastSlash + 1);
	return result;
}
