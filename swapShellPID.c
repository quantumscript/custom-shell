/**************************************************
swapShellPID.c
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


int swapShellPID(char* token, char* tmp, char* shellPID) {
	int found = 0;
	// Reset tmp arry with null terminators
	memset(tmp, '\0', 100);
	// Copy token string into tmp array
	strcpy(tmp, token);
	// Ptr1 address of start of token
	char* ptr1 = token;
	// Ptr2 address of start of '$$'
	char* ptr2 = strstr(token, "$$");
	int offset = 0;

	while( ptr2 != NULL ) { // If "$$" is found in token
		// Replace '$$' in token with 'aa'
		*ptr2 = 'a';
		*(ptr2 + 1) = 'a';

		// Found flag is true
		found = 1;

		// Copy shellPID into [ptr2-ptr1] range of tmp array
		strncpy(&tmp[ptr2 - ptr1 + offset], shellPID, strlen(shellPID));

		// Copy any remainder of token into [ptr2 - ptr1 + strlen(shellPID)]  index of array
		int remainingSize = strlen(token) - (ptr2 - ptr1 + 2);
		strncpy(&tmp[ptr2 - ptr1 + strlen(shellPID) + offset], ptr2 + 2, remainingSize);

		// Check if there are more instances of '$$' in the string
		ptr2 = strstr(token, "$$");

		// Index in tmp is offset from token by larger size of shellPID
		offset += strlen(shellPID) - 2;
	}

	return found;
}
