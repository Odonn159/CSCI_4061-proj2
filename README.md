# CSCI_4061-proj2
This will be our first project involving significant use of system calls, specifically system calls for process creation, process management, I/O, and signal handling. While a single system call by itself is not always interesting, the real challenge and excitement of systems programming is in combining system calls to build useful and powerful tools. We will be building such a tool, the Simple Working Implementation Shell (swish), in this project.  


Whenever you are using a terminal program, you are really interacting with a shell process. Command-line shells allow one access to the capabilities of a computer using simple, interactive means. Type the name of a program and the shell will bring it to life as a new process, run it, and show output. Familiarizing yourself with the inner workings of shells will give you a chance to appreciate how system calls can be usefully combined and will make you a more effective command-line user.  


The goal of this project is to write a simplified command-line shell called swish. This shell will be less functional in many ways from standard shells like bash (the default on most Linux machines), but will still have some useful features.  


This project will cover a number of important systems programming topics:  


String tokenization using strtok()  

Getting and setting the current working directory with getcwd() and chdir()  

Program execution using fork() and wait()  

Child process management using wait() and waitpid()  

Input and output redirection with open() and dup2()  

Signal handling using setpgrp(), tcsetpgrp(), and sigaction()  

Managing foreground and background job execution using signals and kill()  
