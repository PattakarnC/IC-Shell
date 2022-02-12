#define _POSIX_SOURCE

#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>  

#define	MAX_CMD_CHAR 256

char last_cmd[MAX_CMD_CHAR];    // a string holder for last user's input
char cmd[MAX_CMD_CHAR];         // a string holder for user's input
pid_t pid;
int exit_code;

struct job_elem {
    int jid;                    // job ID
    pid_t pid;                  // process ID
    char* state;                // state of a process (E.g. bg, fg, s)
    char** command;             // command
    struct job_elem* next;      // pointer to next job
};
typedef struct job_elem job; 

int job_no = 1;

job* head = NULL;               // head of linked list 

// for duplicating string array 
char** copy_array_of_string(char** src) {
    char** array = malloc(MAX_CMD_CHAR * sizeof(char*));
    for (int i = 0; i < MAX_CMD_CHAR; i++) {
        array[i] = malloc(MAX_CMD_CHAR * sizeof(char));
    } 
    int index = 0;
    while (src[index] != NULL) {
        strcpy(array[index], src[index]);
        index++;
    }
    return array;
}

void add_job(int jid, pid_t pid, char* state, char** command) {
    job* link = (job*) malloc(sizeof(job));
    
    link->jid = job_no;
    link->pid = pid;
    link->command = copy_array_of_string(command);

    size_t len = strlen(state) + 1;
    link->state = (char *)malloc(sizeof(char) * (len + 1));
    memcpy(link->state, state, len);

    link->next = head;
    head = link;
    job_no++;
}

void remove_job_with_pid(pid_t pid) {
    job* current = head;
    job* previous;
    while (current != NULL) {
        if (current->pid == pid) {
            if (current == head) {
                head = current->next;
            }
            else {
                previous->next = current->next;
            }
            job_no--;
        }
        previous = current;
        current = current->next;
    }
}

void print_job_list() {
    int j = 0;
    job* current = head;
    while (current != NULL) {

        if (strcmp(current->state, "bg") == 0) {
            if (current->pid == head->pid) {
                printf("[%d]+  Running                 ", current->jid);
            }
            else if (current->pid == head->next->pid) {
                printf("[%d]-  Running                 ", current->jid);
            }
            else {
                printf("[%d]   Running                 ", current->jid);
            }
            while (strcmp(current->command[j], "")) {
                printf("%s ", current->command[j]);
                j++;
            }
            j = 0;
            printf("&\n");
        }
        
        else if (strcmp(current->state, "s") == 0) {
            if (current->pid == head->pid) {
                printf("[%d]+  Stopped                 ", current->jid);
            }
            else if (current->pid == head->next->pid) {
                printf("[%d]-  Stopped                 ", current->jid);
            }
            else {
                printf("[%d]   Stopped                 ", current->jid);
            }
            while (strcmp(current->command[j], "")) {
                printf("%s ", current->command[j]);
                j++;
            }
            j = 0;
            printf("\n");
        }
        current = current->next;
    }
}

void run_foreground(int jid) {
    int ppid = getpid();
    job* current = head;
    while (current != NULL) {
        if (current->jid == jid) {
            int j = 0;
            int status;
            
            while (strcmp(current->command[j], "")) {
                if (strcmp(current->command[j], "&")) {
                    printf("%s ", current->command[j]);
                }
                j++;
            }
            j = 0;
            printf("\n");

            tcsetpgrp(STDIN_FILENO, current->pid);              // give the terminal access
            kill(current->pid, SIGCONT);                        // continue where the process left off
            waitpid(current->pid, &status, WUNTRACED);          
            
            if (WIFEXITED(status)) { 
                exit_code = WEXITSTATUS(status);
            }
            if (WIFSIGNALED(status)) { 
                exit_code = WTERMSIG(status);           
            }
            if (WIFSTOPPED(status)) {   
                current->state = "s";
                if (current->pid == head->pid) {
                    printf("[%d]+  Stopped                 ", current->jid);
                }
                else if (current->pid == head->next->pid) {
                    printf("[%d]-  Stopped                 ", current->jid);
                }
                else {
                    printf("[%d]   Stopped                 ", current->jid);
                }
                while (strcmp(current->command[j], "")) {
                    printf("%s ", current->command[j]);
                    j++;
                }
                printf("\n");
                
                tcsetpgrp(STDIN_FILENO, ppid); 
                exit_code = WSTOPSIG(status);
                return;       
            }

            remove_job_with_pid(current->pid);              
            tcsetpgrp(STDIN_FILENO, ppid);                     // take back the terminal access
            return;
        }
        current = current->next;
    }

    // if the given job ID is not in the list
    printf("fg: %%%d: no such job\n", jid);
    exit_code = 1;
}

void run_background(int jid) {
    job* current = head;
    while (current != NULL) {
        if (current->jid == jid && strcmp(current->state, "s") == 0) {
            int j = 0;
            current->state = "bg";

            if (current->pid == head->pid) {
                printf("[%d]+ ", current->jid);
            }
            else if (current->pid == head->next->pid) {
                printf("[%d]- ", current->jid);
            }
            else {
                printf("[%d] ", current->jid);
            }

            while (strcmp(current->command[j], "")) {
                printf("%s ", current->command[j]);
                j++;
            }
            printf("&\n");

            kill(current->pid, SIGCONT);
            return;
        }
        current = current->next;
    }

    // if the given job ID is not in the list
    printf("bg: %%%d: no such job\n", jid);
    exit_code = 1;
}


// check for "&" character in a string
int background_checker(char** input) {
    int index = 0;
    int n = 0;
    while (input[index] != NULL) {
        n++;
        if (strcmp(input[index], "&") == 0 && n > 1) {
            input[index] = NULL;
            return 1;
        }
        index++;
    }
    return 0;
}

void input_redirection(char* fileName) {
    int file = open(fileName, O_RDONLY);      // read only
    if (file == -1) {
        printf("Error opening file\n");
        exit(errno);
    }
    dup2(file, STDIN_FILENO);
    if (close(file) == -1) {
        printf("Error closing file\n");
        exit(errno);
    }
}

void output_redirection(char* fileName) {
    int file = open(fileName, O_WRONLY | O_CREAT | O_TRUNC , 0777);      // write only + create a file if not exist + truncate the file
    if (file == -1) {
        printf("Error opening file\n");
        exit(errno);
    }
    dup2(file, STDOUT_FILENO);
    if (close(file) == -1) {
        printf("Error closing file\n");
        exit(errno);
    }
}

void redirection_checker(char** input) {
    char* fileName;
    int i = 0;

    while (input[i] != NULL) {
        fileName = input[i + 1]; 

        // if the string is "<"  and the input is not only "<"
        if (!strcmp(input[i], "<") && i > 0) {
            input_redirection(fileName);
            input[i] = NULL;
            input[i + 1] = NULL;
            return;
        }

        // if the string is ">" and the input is not only ">"
        else if (!strcmp(input[i], ">") && i > 0) {
            output_redirection(fileName);
            input[i] = NULL;
            input[i + 1] = NULL;
            return;
        }
        i++;
    }
}

void assign_last_cmd(char* input) {
    input[strcspn(input, "\n")] = 0;         // remove trailing new line from input 
    strcpy(last_cmd, input);                 // assign it to last_cmd
}

// check if the given string is empty or contains only white space
int is_empty(char* str) {
    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

void exit_with_status(char* status) {
    if (status == NULL) {
        printf("Please specify the exit code!\n");
    }
    else {
        printf("bye");
        exit(atoi(status));
    }
}

// source: https://www.geeksforgeeks.org/c-program-to-trim-leading-white-spaces-from-string/
char* trim_leading_spaces(char* str) {
    static char str1[MAX_CMD_CHAR];
    int count = 0, j, k;
    while (str[count] == ' ') {
        count++;
    }
    for (j = count, k = 0;
        str[j] != '\0'; j++, k++) {
        str1[k] = str[j];
    }

    str1[k] = '\0';
    return str1;
}

// source: https://codeforwin.org/2016/04/c-program-to-trim-trailing-white-space-characters-in-string.html
char* trim_trailing_spaces(char* str) {
    int index = -1;
    int i = 0;
    while (str[i] != '\0') {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n') {
            index= i;
        }
        i++;
    }
    str[index + 1] = '\0';
    return str;
}

// Input: echo hello world
// Output: {"echo", "hello", "world"}
char** get_cmd_agrs_as_tokens(char* input) {
    char** tokens = malloc(MAX_CMD_CHAR * sizeof(char*));
    for (int i = 0; i < MAX_CMD_CHAR; i++) {
        tokens[i] = malloc(MAX_CMD_CHAR * sizeof(char));
    }
    char* pch;
    int index = 0; 
    pch = strtok(input," ");
    while (pch != NULL) {
        tokens[index] = pch;
        pch = strtok(NULL, " ");
        index++;
    }
    tokens[index] = '\0';
    return tokens;
}

// Input: echo hello world
// Output: {"echo", "hello world"}
char** get_cmd_and_args(char* input) {
    char* cleanInput = trim_leading_spaces(input);
    char** cmdAndArgs = malloc(2 * sizeof(char*));
    for (int i = 0; i < 2; i++) {
        cmdAndArgs[i] = malloc(MAX_CMD_CHAR * sizeof(char));
    } 

    char* command = strtok(cleanInput, " ");
    char* argument = strtok(NULL, "\n");

    strcpy(cmdAndArgs[0], trim_trailing_spaces(command));

    // if there is no argument in the input, assign null to the argument string
    if (argument == NULL) {
        cmdAndArgs[1] = '\0';
    }

    // else, trim the trailing and leading white space and assign it
    else {
        char* sanitisedArg = trim_trailing_spaces(argument);
        strcpy(cmdAndArgs[1], trim_leading_spaces(sanitisedArg)); 
    }

    return cmdAndArgs;
}

void cmd_handler(char* input) {
    char listOfCmd[6][5] = {"echo", "!!", "exit", "jobs", "fg", "bg"};
    char** inputArgs = get_cmd_and_args(input);

    // command = echo
    if (strcmp(inputArgs[0], listOfCmd[0]) == 0) {
        assign_last_cmd(input);
        if (inputArgs[1] == NULL) {
            printf("\n");
        }
        else if (strcmp(inputArgs[1], "$?") == 0) {
            printf("%d\n", exit_code);
        }
        else {
            printf("%s\n", inputArgs[1]);
        }
        exit_code = 0;
    } 
    
    // command = !! 
    else if (strcmp(inputArgs[0], listOfCmd[1]) == 0) {

        // if the user has already put other commands (last_cmd != NULL) 
        if (!is_empty(last_cmd)) {
            printf("%s\n", last_cmd);
            cmd_handler(last_cmd);
            exit_code = 0;
        }
    }

    // command = exit
    else if (strcmp(inputArgs[0], listOfCmd[2]) == 0) {
        exit_with_status(inputArgs[1]);
        exit_code = 0;
    }

    // command = jobs
    else if (strcmp(inputArgs[0], listOfCmd[3]) == 0) {
        assign_last_cmd(input);
        print_job_list();
        exit_code = 0;
    }

    // command = fg
    else if (strcmp(inputArgs[0], listOfCmd[4]) == 0) {
        assign_last_cmd(input);
        char** tokens = get_cmd_agrs_as_tokens(input);
        if (tokens[1] != NULL) {
            char num = tokens[1][1];
            if (tokens[1][0] == '%' && !isalpha(num) && num != 0) {
                int jid = num - '0';
                run_foreground(jid);
            }
            else {
                printf("fg: %s: no such job\n", tokens[1]);
                exit_code = 1;
            }
        }
        else {
            // if the user only put "fg"
            printf("Please specify job ID!\n");   
            exit_code = 1;
        }
        free(tokens);
    }

    // command = bg
    else if (strcmp(inputArgs[0], listOfCmd[5]) == 0) {
        assign_last_cmd(input);
        char** tokens = get_cmd_agrs_as_tokens(input);
        if (tokens[1] != NULL) {
            char num = tokens[1][1];
            if (tokens[1][0] == '%' && !isalpha(num) && num != 0) {
                int jid = num - '0';
                run_background(jid);
            }
            else {
                printf("bg: %s: no such job\n", tokens[1]);
                exit_code = 1;
            }
        }
        else {
            // if the user only put "bg"
            printf("Please specify job ID!\n");   
            exit_code = 1;
        }
        free(tokens);
    }
    
    else {
        assign_last_cmd(input);
        char** tokens = get_cmd_agrs_as_tokens(input);
        int background = background_checker(tokens);
        int status;
        pid = fork();

        // parent
        if (pid > 0) {
            if (!background) {
                tcsetpgrp(STDIN_FILENO, pid);          
                waitpid(pid, &status, WUNTRACED);

                if (WIFEXITED(status)) {               // if the process terminated normally
                    exit_code = WEXITSTATUS(status);
                }
                if (WIFSIGNALED(status)) {             // if the process was terminated by a signal
                    exit_code = WTERMSIG(status);
                }
                if (WIFSTOPPED(status)) {              // if the process was stopped by a signal
                    add_job(job_no, pid, "s", tokens);
                    int j = 0;
                    job* current = head;
                    while (current != NULL) {
                        if (current->pid == pid) {
                            if (current->pid == head->pid) {
                                printf("[%d]+  Stopped                 ", current->jid);
                            }
                            else if (current->pid == head->next->pid) {
                                printf("[%d]-  Stopped                 ", current->jid);
                            }
                            else {
                                printf("[%d]   Stopped                 ", current->jid);
                            }
                            while (strcmp(current->command[j], "")) {
                                printf("%s ", current->command[j]);
                                j++;
                            }
                            j = 0;
                            printf("\n");
                        }
                        current = current->next;
                    }
                    exit_code = WSTOPSIG(status);       
                }
                tcsetpgrp(STDIN_FILENO, getpid());     // reset the foreground process group
            }
            else {
                add_job(job_no, pid, "bg", tokens);
                printf("[%d] %d\n", job_no - 1, pid);
            }
        }

        //child
        else if (pid == 0) {

            // check for > and < in the input
            redirection_checker(tokens);             

            // create a process group id for the child process using its PID
            if (setpgid(0, 0) < 0) {      
                perror("ERROR: ");
                exit(EXIT_FAILURE);
            }

            signal(SIGTSTP, SIG_DFL);
            signal(SIGINT, SIG_DFL);

            if (!background) {
                tcsetpgrp(STDIN_FILENO, getpid());   
            }

            int execVal = execvp(tokens[0], tokens);

            // if the user's command doesn't match with any of the command
            if (execVal == -1) {
                printf("bad command\n");
                exit(127);
            }
        }
        else {
            printf("fork error!\n");
            exit(EXIT_FAILURE);
        }
        free(tokens);    
    }
    free(inputArgs);
}
 
void read_command() {
    printf("icsh $ ");

    // to prevent the system from taking an input before printing
    fflush(stdout);

    fgets(cmd, MAX_CMD_CHAR, stdin);
}

void shell_start() {
    while (1) {
        read_command();

        // if the user passes an empty command or space character
        if (is_empty(cmd)) {
            continue;
        }
        else {
            cmd_handler(cmd);
        }
    }
}

void script_mode_start(char* filename) {
    char* buffer = malloc(MAX_CMD_CHAR * sizeof(char));
    FILE* file = fopen(filename, "r");

    // read the file line by line and execute it  
    if (file) {
        while (!feof(file)) {
            memset(buffer, 0x00, MAX_CMD_CHAR);       // clean the buffer
            fscanf(file, "%[^\n]\n", buffer);         // read a line in file 
            if (is_empty(buffer)) {
                continue;
            }
            else {
                cmd_handler(buffer);           
            }
        }
    }
    else {
        printf("the given program does not exist!\n");
    }
    fclose(file);
    free(buffer);
}

// modified from: https://docs.oracle.com/cd/E19455-01/806-4750/signals-7/index.html
void handle_chld() {
    int status;
    pid_t pid;

    pid = waitpid(-1, &status, WNOHANG);    // wait for termination of any procsss 
    
    if (pid > 0) {
        int j = 0;
        job* current = head;
        while (current != NULL) {
            if (current->pid == pid) {
                if (current->pid == head->pid) {
                    printf("\n[%d]+  Done                    ", current->jid);
                }
                else if (current->pid == head->next->pid) {
                    printf("\n[%d]-  Done                    ", current->jid);
                }
                else {
                    printf("\n[%d]   Done                    ", current->jid);
                }

                while (strcmp(current->command[j], "")) {
                    printf("%s ", current->command[j]);
                    j++;
                }
                printf("\n");
                remove_job_with_pid(pid);   // remove process from the list as soon as it finishes running
                return;
            }
            current = current->next;
        }
    }
    return;
}

int main(int argc, char *argv[]) {

    signal(SIGTTOU, SIG_IGN);       // allows the shell to take back control after some foreground process have terminated.
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, &handle_chld);

    if (argv[1]) {
        script_mode_start(argv[1]);
    }
    else {
        printf("Starting IC Shell\n");
        shell_start();
    }
    return 0;
}