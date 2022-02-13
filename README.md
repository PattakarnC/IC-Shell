# IC-Shell
A simple shell for Linux. The functionality of this shell is similar to other popular Linux shells such as bash, csh, zsh, but with a subset of features.

## Basic Functionality
* Interactive and batch mode
* Support built-in some commands 
* Allow the user to execute one or more programs from executable files as either background or foreground jobs
* Provide job-control, including a job list and tools for changing the foreground/background status of currently running jobs and job suspension/continuation/termination.
* Allow for input and output redirection to/from files.

## Built-in Commands
* **`echo <text>`**
  * The echo command prints a given text (until EOL) back to the console.
* **`!!`**
  * Repeat the last command given to the shell.
* **`exit <num>`**
  * This command exits the shell with a given exit code.
* **`echo $?`**
  * This prints out the exit status code of the previous command.
* **`jobs`**
  * The jobs command shows a table of uncompleted job information (job ids, process status, commands). 
* **`fg %<job_id>`**
  * Brings the job identified by `<job_id>` into the foreground. If this job was previously stopped, it will be running.
* **`bg %<job_id>`**
  * Execute the suspended job identified by `<job_id>` in the background.

## Extra Feature
Type **`quiz`** to start solving 20 simple math problems and see how accurate/fast you are!
