#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define CMD_LEN 512
#define PROMPT "@> "

int main(int argc, char **argv) {
    // Task 4: Set up shell to ignore SIGTTIN, SIGTTOU when put in background
    // You should adapt this code for use in run_command().
    struct sigaction sac;
    sac.sa_handler = SIG_IGN;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    strvec_t tokens;
    strvec_init(&tokens);
    job_list_t jobs;
    job_list_init(&jobs);
    char cmd[CMD_LEN];

    printf("%s", PROMPT);
    while (fgets(cmd, CMD_LEN, stdin) != NULL) {
        // Need to remove trailing '\n' from cmd. There are fancier ways.
        int i = 0;
        while (cmd[i] != '\n') {
            i++;
        }
        cmd[i] = '\0';

        if (tokenize(cmd, &tokens) != 0) {
            printf("Failed to parse command\n");
            strvec_clear(&tokens);
            job_list_free(&jobs);
            return 1;
        }
        if (tokens.length == 0) {
            printf("%s", PROMPT);
            continue;
        }
        const char *first_token = strvec_get(&tokens, 0);
        if (strcmp(first_token, "pwd") == 0) {
            // TODO Task 1: Print the shell's current working directory
            // Use the getcwd() system call
            char buffer[CMD_LEN];
            if(getcwd(buffer, CMD_LEN)==NULL){
                //I was told this was the best way to error check getcwd
                perror("pwd failed, buffer set to NULL");
            }
            else{
                printf("%s \n", buffer);
            }
        }

        else if (strcmp(first_token, "cd") == 0) {
            //Error checking was confirmed by TA
            if(strvec_get(&tokens, 1)!=NULL){//If there is a second argument, go into that folder
                if(chdir(strvec_get(&tokens, 1))==-1){
                    perror("chdir");
                }
            }
            else{//otherwise go to home
                if(chdir(getenv("HOME"))==-1){
                    
                    perror("chdir");
                }
            }
        }

        else if (strcmp(first_token, "exit") == 0) {
            strvec_clear(&tokens);
            break;
        }

        // Task 5: Print out current list of pending jobs
        else if (strcmp(first_token, "jobs") == 0) {
            int i = 0;
            job_t *current = jobs.head;
            while (current != NULL) {
                char *status_desc;
                if (current->status == JOB_BACKGROUND) {
                    status_desc = "background";
                } else {
                    status_desc = "stopped";
                }
                printf("%d: %s (%s)\n", i, current->name, status_desc);
                i++;
                current = current->next;
            }
        }

        // Task 5: Move stopped job into foreground
        else if (strcmp(first_token, "fg") == 0) {
            if (resume_job(&tokens, &jobs, 1) == -1) {
                printf("Failed to resume job in foreground\n");
            }
        }

        // Task 6: Move stopped job into background
        else if (strcmp(first_token, "bg") == 0) {
            if (resume_job(&tokens, &jobs, 0) == -1) {
                printf("Failed to resume job in background\n");
            }
        }

        // Task 6: Wait for a specific job identified by its index in job list
        else if (strcmp(first_token, "wait-for") == 0) {
            if (await_background_job(&tokens, &jobs) == -1) {
                printf("Failed to wait for background job\n");
            }
        }

        // Task 6: Wait for all background jobs
        else if (strcmp(first_token, "wait-all") == 0) {
            if (await_all_background_jobs(&jobs) == -1) {
                printf("Failed to wait for all background jobs\n");
            }
        }

        else {
            int parallel = 0;
            if(strcmp(strvec_get(&tokens,tokens.length-1),"&")==0){//If last token is & set parallel to 1
                parallel=1;
            }
            pid_t pidval = fork();//fork child, remember pidval
            int status; //status, to be used later
            if(pidval==0){//Child runs command
                if(run_command(&tokens)==-1){
                    //error messages are in run_command
                    return -1;
                }
            }
            else{//Parent Continues
                if(parallel==0){ //If not in parallel
                    //The following chaining else if statements are so that if an error occurs beforehand, it does not keep trying to run
                    //commands and give the user like 6 errors in a row. 
                    if(tcsetpgrp(STDIN_FILENO, pidval)==-1){//pull child to foreground
                        perror("tcsetpgrp");
                    }
                    else if(waitpid(pidval, &status, WUNTRACED)==-1){//Wait, while keeping WUNTRACED flag, for future use (Stopped or terminated)
                        perror("waitpid");
                    }
                    else if(tcsetpgrp(STDIN_FILENO, getpid())==-1){//Return Shell to foreground
                        perror("tcsetpgrp");
                    }
                    else if(WIFSTOPPED(status)){//If stopped, add to job lists as stopped
                        job_list_add(&jobs, pidval, strvec_get(&tokens, 0),JOB_STOPPED);
                    }
                }
                else{//If in parallel, add jobs as JOB_BACKGROUND, without waiting
                    if(job_list_add(&jobs, pidval, strvec_get(&tokens, 0),JOB_BACKGROUND)==-1){
                        perror("job_list_add");
                    }
                }
            }
        }
        strvec_clear(&tokens);
        printf("%s", PROMPT);
    }
    job_list_free(&jobs);
    return 0;
}
