#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define MAX_ARGS 10

int tokenize(char *s, strvec_t *tokens) {
    char *token = strtok(s, " ");
    //strtok for first token before " "
    if(token==NULL){
        perror("Failed to Tokenize");
        return -1;
    }
    while(token !=NULL){
        //add to strvec
        if(strvec_add(tokens, token)==-1){
            perror("strvec_add");
            return -1;
        }
        //strtok for next argument
        token = strtok(NULL, " ");
    }
    return 0;
}

int run_command(strvec_t *tokens) {
    //set up action handler
    struct sigaction sac;
    sac.sa_handler = SIG_DFL;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return -1;
    }
    sac.sa_flags = 0;
    //Set up flags and commands
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return -1;
    }
    //set groupid to be its own pid
    if(setpgid(getpid(),getpid())==-1){
        perror("setpgid");
        return -1;
    }
    //Set up a list so it is easy to access all relevant elements and pass them into execvp
    char *tokenlist[MAX_ARGS];
    //token is the currently iterated token within strvec
    char *token = "";
    
    int redout = strvec_find(tokens, ">");//index where > occurs, -1 if not occuring
    int redin = strvec_find(tokens, "<");//index where < occurs, -1 if not occuring
    int redoutappend = strvec_find(tokens, ">>");//index where >> occurs, -1 if not occuring
    int x =0;
    while(x<MAX_ARGS && token!=NULL){
        //iterate through all tokens and add them to tokenlist, in order to pass all args into exec
        x+=1;
        token = strvec_get(tokens, x);
        //Should never error strvecget, as when it fails returning NULL is exactly what we want it to do
        tokenlist[x] = token;
    }
    if(redoutappend!=-1){ //Open the file one after the >> arg
        int fd = open(strvec_get(tokens, redoutappend+1), O_CREAT|O_WRONLY|O_APPEND, S_IWUSR|S_IRUSR);
        if(fd==-1){
            perror("Failed to open Output File");
            return -1;
        }
        //Redirect as needed
        if(dup2(fd, STDOUT_FILENO)==-1){
            perror("dup2");
            close(fd);
            return -1;
        }
        tokenlist[redoutappend]=NULL;
        //Set an item to Null  (Sentinel Value) so that the program exec stops passing in arguments
        //specifically the token at element >>, so that no later elements are read.
        close(fd);
    }
    else if(redout!=-1){ //Open the file one after the > arg
        int fd = open(strvec_get(tokens, redout+1), O_CREAT|O_TRUNC|O_WRONLY, S_IWUSR|S_IRUSR);
        if(fd==-1){
            perror("Failed to open Output File");
            return -1;
        }
        //Redirect as needed
        if(dup2(fd, STDOUT_FILENO)==-1){
            perror("dup2");
            close(fd);
            return -1;
        }
        tokenlist[redout]=NULL;
        //Set an item to Null  (Sentinel Value) so that the program exec stops passing in arguments
        //specifically the token at element >, so that no later elements are read.
        close(fd);
    }
    if(redin!=-1){//Open the file one after the < arg
        int fd = open(strvec_get(tokens, redin+1), O_RDONLY);
        if(fd==-1){
            perror("Failed to open input file");
            return -1;
        }
        //Redirect as needed
        if(dup2(fd, STDIN_FILENO)==-1){
            perror("dup2");
            close(fd);
            return -1;
        }
        tokenlist[redin]=NULL;
        //Set an item to Null  (Sentinel Value) so that the program exec stops passing in arguments
        //specifically the token at element <, so that no later elements are read.
        close(fd);
    }
    //The sentinel value (NULL), will either be the last element in tokenlist, after all other args are passed in
    //or if there was a redirect (input, output or output and append), then it is the first index where a redirect command occurs.
    //Otherwords the NULL sentinel value will occur at the end of the tokenslist, and at the index, redout, redin, and redoutappend
    //Making certain that the earliest occurrence will always end future args in execvp.
    execvp(strvec_get(tokens, 0),tokenlist);
    //if it reached this line, error occurs as execvp failed.
    perror("exec");
    return -1;
    //was told can assume no duplicates, or > and >>
}

int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground) {
    int index = atoi(strvec_get(tokens,1));
    //Atoi does not detect errors
    if(index>=jobs->length){
        printf("Job index out of bounds\n");
        return -1;
    }
    if(job_list_get(jobs,index)==NULL){
        perror("Job not found");
        return -1;
    }
    pid_t jobid = job_list_get(jobs,index)->pid;
    int status = 0;
    if(is_foreground==1){//Resume it in the foreground
    //I was told by TA, this was proper error checking, specifically as to make sure that the function returns as soon as an error is detected, (no cleaunup)
        if(tcsetpgrp(STDIN_FILENO, jobid)==-1){
            perror("resume_program: tcsetpgrp");
            return -1;
        }//set group
        if(kill(jobid, SIGCONT)==-1){
             perror("resume_program: Kill");
             return -1;
        }//command to resume
        if(waitpid(jobid, &status, WUNTRACED)==-1){
            perror("waitpid");
            return -1;
        }//wait to see if terminated or stopped
        if(!WIFSTOPPED(status)){//if terminated remove
            job_list_remove(jobs, index);
        }//reset group
        if(tcsetpgrp(STDIN_FILENO, getpid())==-1){
            perror("resume_program: tcsetpgrp");
            return -1;
        }
    }
    else{//If it's in the background, resume it
    //no redirect groups.
        if(kill(jobid, SIGCONT)==-1){
             perror("resume_program: Kill");
             return -1;
        }
        //joblistget has already confirmed to have worked do to above error checking
        job_list_get(jobs,index)->status = JOB_BACKGROUND;
    }
    return 0;
}

int await_background_job(strvec_t *tokens, job_list_t *jobs) {
    int index = atoi(strvec_get(tokens,1));
    //Atoi does not detect errors
    job_t *currentjob = job_list_get(jobs,index);
    if(currentjob==NULL){
        perror("job_list_get");
        return -1;
    }
    int status;
    if(currentjob->status==JOB_BACKGROUND){
//I was told by TA, this was proper error checking, specifically as to make sure that the function returns as soon as an error is detected, (no cleaunup) or modifying job function.
        if(waitpid(currentjob->pid, &status, WUNTRACED)==-1){
            perror("waitpid");
            return -1;
        }
        //if stopped terminated, remove
        if(!WIFSTOPPED(status)){
            job_list_remove(jobs,index);
        }
        return 0;
    }
    else{
        printf("Job index is for stopped process not background process\n");
        return -1;
    }
}

int await_all_background_jobs(job_list_t *jobs) {
    job_t *currentjob = job_list_get(jobs,0); //I chose this as it was easier than jobs-> head in my mind
    int statuslocal =0;
    while(currentjob!=NULL){
        //for each job, that is not stopped, wait for it to finish
        if(currentjob->status!=JOB_STOPPED){
            //This guaranteses that all are background, as Either JOB stopped or job background are the only two options.
            if(waitpid(currentjob->pid, &statuslocal, WUNTRACED)==-1){
                perror("waitpid");
                return -1;
            }
            //TLDR: I was told by TA, this was OK. 
            //I was told specifically as to make sure that the function returns as soon as an error is detected, (no cleaunup)
            //So some if an error has occurred, some functions may have terminated naturally, before wait errors out
            //But they are not updated or removed from the joblists
            //I do not know if an error in a job that is halfway through the job_list would cause cascading errors for all future commands. 
            //Making it so that jobs which no longer are running and have been terminated still are flagged as JOB_BACKGROUND,
            //I do not know how a looping waitpid command works in that scenario and
            //I cannot test it on my own. I could not find a way to make waitpid fail, using normal shell commands in an organic way...
            //So I am taking the TA's word and assuming that this is just ok/acceptable. 

            //If stopped, update the job struct
            if(WIFSTOPPED(statuslocal)){
                currentjob->status=JOB_STOPPED;
            }
        }
        //Iterate to next
        currentjob=currentjob->next;
    }
    //No error checking for remove by status
    job_list_remove_by_status(jobs, JOB_BACKGROUND);
    //Remove all background processes from job list, as they are now done.
    return 0;
}
