#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#define MAX_LENGTH 2048
#define MAX_ARG 513

struct child{
    int spawnPid;
    int childStatus;
    struct child *next;
};

struct child *head = NULL;
struct child *tail = NULL;

// creats struct of child processes.
struct child *addNode(int spawnPid, int childStatus){
    
    struct child *currNode = malloc(sizeof(struct child));
    currNode->spawnPid = spawnPid;
    currNode->childStatus = childStatus;
    currNode->next = NULL;

    if ( head==NULL ){
        head = currNode;
        tail = currNode;
    } else {
        tail->next = currNode;
        tail = currNode;
    }

    return head;
}

void getStatus(int childPid, int childStatus){
    // Prints exit status of child.
    if(WIFEXITED(childStatus)){
			fprintf(stdout, "exit value %d\n", WEXITSTATUS(childStatus));
            fflush(stdout);
    } else {
			fprintf(stdout, "exit value %d\n", WTERMSIG(childStatus));
            fflush(stdout);
    }
}

int * otherCom( char *usrStr, struct child *head ){
    // Function handles all non-built in functions.

    char *commArr[MAX_ARG] = {NULL};
    char *saveptr;
    int args = 0;
    int childStatus;
    static  int childArr[2];
    bool childBg = false;
    
    char *infile = 0x0;
    char *outfile = 0x0;

    // Tokenizes the user supplied command.
    char *token = strtok_r(usrStr, " ", &saveptr);
    while ( token != NULL ){
        commArr[args] = token;
        token = strtok_r(NULL, " ", &saveptr);
        args++;    
    }
    strtok(commArr[args-1], "\n");

    // Determines if command should be run in background
    if ( strncmp(commArr[args-1], "&", 1)==0 ){
        childBg = true;
        commArr[args-1] = 0x0;
        args--;
    }

    // Handles $$ variable expansion.
    for ( int i = 0; i < args; i++ ){
        for ( int j=0; j < strlen(commArr[i]); j++){
            if ( strncmp(&commArr[i][j], "$", 1)==0 && strncmp(&commArr[i][j+1], "$", 1)==0 ){
                int ppid = getppid();
                char numStr[20];
                sprintf(numStr, "%d", ppid);
                strtok(commArr[i], "$");
                commArr[i] = strcat(commArr[i], numStr);
            }
        }
    }

    char *tempArr[MAX_ARG] = {NULL};
    // If user requests I/O redirection, saves infile, outfile
    for (int j = 0; j < args; j++){
        if ( strncmp("<", commArr[j], 1)==0 ){
            infile = calloc( strlen(commArr[j+1]+1), sizeof(char) );
            strcpy(infile, commArr[j+1]);
        }
        if ( strncmp(">", commArr[j], 1)==0 ){
            outfile = calloc( strlen(commArr[j+1])+1, sizeof(char) );
            strcpy(outfile, commArr[j+1]);
        }
    }
    
    // removes infile, outfile, "<", and ">" from commArr
    for (int k = 0; k < args; k++){
        if( strcmp(commArr[k], "<")==0 ){
            break;
        }
        if( strcmp(commArr[k], ">")==0 ){
            break;
        }
        tempArr[k] = commArr[k];
    }

    if(tempArr[0] != NULL){
        memcpy( commArr, tempArr, sizeof(commArr)/sizeof(commArr[0]) );
    }

    // Saves terminal stdout
    int saved_stdout = dup(1);

    // Redirects stdin
    if (infile != NULL){
        int sourceFD = open(infile, O_RDONLY);
        if (sourceFD == -1){
            perror("source open()");
            return 0;
        }
        int result = dup2(sourceFD, 0);
        if (result == -1){
            perror("source dup2()");
            return 0;
        }
    }

    // Redirects stdout
    if (outfile != NULL){
        int targetFD = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if (targetFD == -1){
            perror("target open()");
            return 0;
        }
        int result = dup2(targetFD, 1);
        if (result == -1){
            perror("target dup2()");
            return 0;
        }
    }

    // forks child
    pid_t spawnPid = fork();
    switch(spawnPid){
        case -1:
        // Error handling
            perror("fork()\n");
            exit(1);
            break;
        case 0: 
            // Executes child command
            execvp(commArr[0], commArr);
            perror("execve");   
            exit(2);  
            break;
        default:
            // parent process
            if ( childBg == false ) {
                waitpid(spawnPid, &childStatus, 0);
            } else {
                // child as background process
                //redirects stdin to /dev/null if not specified
                 if (infile != NULL){
                    int sourceFD = open("/dev/null", O_RDONLY);
                    if (sourceFD == -1){
                        perror("source open()");
                        return 0;
                    }
                    int result = dup2(sourceFD, 0);
                    if (result == -1){
                        perror("source dup2()");
                        return 0;
                    }
                }

                // Redirects stdout to /dev/null if not specified
                if (outfile != NULL){
                    int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    if (targetFD == -1){
                        perror("target open()");
                        return 0;
                    }
                    int result = dup2(targetFD, 1);
                    if (result == -1){
                        perror("target dup2()");
                        return 0;
                    }
                }

                waitpid(spawnPid, &childStatus, WNOHANG);
                fprintf(stdout, "background pid is %d\n", spawnPid);
                fflush(stdout);
            }
            break;
    
        //getStatus(spawnPid, childStatus);
        head = addNode(spawnPid, childStatus); 
    }

    struct child *node = NULL;
    node = head;
    if ( childBg == true ){
        while (node != NULL){
            if (waitpid(node->spawnPid, &childStatus, WNOHANG) != 0){
                if (WIFEXITED(childStatus)){
                    fprintf(stdout, "background pid %d is done: terminated by signal %d", head->spawnPid, head->childStatus);
                    fflush(stdout);
                }
            }
            node = node->next;
        }
    }
    
    dup2(saved_stdout, 1);
    close(saved_stdout);
    return childArr;
}

void changeDir( char *usrStr ){
    
    char *saveptr;

    // Tokenizes user string
    char *token = strtok_r(usrStr, " ", &saveptr);
    token = strtok_r(NULL, " ", &saveptr);

    if (token != 0x0){
        // if user specifies a directory, saves directory name and executes chdir command.
        char *directory = calloc(strlen(token), sizeof(char));
        strcpy(directory, token);
        directory[strcspn(directory, "\n")] = 0; // cuts off newline char from directory string. 
        chdir(directory);
    } else {
        // if user does not specify directory change directory to HOME environment variable.
        chdir( getenv("HOME") );
    }
}

int main(){
    bool loop = true;
    char usrStr[MAX_LENGTH];
    int *childStatus = NULL;
    struct child *head = NULL;

    // Parent process ignores SIGINT
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);


    while ( loop == true ){

        // Gets user input
        fprintf(stdout, ": ");
        fflush(stdout);
        fgets(usrStr, MAX_LENGTH, stdin);

        if ( strncmp("#", usrStr, 1) == 0 || strncmp("\n", usrStr, 1) == 0 ){        // ignores lines with preceding '#' char as comments
            continue;                                                                   
        } else if ( strncmp("exit", usrStr, 4) == 0 ){      // exits shell
            loop = false;
        } else if ( strncmp("cd", usrStr, 2) == 0 ){        // changes directory 
            changeDir(usrStr);
        } else if ( strncmp("status", usrStr, 6) == 0 ){    // gets exit status
            if ( childStatus == NULL ){
                fprintf(stdout, "exit value 0\n");
                fflush(stdout);
            } else {
                getStatus(childStatus[0], childStatus[1]);
            }
        } else { 
            childStatus = otherCom(usrStr, head);           // exec() for all other commands
        }
    }
    return 0;
}