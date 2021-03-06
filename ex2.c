#include "stdio.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#define SIZE 50
#define length 510

int pipe_c = 0;           // Count PIPE
int redi_counter = 0;     // Count Redirection
int curr_cmd_count = 0;   // count current commands only
double total_count = 0;   // Total commands counter
double command_count = 0; // Total commands chars counter.

int find_pipe(char *input, char **output);
int find_redirections(char *input);
char **divid_to_words(char *string);
void write_to_log(const char *input, const char *path);
int count_words(char **input);
void error(char *msg);
char **parse_str(char *input, int ind, int i, int size);
int find_red(char *str);

int main(int argc, char **argv)
{
    char user_IN[length];             // user commands input
    char copy[length], copy2[length]; // copy of user_IN
    char buffer[SIZE];                // path string
    int i = 0;

    getcwd(buffer, SIZE); // get the path string

    struct passwd *user_name;

    user_name = getpwuid(getuid()); // get the user name string

    while (strcmp("done", user_IN) != 0)
    {
        printf("%s@%s>", user_name->pw_name, buffer); // output of the shell

        fgets(user_IN, length, stdin); // get input from the user

        //write the commands to log
        write_to_log(user_IN, argv[1]);
        redi_counter += find_red(user_IN);
       
        //if fgets() return NULL- continue the loop.
        if (user_IN == NULL)
            continue;

        user_IN[strlen(user_IN) - 1] = '\0'; // delete '\n' from the string

        // if no input but enter, repeat till get input.
        if (user_IN[0] == '\0')
            continue;

        strcpy(copy, user_IN);
        strcpy(copy2, user_IN);
        
        char *word = strtok(user_IN, " ");
        // if there only spaces on the string- continue the loop.
        if (word == NULL)
            continue;

        total_count++;
        command_count += strlen(copy);

        //unallowed command 'cd'
        if (strcmp(word, "cd") == 0)
        {
            printf("command not supported (Yet)\n");
            continue;
        }
        
        //If user type "done"- end the shell
        if (strcmp("done", user_IN) == 0)
            break;

        //find_redirections(copy);
        //Find pipe in the string.
        char *divided_str[2];
        if ((find_pipe(copy2, divided_str)) == 1)
            continue;

        //If no pipe, continue.
        char **array_of_cmd;
        array_of_cmd = divid_to_words(copy);
        int size = count_words(array_of_cmd);
        // send the son procces command to execute.
        pid_t x;
        if ((x = fork()) < 0)
            error("Fork Failed!\n");
        if (x == 0) //Son
        {
            //Check if there is redirections: 
            if (find_redirections(copy) == 1)
                exit(1);
            //If no redirections: 
            if (execvp(array_of_cmd[0], array_of_cmd) < 0)
                error("Execvp Failed!\n");
            exit(1);
        }
        //return to parent and continue the loop.
        wait(NULL);

        for (i = 0; i < size; i++)
            free(array_of_cmd[i]);
        free(array_of_cmd);
    }

    // print summary
    printf("Num of commands: %d\n", (int)total_count);
    printf("Total length of all commands: %d\n", (int)command_count);
    printf("Averege length of all commands: %f\n", (double)(command_count / total_count));
    printf("Number of command that include pipe: %d\n", pipe_c);
    printf("Number of command that include redirection: %d\n", redi_counter);
    printf("See you Next Time !\n");
    return 0;
}
//This function write the user input into log file.
void write_to_log(const char *input, const char *path)
{
    int saved_stdout = dup(1);
    int fd = open(path, O_RDWR | O_CREAT | O_APPEND , S_IRWXG | S_IRWXO | S_IRWXU);
    if (fd < 0)
        error("Open Failed!\n");

    int value = dup2(fd, STDOUT_FILENO);
    if (value < 0)
        error("Dup Failed!\n");
    fprintf(stdout, "%s\n", input);

    close(fd);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
}
//Find pipe in the user input. If execute pipe, check if there redirection too.
int find_pipe(char *input, char **output)
{
    //find the pipe if exist and divide the string to before and after.
    for (size_t i = 0; i < 2; i++)
    {
        output[i] = strsep(&input, "|");
        if (output[i] == NULL)
            break;
    }
    if (output[1] != NULL)
    { // that's mean there is pipe and the string is divided.
        pipe_c++;
        char **after = NULL, **before = NULL;
        int fd[2], value = 0, red_f = 0, size_b = 0, size_a = 0;
        pid_t _first, _second;
        if (pipe(fd) < 0)
            error("Pipe Failed.\n");
        if ((_first = fork()) < 0)
            error("Fork Failed.\n");

        if (_first == 0) // First son
        {  
            before = divid_to_words(output[0]);
            size_b = count_words(before);
            close(fd[0]);
            value = dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);
            if (value < 0)
                error("Dup Failed!\n");
            if ((execvp(before[0], before)) < 0)
                error("Execvp Failed!\n");
            exit(0);
        }
        if ((_second = fork()) < 0)
            error("Fork Failed.\n");

        if (_second == 0) //Second son.
        {
            close(fd[1]);
            value = dup2(fd[0], STDIN_FILENO);
            if (value < 0)
                error("Dup Failed.\n");
            close(fd[0]);
            //Find redirections
            red_f = find_redirections(output[1]);
            if (!red_f)//If no redirections 
            {
                after = divid_to_words(output[1]);
                size_a = count_words(after);
                if ((execvp(after[0], after)) < 0)
                    error("Execvp Failed.\n");
                exit(0);
            }
            exit(0);
        }
        close(fd[1]);
        close(fd[0]);
        wait(NULL);
        wait(NULL);
        int j;
        //Free memory and end the function.
        for (j = 0; j < size_b; j++)
            if (before[j] != NULL)
                free(before[j]);
       
        if (!red_f)
        {
            for (j = 0; j < size_a; j++)
                if (after[j] != NULL)
                    free(after[j]);
         
        }//Return 1 to start new cmd loop.
        return 1;
    }
    //If no pipe, return 0 to continue the current cmd loop.
    return 0;
}
//Find redirection and execute the command.
int find_redirections(char *input)
{
    int i = 0, i_size = 0, redi_case = 0;
    char **parsed = NULL;

    //Fine the size of the string.
    while (input[i] != '\0')
    {
        i_size++;
        i++;
    }
    //Search for redirection.
    i = 0;
    while (i < i_size)
    {
        if (input[i] == '>')
        {
            if (input[i + 1] == '>')
            {
                parsed = parse_str(input, i, 2, i_size);
                redi_case = 2;
                break;
            }
            parsed = parse_str(input, i, 1, i_size);
            redi_case = 1;
            break;
        }
        if (input[i] == '<')
        {
            parsed = parse_str(input, i, 1, i_size);
            redi_case = 3;
            break;
        }
        if (input[i] == '2' && input[i + 1] == '>')
        {
            parsed = parse_str(input, i, 2, i_size);
            redi_case = 4;
            break;
        }
        i++;
    }
    //If redi_case != 0, its mean there is redirection cmd.
    if (redi_case != 0)
    {
        int fd = 0, value = 0;
        char **word_arr = divid_to_words(parsed[0]);
        char **file_path = divid_to_words(parsed[1]); 
        int size = count_words(word_arr);

        // i: 1 = >, 2 = >>, 3 = 2>, 4 = <
        switch (redi_case)
        {
        case 1: // CASE OF >
            if((fd = open(file_path[0], O_RDWR | O_CREAT | O_TRUNC,
                      0777)) < 0)
                error("Open Failed!\n");
            if((value = dup2(fd, STDOUT_FILENO)) < 0)
                error("Dup Failed!\n");
            close(fd);
            if (execvp(word_arr[0], word_arr) < 0)
                error("Execvp Failed!\n");
            exit(0);
            break;
        case 2: // CASE OF >>

            if((fd = open(file_path[0], O_RDWR | O_CREAT | O_APPEND,
                      0777)) < 0)
                error("Open Failed!\n");

            if((value = dup2(fd, STDOUT_FILENO)) < 0)
                error("Dup Failed!\n");
            close(fd);
            if (execvp(word_arr[0], word_arr) < 0)
                error("Execvp Failed!\n");
            exit(0);
            break;

        case 3: // CASE OF <

           if((fd = open(file_path[0], O_RDWR | O_CREAT | O_APPEND,
                      0777)) < 0)
                error("Open Failed!\n");

            if((value = dup2(fd, STDIN_FILENO)) < 0)
                error("Dup Failed!\n");
            close(fd);
            if (execvp(word_arr[0], word_arr) < 0)
                error("Execvp Failed!\n");
            exit(0);
            break;
        case 4: // CASE OF 2>

            if((fd = open(file_path[0], O_RDWR | O_CREAT | O_APPEND,
                      0777)) < 0)
                error("Open Failed!\n");
           if((value = dup2(fd, STDERR_FILENO)) < 0)
                error("Dup Failed!\n");
            close(fd);
            if (execvp(word_arr[0], word_arr) < 0)
                perror("Execvp Failed!\n");
            exit(0);
            break;
        default:
            break;
        }
        int j = 0;
        for (j = 0; j < size; j++)
            if (word_arr[j] != NULL)
                free(word_arr[j]);
        free(word_arr);

        for (j = 0; j < 2; j++)
            if (parsed[j] != NULL)
                free(parsed[j]);

        free(file_path[0]);
        free(file_path);
        return 1;
    }
    return 0;
}
//Divide user input without change it (use copies).
char **divid_to_words(char *string)
{
    char **output = NULL;
    char *copy = (char *)malloc(sizeof(char) * strlen(string));
    assert(copy);
    strcpy(copy, string);
    char *copy2 = (char *)malloc(sizeof(char) * strlen(string));
    assert(copy2);
    strcpy(copy2, string);

    char *word = strtok(copy2, " ");

    curr_cmd_count = 1; // reset this to 1 since its requiered only for the current commands.

    //check how many words contain the string from the user.
    while (word != NULL)
    {
        curr_cmd_count++;
        word = strtok(NULL, " ");
    }
    //array for words
    output = (char **)malloc(sizeof(char *) * curr_cmd_count);
    assert(output);

    char *word_copy = strtok(copy, " ");

    // fill the arr_cmd
    int i = 0;
    while (word_copy != NULL)
    {
        output[i] = (char *)malloc(sizeof(char) * strlen(word_copy));
        assert(output[i]);
        strcpy(output[i], word_copy);
        i++;
        word_copy = strtok(NULL, " ");
    }
    output[i] = NULL;
    return output;
}
//Return the size of cmd array
int count_words(char **input)
{
    int i = 0, counter = 0;
    while (input[i] != NULL)
    {
        counter++;
        i++;
    }
    return counter;
}
//Print error msg and exit.
void error(char *msg)
{
    perror(msg);
    exit(1);
}
//Divide string by index (use to divide string with redirection's chars.)
char **parse_str(char *input, int ind, int i, int size)
{
    char **parsed = (char **)malloc(sizeof(char *) * 2);
    assert(parsed);

    parsed[0] = (char *)malloc(sizeof(char) * ind);
    assert(parsed[0]);
    memset(parsed[0], '\0', ind);
    strncpy(parsed[0], input, ind);

    parsed[1] = (char *)malloc(sizeof(char) * (size - ind - i));
    assert(parsed[1]);
    memset(parsed[1], '\0', size - ind - i);
    strncpy(parsed[1], input + ind + i, size - ind - i);

    return parsed;
}

int find_red(char *str){
    int i = 0;
    while (str[i] != '\0'){
        if ((str[i]=='>') || (str[i]=='<') || ((str[i]=='2') &&(str[i+1] == '>')))
            return 1;
        i++;
    }
    return 0;
}