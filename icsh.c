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

#define _POSIX_SOURCE
#define	MAX_CMD_CHAR 256

char last_cmd[MAX_CMD_CHAR];    // a string holder for last user's input
char cmd[MAX_CMD_CHAR];         // a string holder for user's input
pid_t pid;
int exit_code;

struct job_elem {
    int jid;                    // job ID
    pid_t pid;                  // process ID
    char* state;                // state of a process (E.g. bg, fg)
    char** command;             // command
    struct job_elem* next;      // next node (for linked list)
};
typedef struct job_elem job; 

int job_no = 1;

job* head = NULL;

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
        else if (strcmp(current->state, "fg") == 0) {}
        current = current->next;
    }
}

void run_foreground(int jid) {
    int j = 0;
    int ppid = getpid();
    job* current = head;
    while (current != NULL) {
        if (current->jid == jid && strcmp(current->state, "bg") == 0) {
            int status;
            current->state = "fg";
            
            while (strcmp(current->command[j], "")) {
                if (strcmp(current->command[j], "&")) {
                    printf("%s ", current->command[j]);
                }
                j++;
            }
            j = 0;
            printf("\n");

            tcsetpgrp(STDIN_FILENO, current->pid);
            kill(current->pid, SIGCONT);
            waitpid(current->pid, &status, 0);
            remove_job_with_pid(current->pid);
            tcsetpgrp(STDIN_FILENO, ppid);
        }
        current = current->next;
    }
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

void redirection_checker(char** input) {
    int index = 0;
    while (input[index] != NULL) {

        // if the string is "<" and the file name is not null
        if (strcmp(input[index], "<") == 0 && input[index + 1] != NULL) {
            int file = open(input[index+1], O_RDONLY);      // read only
            if (file == -1) {
                printf("Error opening file\n");
                exit(errno);
            }
            dup2(file, STDIN_FILENO);
            input[index] = NULL;
            close(file);
            break;
        }

        // if the string is ">" and the file name is not null
        else if (strcmp(input[index], ">") == 0 && input[index + 1] != NULL) {
            int file = open(input[index+1], O_WRONLY | O_CREAT, 0777);      // write only + create a file if not exist
            if (file == -1) {
                printf("Error opening file\n");
                exit(errno);
            }
            dup2(file, STDOUT_FILENO);
            input[index] = NULL;
            close(file);
            break;
        }
        index++;
    }
}

void assign_last_cmd(char* input) {
    input[strcspn(input, "\n")] = 0;         // removing trailing new line from input 
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
        char num = tokens[1][1];
        if (tokens[1][0] == '%' && !isalpha(num)) {
            int jid = num - '0';
            run_foreground(jid);
        }
        
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

            redirection_checker(tokens);             // check for > and < in the input

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

// modiefied from: https://docs.oracle.com/cd/E19455-01/806-4750/signals-7/index.html
void handle_chld() {
    int wstat;
    pid_t pid;
    
    pid = wait3(&wstat, WNOHANG, (struct rusage *)NULL );
    if (pid == 0) {
        return;
    }
    else if (pid == -1) {
        return;
    }
    else {
        int j;
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
                remove_job_with_pid(pid);
                return;
            }
            current = current->next;
        }
    }
}

int main(int argc, char *argv[]) {

    signal(SIGTTOU, SIG_IGN);       // allows the shell to take back control after some foreground process have terminated.
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
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