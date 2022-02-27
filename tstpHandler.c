/**************************************************
tstpHandler.c
Input: char* token, char* temp, char* shellPID
Process: Replaces all instances of '$$' in a token string
Output: int found, 1 if '$$' found
**************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include "functions.h"

int tstpActive;
int flagFgOnly;

void tstpHandler(int signo) {
	static int callHandler = 0;
	char* message;
	callHandler++;
	tstpActive = 1;
	// Enter foreground only mode on odd calls
	if ( callHandler % 2 == 1 ) {
		flagFgOnly = 1;
		message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 50);
	}

	// Enter background only mode on even calls
	else {
		flagFgOnly = 0;
		message = "\nExiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 31);
	}
}
