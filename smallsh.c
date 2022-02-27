/********************************************************************
Class: CS 344 - Operating Systems
Program: Program 3 - smallsh.c
Author: KC
Date: May 15 2018
Description: Create a shell with a prompt, built in commands (exit, cd,
status), command execution using child processes, redirection of stdin
and stdout, ability to run foreground and background processes, and signal
handling (CTRL-C and CTRL-Z)
**********************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include "functions.h"

#define MAX_CHARS 2048
#define MAX_ARGS 100
#define SIZE_PROC_ARR 10

// Global vars for handling signals. sigaction struct requires ptr to func with 1 int arg
extern int tstpActive;
extern int flagFgOnly;


int main() {

	// Define global vars
	tstpActive = 0;
	flagFgOnly = 0;

	// Shell variables
	int continuePrompt = 1;
	int flagBlankLine = 0;
	int flagCommentLine = 0;
	int flagBg = 0;

	// Token array for processing getline output into pieces
	char* tokenArr[MAX_ARGS * sizeof(char*)];
	memset(tokenArr, '\0', MAX_ARGS * sizeof(char*));

	// Flags
	int exitStatus = -5; // Exit status value will be 0 if exited normally
	int termSig = -4; // Terminating signal value > 0 if terminated

	// Get shell process ID in charPtrShellPID
	char shellPID[7]; memset(shellPID, '\0', 7);
	sprintf(shellPID, "%ld", (long)getpid());

	// Background process array of PID values - check for completed processes
	pid_t bgProcArr[SIZE_PROC_ARR];
	int a;
	for (a=0; a< SIZE_PROC_ARR; a++) {
		bgProcArr[a] = -3;
	}

	/************** SIGNAL HANDLERS *****************/
	struct sigaction ignore_signal = {0};
	struct sigaction default_signal = {0};
	struct sigaction tstp_signal = {0};

	ignore_signal.sa_handler = SIG_IGN; // Ignore signal
	default_signal.sa_handler = SIG_DFL; // Default response
	tstp_signal.sa_handler = tstpHandler;

	ignore_signal.sa_flags = 0;
	default_signal.sa_flags = 0;
	tstp_signal.sa_flags = 0;

	// Shell and background processes ignore SIGINT, foreground processes change
	//to defult_signal after fork() and before exec()
	sigaction(SIGINT, &ignore_signal, NULL);
	// Shell processes SIGTSTP, foreground and background processes change to
	//ignore_signal after fork() and before exec()
	sigaction(SIGTSTP, &tstp_signal, NULL);

	/************** SHELL INTERFACE *****************/
	do {
		// Check if any background processes have completed
		int childExitMethod = -5;
		pid_t childPID;
		int b;

		// Garbage collector for malloc'd tmp arrays
		char* garbage[MAX_ARGS];
		memset(garbage, '\0', MAX_ARGS);

		for (b=0; b<SIZE_PROC_ARR; b++) {
			childPID = waitpid(bgProcArr[b], &childExitMethod, WNOHANG);
			// Print pid of completed bg process in array and exit status if exited normally
			if ( childPID == bgProcArr[b] && WIFEXITED(childExitMethod) != 0) {
				printf("background pid %i is done: exit status %d\n", childPID, WEXITSTATUS(childExitMethod));
				fflush(stdout);
				bgProcArr[b] = -3;
			}
			// Print pid of completed bg process in arry and terminating signal if terminated by a signal
			if ( childPID == bgProcArr[b] && WIFEXITED(childExitMethod) == 0) {
				printf("background pid %i is done: terminated by signal %d\n", childPID, WTERMSIG(childExitMethod));
				fflush(stdout);
				bgProcArr[b] = -3;
			}
		}

		// Prompt the user for input
		tstpActive = 0;
		printf(": "); fflush(stdout);
		clearerr(stdin); // Must include to remove error flag with TSTP signal interrupts

		// Working on getting input from user
		int numCharsEntered = -1;
		char* lineInput = NULL;		//to NULL tells getline to allocate a buffer with malloc
		size_t bufferSize = 0; // Setting input size = 0 tells getline to allocate buffer with malloc
		numCharsEntered = getline(&lineInput, &bufferSize, stdin); // returns -1 if no character read

		// Implement command line limit restriction
		if ( numCharsEntered > MAX_CHARS ) {
			printf("Error: Command line limit is %d.", MAX_CHARS);
			fflush(stdout);
		}

		// If command line char limit is OK
		else {
			// Blank line - No text entered, getline returns -1, 1 char entered if enter
			if (numCharsEntered == -1 || numCharsEntered == 1) {
				flagBlankLine = 1;
			}

			// Text entered
			else {
				flagBlankLine = 0;
				lineInput[ numCharsEntered - 1 ] = '\0'; // Remove newline that getline adds to end of line
			}

			// If line isn't blank, get first token, see if it's a comment
			memset(tokenArr, '\0', MAX_ARGS * sizeof(char*));

			if ( flagBlankLine == 0 && tstpActive == 0 ) {
				tokenArr[0] = strtok(lineInput, " ");

				if ( strstr(tokenArr[0], "#") != NULL ) flagCommentLine = 1;
				else flagCommentLine = 0;
			}

			// If NOT blank line AND NOT comment, get the rest of the tokens
			int numTokens = 0;
			if ( flagBlankLine == 0 && flagCommentLine == 0 && tstpActive == 0 ) {
				char* token = strtok(NULL, " ");
				int i = 1; // Index of potential second token
				int found = -1;

				while ( token != NULL ) {
					// Temporary array for replacing '$$' with shellPID
					char* tmp = malloc(sizeof (char) * 100);
					garbage[i-1] = tmp;

					// Token contains $$, swap with PID using tmp array
				 	found = swapShellPID(token, tmp, shellPID);
					if (found) tokenArr[i] = tmp;

					// Token does not contain $$
					else tokenArr[i] = token;

					// Get next token from strtok
					token = strtok(NULL, " ");
					i++;
				}
				numTokens = i;
			}

			// If NOT blank line AND NOT comment
			// Process the token and find where < > and & are if present
			// & must be the last word otherwise treat it as a char
			int nArgs = 0; // Includes command
			char* inputFile = NULL;
			char* outputFile = NULL;
			if ( flagBlankLine == 0 && flagCommentLine == 0 && tstpActive == 0 ) {
				for (int i=0; i < numTokens; i++) {
					// If find <, next token is inputFile
					if ( strcmp(tokenArr[i],"<") == 0 ) {
						inputFile = tokenArr[i+1];
						i++;	// skips over reading the file string
				}

					else if ( strcmp(tokenArr[i],">") == 0 ) {
						outputFile = tokenArr[i+1];
						i++; // skips over reading the file string
				}
					// matches "&" token, is the last token, and the exclusivly foreground flag is off
					else if ( strcmp(tokenArr[i], "&") == 0 && i == numTokens - 1 ) {
						// Need to include so '&' token
						//doesn't increment nArgs when flagFgOnly is on
						if ( flagFgOnly == 0 ) flagBg = 1;
					}

					else nArgs++;
				}
			}

			/***************** Execute commands  ****************/
			if ( flagBlankLine == 0 && flagCommentLine == 0 && tstpActive == 0 ) {
				char** argArr = malloc (sizeof(char*) * (nArgs + 1)); // + 1 for NULL as last element

				// ***** EXIT - BUILT-IN COMMAND ***** exit shell, kill all other processes
				if ( strncmp( tokenArr[0], "exit", 4 ) == 0 ) {
					int i=0;
					while (i < SIZE_PROC_ARR && bgProcArr[i] != -3) {
						kill(bgProcArr[i], SIGTERM);
						i++;
					}

					// Free dynamic memory
					free(argArr);
					argArr = NULL;

					exit(0);
				}

				// ***** STATUS - BUILT-IN COMMAND ***** prints out exit status or the terminating
				// signal of the last foreground process run by the shell
				else if ( strncmp( tokenArr[0], "status", 6 ) == 0 ) {
					flagBg = 0;		// Ignore any background command

					// Exited normally - print exitStatus
					if ( exitStatus >= 0 ) printf("exit value %d\n", exitStatus);

					// Killed by signal - print terminating signal
					else if ( termSig >= 0 ) printf("terminated by signal %d\n", termSig);

					// Status is the first command - print exit value fine
					else printf("exit value fine\n");

					fflush(stdout);
				}

				/***** CD - BUILT-IN COMMAND ***** changes directory */
				else if ( strncmp( tokenArr[0], "cd", 2 ) == 0 ) {
					flagBg = 0;		// Ignore any background command

					// No args, changes directory to HOME env variable
					if ( numTokens == 1 ) {
						char* homeDir = getenv("HOME");
						chdir(homeDir);
					}

 					// With 1 arg, changes directory to that path, which can be relative or absolute
					else chdir(tokenArr[1]);
				}

				/* ***** ALL OTHER COMMANDS ****** fork() and exec() child process*/
				else {
					// Create execvp() argument array from tokenArr[]
					for (int a=0; a < nArgs; a++) {
						argArr[a] = tokenArr[a];
					}
					argArr[nArgs] = NULL;

					pid_t spawnPID = -5;
					int childExitStatus = -5;

					spawnPID = fork();
					switch (spawnPID) {
						// ERROR FORKING TO CHILD PROCESS
						case -1: perror("Error forking to child process.\n"); exit(1); break;

						// CHILD PROCESS
						case 0: {
							// Ignore SIGTSTP for all child processes
							sigaction(SIGTSTP, &ignore_signal, NULL);

							// BACKGROUND CHILD PROCESS HANDLING
							if ( flagBg == 1 ) {
								int devNull = open("/dev/null", O_WRONLY);
								// Redirect stdin to /dev/null
								if ( dup2(devNull, 0) == -1 ) {
										perror("Error redirecting stdin to /dev/null in dup2()\n");
								}
								// Redirect stdout to /dev/null
								if ( dup2(devNull, 1) == -1 ) {
										perror("Error redirecting stdout to /dev/null in dup2()\n");
								}
							}

							// FOREGROUND CHILD PROCESS HANDLING
							else {
								// Set handling of SIGINT to default. Terminates foreground process
								sigaction(SIGINT, &default_signal, NULL);

								// Redirect stdin to inputFile for foreground
								if ( inputFile != NULL) {
									int targetFD = open(inputFile, O_RDONLY);
									if (targetFD == -1) { perror("open() for RDONLY failed"); exit(1); break; }
									else  dup2(targetFD, 0);
								}

								// Redirect stdout to outputFile for foreground
								if ( outputFile != NULL) {
									int targetFD = open(outputFile, O_WRONLY | O_TRUNC | O_CREAT, 0600);
									if (targetFD == -1) { perror("open() for WRONLY failed"); exit(1); break; }
									else  dup2(targetFD, 1);
								}
							}

							execvp(argArr[0], argArr);
							perror("CHILD exec() process failed!\n");
							exit(1); break;
						}

						// PARENT PROCESS
						default: {
							// printf("I'm the parent process with PID: %d\n", getpid());
							if ( flagBg == 0 ) { // Wait for foreground child process to terminate
								waitpid(spawnPID, &childExitStatus, 0);

								// If child exited normally
								if (WIFEXITED(childExitStatus) != 0) exitStatus = WEXITSTATUS(childExitStatus);

								// Otherwise child terminated by signal
								else termSig = WTERMSIG(childExitStatus);
							}

							else { // Don't wait for background child process to terminate
								int i=0;
								while (i < SIZE_PROC_ARR && bgProcArr[i] != -3) i++;
								bgProcArr[i] = spawnPID;
								printf("background pid is %i\n", bgProcArr[i]); fflush(stdout);
								flagBg = 0;
							}

							break;
						} // parent process
					} // switch(spawnPID)
				} // else for all other commands
			} // execute commands
		} // command line char limit OK

		// Free dynamic memory
		free(lineInput);
		int i = 0;
		while (garbage[i] != NULL) {
			free(garbage[i]);
			i++;
		}

	} while ( continuePrompt );
	return 0;
}
