#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

int prepare(void){
    if(signal(SIGINT,SIG_IGN)==SIG_ERR){
        perror("error changing the handling of the signal");
        return 1;/*this indicates an error*/
    }
    if(signal(SIGCHLD,SIG_IGN)==SIG_ERR){
        perror("error changing the handling of the signal");
        return 1;/*this indicates an error*/
    }
    return 0;
}

/*arglist - a list of char* arguments (words) provided by the user*/ 
/*it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL*/ 
/*RETURNS - 1 if should continue, 0 otherwise*/ 
int process_arglist(int count, char** arglist){
    int i=0, flag=0;
    /*flag =0 at the end of the for indicates we should do the simple execution,
    meaning that we should execute a foreground process and waits for it */
    int pipe_arr[2];
    int input_file;
    int waiting_status,waiting_status1,waiting_status2;
    pid_t pid,pid1,pid2;
    for(i=0;i<count;i++){
        /*from the assumption provided in the pdf, at most one if condition is satisfied*/
        if(*arglist[i]=='&'){
            /*executing background processes*/
            flag=1;
            pid=fork();
            if(pid>0){
                /*the parent process*/
                return 1;
            }
            else{
                if(pid==0){
                    /*the child process*/
                    if(signal(SIGCHLD,SIG_DFL)==SIG_ERR){
                        perror("An error has occured changing the signal handling.");
                        exit(1);
                    }
                    arglist[count-1]=NULL;
                    execvp(arglist[0],arglist);
                    /*the next two lines are reachable only in case the execvp
                    didn't work, another way is to check if the return value is -1
                    and then prints the error */ 
                    perror("An error has occured executing the command, Make sure that you have provided a valid command.");
                    exit(1);
                }
                else{
                    /*we got an error since pid==-1*/
                    perror("An error has occured in the fork.");
                    return 0;
                }
            }
        }
        if(*arglist[i]=='|'){
            flag=1;
            if(pipe(pipe_arr)==-1){
                perror("An  error has occured in the pipe.");
                return 0;
            }

            pid1=fork();
            if(pid1==0){
                /*the first child process is only writing
                so we close the reading pipe end then we
                redirect the stdout to write to the reading pipe*/
                if(signal(SIGINT,SIG_DFL)==SIG_ERR){
                    perror("An error has occured changing the signal handling.");
                    exit(1);
                }
                if(signal(SIGCHLD,SIG_DFL)==SIG_ERR){
                    perror("An error has occured changing the signal handling.");
                    exit(1);
                }
                close(pipe_arr[0]);

                dup2(pipe_arr[1],1);/*1 is stdout*/
                arglist[i]=NULL;
                execvp(arglist[0],arglist);
                perror("An error has occured executing the command, Make sure that you have provided a valid command.");
                exit(1);
            }
            if(pid1==-1){
                /*we got an error*/
                perror("An error has occured in the fork.");
                return 0;
            }
            pid2=fork();
            if(pid2==0){
                /*the second child process is only reading
                so we close the writing pipe end and then we
                redirect the stdin to read from the pipe*/
                if(signal(SIGINT,SIG_DFL)==SIG_ERR){
                    perror("An error has occured changing the signal handling.");
                    exit(1);
                }
                if(signal(SIGCHLD,SIG_DFL)==SIG_ERR){
                    perror("An error has occured changing the signal handling.");
                    exit(1);
                }
                close(pipe_arr[1]);

                dup2(pipe_arr[0],0);/*0 is stdin*/
                execvp(arglist[i+1],&arglist[i+1]);
                perror("An error has occured executing the command, Make sure that you have provided a valid command.");
                exit(1);
            }
            if(pid2==-1){
                /*we got an error*/
                perror("An error has occured in the fork.");
                return 0;
            }
            /*next lines to close both the reading and writing and then
            waits for the child processes to finihs*/
            close(pipe_arr[0]);
            close(pipe_arr[1]);
            waitpid(pid1,&waiting_status1,0);
            waitpid(pid2,&waiting_status2,0);

            if(waiting_status1==-1 && errno!=ECHILD && errno!=EINTR){
                perror("An error has occured in waiting the child process.");
                return 0;
            }
            if(waiting_status2==-1 && errno!=ECHILD && errno!=EINTR){
                perror("An error has occured in waiting the child process.");
                return 0;
            }
        }
        if(*arglist[i]=='<'){
            flag=1;
            input_file=open(arglist[i+1],O_RDONLY,0777);
            if(input_file==-1){
                perror("An error has occured opening the file.");
                return 0;
            }
            pid=fork();
            if(pid==0){
                if(signal(SIGINT,SIG_DFL)==SIG_ERR){
                    perror("An error has occured changing the signal handling.");
                    exit(1);
                }
                if(signal(SIGCHLD,SIG_DFL)==SIG_ERR){
                    perror("An error has occured changing the signal handling.");
                    exit(1);
                }


                if(dup2(input_file,0)==-1){
                    perror("An error has occured in input redirecting: in dup2.");
                    exit(1);
                }
                arglist[i]=NULL;
                execvp(arglist[0],arglist);
                perror("An error has occured executing the command, Make sure that you have provided a valid command.");
                exit(1);
            }
            if(pid==-1){
                /*we got an error*/
                perror("An error has occured in the fork.");
                return 0;
            }
            close(input_file);
            waitpid(pid,&waiting_status,0);
            if(waiting_status==-1 && errno!=ECHILD && errno!=EINTR){
                perror("An error has occured in waiting the child process.");
                return 0;
            }
            
        }
        if(*arglist[i]=='>'){

            flag=1;
            input_file=open(arglist[i+1],O_WRONLY | O_CREAT | O_TRUNC,0777);
            if(input_file==-1){
                perror("An error has occured opening the file.");
                return 0;
            }
            pid=fork();
            if(pid==0){
                if(signal(SIGINT,SIG_DFL)==SIG_ERR){
                    perror("An error has occured changing the signal handling.");
                    exit(1);
                }
                if(signal(SIGCHLD,SIG_DFL)==SIG_ERR){
                    perror("An error has occured changing the signal handling.");
                    exit(1);
                }
                if(dup2(input_file,1)==-1){
                    perror("An error has occured in output redirecting: in dup2.");
                    exit(1);
                }
                
                arglist[i]=NULL;
                execvp(arglist[0],arglist);
                perror("An error has occured executing the command, Make sure that you have provided a valid command.");
                exit(1);
            }
            if(pid==-1){
                /*we got an error*/
                perror("An error has occured in the fork.");
                return 0;
            }
            close(input_file);
            waitpid(pid,&waiting_status,0);
            if(waiting_status==-1 && errno!=ECHILD && errno!=EINTR){
                perror("An error has occured in waiting the child process.");
                return 0;
            }
        }
        
    }
    if(flag==0){
        pid=fork();
        if(pid==-1){
            perror("An error has occured in the fork.");
            return 0;
        }
        else{
            if(pid==0){
                if(signal(SIGINT,SIG_DFL)==SIG_ERR){
                    perror("An error has occured changing the signal handling.");
                    exit(1);
                }
                execvp(arglist[0],arglist);
                perror("An error has occured executing the command, Make sure that you have provided a valid command.");
                exit(1);
            }
            waiting_status=waitpid(pid,&waiting_status,0);
            if(waiting_status==-1 && errno!=ECHILD && errno!=EINTR){
                perror("An error has occured in waiting the child process.");
                return 0;
            }
        }
    }
    return 1;
}

int finalize(void){
    return 0;
}