#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "squish_run.h"
#include "squish_tokenize.h"

void inputStream(char *fp){

	int fid;

    fid = open(fp, O_RDONLY);
    dup2(fid, 0);
    close(fid);
}

void outputStream(char *fp){

    int fid;
	
	fid = open(fp, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    dup2(fid, 1);
    close(fid);
}

void final(char **command){

	int status, pid, pides;

	if((pid = fork()) < 0){
		perror("Fork Error\n");
	}
	if ( pid == 0 ) {
		execvp(command[0], command);
	}
	else {
			if( waitpid(pid, NULL, 0) == -1){
				printf("Child(<%d>) did not exit (crashed?)", pid);
			}
			if ( WIFEXITED(status) ) {
				pides = WEXITSTATUS(status);
				if(pides == 0){
					fprintf(stdout, "Child(%d) exited -- success (%d)\n", pid, pides);
				}
				else{
					fprintf(stdout, "Child(%d) exited -- failure (%d)\n", pid, pides);
				}
			}

			
	}
	inputStream("/dev/tty");
	outputStream("/dev/tty");

}

void makePipe(char **command){

	int pipefds[2];

	if((pipe(pipefds)) < 0){
		perror("Piping error\n");
	}
	else{
		close(1);
		dup2(pipefds[1], 1);
		close(pipefds[1]);

		final(command);

		dup2(pipefds[0], 0);
		close(pipefds[0]);
	}
}

/* Print a prompt if the input is coming from a TTY */
static void prompt(FILE *pfp, FILE *ifp){

	if (isatty(fileno(ifp))) {
		fputs(PROMPT_STRING, pfp);
	}
}

/* Actually do the work */
int execFullCommandLine( FILE *ofp, char ** const tokens, int nTokens, int verbosity) {

	int i, j;
	char** command = malloc(1024);

	if (verbosity > 0) {
		fprintf(stderr, " + ");
		fprintfTokens(stderr, tokens, 1);
	}

	i = 0;
	j = 0;

	if(*tokens[0] == '|'){
		goto flag;
	}

	if(strcmp(tokens[0], "cd") == 0 ){
		chdir(tokens[1]);
	}
	
	while(tokens[i]){
		
		if(strcmp(tokens[i], "exit") == 0 ){
			exit(0);
		}
		if(strcmp(tokens[i], "<") == 0 ){
			if(tokens[i+1]){
				inputStream(tokens[i+1]);
			}
			i += 1;
		}
		if(strcmp(tokens[i], ">") == 0 ){
			if(tokens[i+1]){
				outputStream(tokens[i+1]);
			}
			i += 2;
		}
		
		else if(strcmp(tokens[i], "|") == 0 ){

			if(command){
				
				makePipe(command);
				j = 0;

				while(command[j]){
					command[j] = NULL;
					j++;
				}

				j = 0;
			}
			++i;
		}
		else{
			command[j] = tokens[i];
			i++;
			j++;
		}
		
	}

	command[i] = NULL;

	if(command){
		final(command);
	}
	
	flag:
	return 1;
}

/* Load each line and perform the work for it */
int runScript( FILE *ofp, FILE *pfp, FILE *ifp, const char *filename, int verbosity ){
	
	char linebuf[LINEBUFFERSIZE];
	char *tokens[MAXTOKENS];
	int lineNo = 1;
	int nTokens, executeStatus = 0;

	fprintf(stderr, "SHELL PID %ld\n", (long) getpid());

	prompt(pfp, ifp);
	while ((nTokens = parseLine(ifp, tokens, MAXTOKENS, linebuf, LINEBUFFERSIZE, verbosity - 3)) > 0) {
		
		lineNo++;

		if (nTokens > 0) {

			executeStatus = execFullCommandLine(ofp, tokens, nTokens, verbosity);

			if (executeStatus < 0) {
				fprintf(stderr, "Failure executing '%s' line %d:\n    ",
						filename, lineNo);
				fprintfTokens(stderr, tokens, 1);
				return executeStatus;
			}
		}
		prompt(pfp, ifp);
	}

	return (0);
}


/*  Open a file and run it as a script */
int runScriptFile(FILE *ofp, FILE *pfp, const char *filename, int verbosity) {

	FILE *ifp;
	int status;

	ifp = fopen(filename, "r");
	if (ifp == NULL) {
		fprintf(stderr, "Cannot open input script '%s' : %s\n",
				filename, strerror(errno));
		return -1;
	}

	status = runScript(ofp, pfp, ifp, filename, verbosity);
	fclose(ifp);
	return status;
}

