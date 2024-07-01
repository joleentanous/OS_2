#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

//possible commands
int exec_com(int count, char** arglist);
int execBackground_com(int count, char** arglist);
int pipe_com(int count, char** arglist, int pipe_ind);
int inputRedirect_com(int count, char** arglist);
int outputRedirect_com(int count, char** arglist);
int contains_pipe(int count, char** arglist);



int exec_com(int count, char** arglist){
    pid_t pid = fork();
    if (pid == -1){
        perror("Fork failed!");
        return 0;
    }
    if (pid == 0){ // child
        if (signal(SIGINT,SIG_DFL) == SIG_ERR){ //terminate on SIGINT in child process
            perror("SIGINT handling failed");
            exit(EXIT_FAILURE);
        }
        if (signal(SIGCHLD,SIG_DFL) == SIG_ERR){ //terminate on SIGCHLD in child process
            perror("SIGCHLD handling failed");
            exit(EXIT_FAILURE);
        }

        int exec_res = execvp(arglist[0], arglist); //executing commands
        if (exec_res == -1){
            perror("execvp failed!");// this means that the execvp returned, AKA it failed
            exit(EXIT_FAILURE); // 
        }
        
    }

    else { //parent
        pid_t wait_res = waitpid(pid, NULL, 0);
        if (wait_res == -1){ // to avoid zombies
            if ((errno != ECHILD) && (errno != EINTR)){
                perror("Waitpid failed!");
                return 0;
            }
        }
    }
    return 1;
    
}



int execBackground_com(int count, char** arglist){
    arglist[count-1] = NULL; // to not pass the '&' symbol
    pid_t pid = fork();
    if (pid == -1){
        perror("Fork failed!");
        return 0;
    }
    else if (pid == 0) { //child
        if (signal(SIGCHLD,SIG_DFL) == SIG_ERR){
            perror("SIGCHLD handling failed");
            exit(EXIT_FAILURE);
        } // this is a background child process, and so it doesnt terminate on SIGINT
        int exec_res = execvp(arglist[0], arglist);
        if (exec_res == -1){
            perror("execvp failed!");// this means that the execvp returned, AKA it failed
            exit(EXIT_FAILURE); // 
        }
    }
    // parent
    return 1;

}


int pipe_com(int count, char** arglist, int pipe_ind){
    arglist[pipe_ind] = NULL; // to execute correctly, nullifying the '|' symbol
    int pipe_ends[2]; //pipe setup
    if (pipe(pipe_ends) == -1) {
        perror("Piping failed!");
        return 0;
    }

    pid_t pid = fork();
    if (pid == -1){
        perror("Fork failed!");
        return 0;
    }
    if (pid == 0) { //child - read end
        if (signal(SIGINT,SIG_DFL) == SIG_ERR){
            perror("SIGINT handling failed");
            exit(EXIT_FAILURE);
        }
        if (signal(SIGCHLD,SIG_DFL) == SIG_ERR){
            perror("SIGCHLD handling failed");
            exit(EXIT_FAILURE);
        }
        close(pipe_ends[0]);
        int dup_res = dup2(pipe_ends[1],STDOUT_FILENO);
        if (dup_res == -1){
            perror("dup2 failed on stdout!");
            exit(EXIT_FAILURE);
        }
        close(pipe_ends[1]);
        int exec_res = execvp(arglist[0], arglist);
        if (exec_res == -1){
            perror("execvp failed!");// this means that the execvp returned, AKA it failed
            exit(EXIT_FAILURE); // 
        }
    }
    // parent
    close(pipe_ends[1]); //using the parent as the second end instead of forking twice
    int dup_res = dup2(pipe_ends[0],STDIN_FILENO);
        if (dup_res == -1){
            perror("dup2 failed on stdin!");
            return 0; // maybe use: exit(EXIT_FAILURE)
        }
    close(pipe_ends[0]);
    int exec_res = execvp(arglist[pipe_ind + 1], arglist + pipe_ind + 1);
        if (exec_res == -1){
            perror("execvp failed!");// this means that the execvp returned, AKA it failed
            return 0; // maybe use: exit(EXIT_FAILURE)
        }
    pid_t wait_res = waitpid(pid, NULL, 0);
    if (wait_res == -1){
        if ((errno != ECHILD) && (errno != EINTR)){
            perror("Waitpid failed!");
            return 0;
        }
    }

    return 1;
}


int inputRedirect_com(int count, char** arglist){
    char* inp_file = arglist[count-1];
    int fd = open(inp_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); //Read and write for the user, read-only for group and others
    if (fd == -1){
        perror("failed to open input file!");
        return 0;
    }

    arglist[count-1] = arglist[count-2] = NULL;

    pid_t pid = fork();
    if (pid == -1){
        perror("Fork failed!");
        close(fd);
        return 0;
    }
    if (pid == 0) { //child - read end
        if (signal(SIGINT,SIG_DFL) == SIG_ERR){
            perror("SIGINT handling failed");
            close(fd);
            exit(EXIT_FAILURE);
        }
        if (signal(SIGCHLD,SIG_DFL) == SIG_ERR){
            perror("SIGCHLD handling failed");
            close(fd);
            exit(EXIT_FAILURE);
        }
        int dup_res = dup2(fd, STDIN_FILENO);
        if (dup_res == -1) {
            perror("dup2 failed on stdin!");
            close(fd);
            exit(EXIT_FAILURE); // 
        }
        close(fd);
        int exec_res = execvp(arglist[0], arglist);
        if (exec_res == -1){
            perror("execvp failed!");// this means that the execvp returned, AKA it failed
            exit(EXIT_FAILURE); // maybe use: return 0
        } 
    }
    else{ //parent
        close(fd);// not necessary
        pid_t wait_res = waitpid(pid, NULL, 0);
        if (wait_res == -1){
            if ((errno != ECHILD) && (errno != EINTR)){
                perror("Waitpid failed!");
                return 0;
            }
        }
    }
    return 1;
}


int outputRedirect_com(int count, char** arglist){
    char* out_file = arglist[count-1];
    int fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); //Read and write for the user, read-only for group and others
    if (fd == -1){
        perror("failed to open input file!");
        return 0;
    }

    arglist[count-1] = arglist[count-2] = NULL;

    pid_t pid = fork();
    if (pid == -1){
        perror("Fork failed!");
        close(fd);
        return 0;
    }
    if (pid == 0) { //child - read end
        if (signal(SIGINT,SIG_DFL) == SIG_ERR){
            perror("SIGINT handling failed");
            close(fd);
            exit(EXIT_FAILURE);
        }
        if (signal(SIGCHLD,SIG_DFL) == SIG_ERR){
            perror("SIGCHLD handling failed");
            close(fd);
            exit(EXIT_FAILURE);
        }
        int dup_res = dup2(fd, STDOUT_FILENO);
        if (dup_res == -1) {
            perror("dup2 failed on stdout!");
            close(fd);
            exit(EXIT_FAILURE); // 
        }
        close(fd); 
        int exec_res = execvp(arglist[0], arglist);
        if (exec_res == -1){
            perror("execvp failed!");// this means that the execvp returned, AKA it failed
            exit(EXIT_FAILURE); // maybe use: return 0
        } 
    }
    else{ //parent
        close(fd);// not necessary
        pid_t wait_res = waitpid(pid, NULL, 0);
        if (wait_res == -1){
            if ((errno != ECHILD) && (errno != EINTR)){
                perror("Waitpid failed!");
                return 0;
            }
        }
    }
    return 1;
}




int contains_pipe(int count, char** arglist){
    for (int i = 0; i<count; i++){
        if (strcmp(arglist[i], "|") == 0){
            return i;
        }
    }
    return -1;
}

int prepare(void){ //signal handling for SIGINT (Ctrl-C) //should not use signal
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("Error - failed to change signal SIGINT handling");
        return -1;
    }
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) { // dealing with zombies based on ERAN'S TRICK
        perror("Error - failed to change signal SIGCHLD handling");
        return -1;
    }
    return 0;
}



int process_arglist(int count, char** arglist){
    int ret_val;
    int pipe_ind = contains_pipe(count, arglist); //checks piping commands
    if (*arglist[count-1] == '&'){ //background commands
        ret_val = execBackground_com(count,arglist);
    }
    else if ((count > 2) && (pipe_ind != -1)){ //pipe commands
        ret_val = pipe_com(count, arglist, pipe_ind);
    }
    else if ((count > 2) && ((strcmp(arglist[count-2],"<")) == 0)){ //input redirecting
        ret_val = inputRedirect_com(count, arglist);
    }
    else if ((count > 2) && ((strcmp(arglist[count-2],">>")) == 0)){ //output redirecting
        ret_val = outputRedirect_com(count, arglist);
    }
    else{ //regular commands
        ret_val = exec_com(count, arglist);
    }

    return ret_val;
}


int finalize(void){
    return 0;
}
