#include <stdlib.h> 
#include <stdio.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/wait.h> 
#include <fcntl.h> 
#include <signal.h> 
#include <pwd.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define CMD_READLINE_BUFSIZE 1024
#define CMD_STREAM_DELIM ";\n"
#define CMD_TOK_DELIM " \t\r\a"
#define CMD_TOK_BUFSIZE 64
#define TYPE_CMD_STREAM 1
#define TYPE_CMD_CMD 2

void sigint_handler(int signal); 
void shell_loop(int run_status); 
void *get_current_path(); 
char *cmd_read_line(); 
char **cmd_split(char *line, int split_type); 
int cmd_execute_cmd(char **cmd);
void cd(char *pth); 
int handle_batch_file(char *file);


/**
    Main function to decide that if shell should run in
    normal mode or batch mode

    @param number of arguments and arguments themselves 
    from shell
    @return exit code EXIT_SUCCESS or EXIT_FAILURE
*/
int main(int argc, char **argv) {
  signal(SIGINT, sigint_handler);
  if (argc == 2) { // Batch mode
    int status; // status if batch mode exec "quit"
    status = handle_batch_file(argv[1]); // handle batch file
    shell_loop(status);  // continue running normal mode shell with quit status from handle_batch_file
  } else if (argc == 1) { // Normal mode
    shell_loop(1); // run shell in normal mode
  } else {
    fprintf(stderr, "SHELL: Error running shell. Usage %s <cmd_batch file>\n", argv[0]); // bad run shell command
  }
  printf("Exited\n"); // after done shell
  return EXIT_SUCCESS; 
}

/**
    handle CTRL+C. if pressed, prints "Exit ungracefully"
    string

    @param signal code
    @return void
*/
void sigint_handler(int signal) { // handle CTRL+C
  fprintf(stderr, "\nSHELL: Exited ungracefully\n");
  exit(EXIT_FAILURE);
}

/**
    main shell loop to act as normal mode shell
    continuously take command from user until
    "quit" is caught
    note that line and cmd_stream would be something
    like this
    
        cmd_stream to be splitted to cmd
        _______________|_____________
       |   |     |         |        |
      ls; pwd; cd ..; echo Hello; quit
      |______________________________|
                      |
                    line

    @param quit_status use to indicate if batch mode caught
    "quit". if caught, this var must be 0 else 1
    @return void
*/
void shell_loop(int quit_status) {
  int status = quit_status;

  // if batch mode quit
  if (status == 0) { 
    return;
  }

  // path array for print prompt text
  char path[1024]; 

  do {
    // line to get from user
    char *line; 

    // commands from slicing line with ;
    char **cmd_stream; 

    // retrive current working dir 
    getcwd(path, 1024); 
    
    // do trimming path
    int i; 
    for (i = strlen(path)-1; path[i] != '/'; i--); 
    char prompt_path[1024]; 
    strncpy(prompt_path, path + i + 1, strlen(path) - i); 
    if (strlen(prompt_path) == 0) {
      prompt_path[0] = '/'; 
      prompt_path[1] = 0; 
    }

    // print prompt
    printf("[%s]> ", prompt_path); 

    // read line from user
    line = cmd_read_line();

    // slice line into commands by ; 
    cmd_stream = cmd_split(line, TYPE_CMD_STREAM); 

    // loops do all command in cmd_stream
    int cmd_stream_indx = 0; 
    while (cmd_stream[cmd_stream_indx] != NULL) {
      
      // array of single command
      char **cmd;
      int stdinFd = -1, stdoutFd = -1, inpFd, optFd;

      // slice one of cmd_stream into command and arguments
      cmd = cmd_split(cmd_stream[cmd_stream_indx], TYPE_CMD_CMD);
      
      // process file redirection by looping through command
      int j = 0;
      while(cmd[j] != NULL) {

        // if found output redirect symbol and output file
        if (strcmp(cmd[j], ">") == 0 && cmd[j+1] != NULL) {

          // if we never redirect stdout before in this command
          if (stdoutFd == -1) {

            // try to create new output file
            if ((optFd = open(cmd[j+1], O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {

              // error handling
              perror(cmd[j+1]);

              // make this command null
              cmd[0] = 0;
              break;
            }

            // if no error, redirect stdout to that file
            // copy stdout fd
            stdoutFd = dup(1);
            dup2(optFd, 1);

            // null out the '>'
            cmd[j] = 0;
          } else { // else user use some redirect before in this command
            fprintf(stderr, "SHELL: Error. Detect more than one output redirect.\n");

            // make this command null
              cmd[0] = 0;
              break;
          }
        } 
        // if found output append redirect symbol and output file
        else if (strcmp(cmd[j], ">>") == 0 && cmd[j+1] != NULL) { 

          // if we never redirect stdout before in this command
          if (stdoutFd == -1) {

            // try to append or create output file
            if ((optFd = open(cmd[j+1], O_CREAT|O_WRONLY|O_APPEND, 0644)) < 0) {

              // error handling
              perror(cmd[j+1]);

              // make this command null
              cmd[0] = 0;
              break;
            }

            // if no error, redirect stdout to that file
            // copy stdout fd
            stdoutFd = dup(1);
            dup2(optFd, 1);

            // null out the '>'
            cmd[j] = 0;
          } else {  // else user use some redirect before in this command
            fprintf(stderr, "SHELL: Error. Detect more than one output redirect.\n");

            // make this command null
              cmd[0] = 0;
              break;
          }
        }
        // if found input redirect symbol and input file
        else if (strcmp(cmd[j], "<") == 0 && cmd[j+1] != NULL && strcmp(cmd[j], ">") != 0) { 

          // if we never redirect stdin before in this command
          if (stdinFd == -1) {
            
            // try to open input file
            if ((inpFd = open(cmd[j+1], O_RDONLY)) < 0) {

              // errir notify
              perror(cmd[j+1]);

              // make this command null
              cmd[0] = 0;
              break;
            }

            // if no error, redirect that file to stdin
            // copy stdin fd
            stdinFd = dup(0);
            dup2(inpFd, 0);

            // null out the '>'
            cmd[j] = 0;
          } else { // else user use some redirect before in this command
            fprintf(stderr, "SHELL: Error. Detect more than one input redirect.\n");

            // make this command null
              cmd[0] = 0;
              break;
          }
        } 
        // if redirect symbol not followed by filename
        else if ( (strcmp(cmd[j], ">") == 0 && cmd[j+1] == NULL) || (strcmp(cmd[j], ">>") == 0 && cmd[j+1] == NULL) || (strcmp(cmd[j], "<") == 0 && ( cmd[j+1] == NULL || strcmp(cmd[j], ">") == 0)) ){

          // error notify
          fprintf(stderr, "SHELL: Error, unable to redirect file\n");

          // make this command null
          cmd[0] = 0;
        }
        j++;
      }

      if (cmd[0] != 0) {
        // execute command and get quit status
        status = cmd_execute_cmd(cmd);

        // if command is quit
        if (status == 0)  
          // break shell loop
          break;
      }

      // if redirected stdout, change it back
      if (stdoutFd != -1) {

        // close output file
        close(optFd);

        // copy stdout fd back
        dup2(stdoutFd, 1);
      }

      // if redirected stdin, change it back
      if (stdinFd != -1) {

        // close input file
        close(inpFd);

        // copy stdin fd back
        dup2(stdinFd, 0);
      }
      cmd_stream_indx++; 
    }

    // free dynamic allocation variable
    free(line); 
    free(cmd_stream); 
  }while (status != 0); 
}

/**
    read line from user in normal mode

    @param none
    @return line read from user
*/
char *cmd_read_line() {
  char *line = NULL; 

  // have getline allocate a buffer for us
  ssize_t bufsize = 0; 
  
  // use getline() to do job for us
  getline( &line,  &bufsize, stdin); 
  return line; 
}

/**
    split line into commands or command into command and
    arguments depends on split_type

    @param line/command to be splitted and split_type
    which are CMD_STREAM_DELIM and CMD_TOK_DELIM
    @return array commands or array of command and
    its arguments
*/
char **cmd_split(char *line, int split_type) {

  // buffer size to allocate
  int bufsize = CMD_TOK_BUFSIZE, position = 0; 

  // allocate arrays of splitted
  char **tokens = malloc(bufsize *sizeof(char *)); 
  char *token; 
  
  // handle error where cannot allocate memory
  if ( ! tokens) {
    fprintf(stderr, "SHELL: allocation error\n"); 
    exit(EXIT_FAILURE); 
  }

  // choose delimeter based on split_type
  char *delim = (split_type == TYPE_CMD_STREAM ? CMD_STREAM_DELIM : CMD_TOK_DELIM); 

  // split line or command based on split_type
  token = strtok(line, delim); 

  while (token != NULL) {
    
    // split strings
    tokens[position] = token; 

    /**
      detect "\ " for file or folder name
      for example, "cd OPERATING\ SYSTEM"
      will get splitted into "cd" and "OPERATING SYSTEM"
      instead of "cd", "OPERATING\" and "SYSTEM"
    */
    if (position != 0) {
      char *lastTok = tokens[position - 1];

      // check if previous token ends with '\'
      if (lastTok[strlen(lastTok) -1] == '\\') {

        // if true, do concat current token to previous
        strcpy(&lastTok[strlen(lastTok) - 1], " ");
        strcat(lastTok, tokens[position]);
        position--;
      }
    }
    position++; 

    // if we are out of buffer, reallocate buffer
    if (position >= bufsize) {
      bufsize += CMD_TOK_BUFSIZE; 
      tokens = realloc(tokens, bufsize *sizeof(char *)); 
      if ( ! tokens) {
        fprintf(stderr, "SHELL: allocation error\n"); 
        exit(EXIT_FAILURE); 
      }
    }

    // continut split until ends of input string
    token = strtok(NULL, delim); 
  }

  // ends array with NULL to be able to use with execvp
  tokens[position] = NULL; 
  return tokens; 
}

/**
    execute given command, special treat "cd", "quit"
    and "exit"

    @param array of command which is splitted into command
    itself and its arguments
    @return quit status. if command is quit, return 0 else 1
*/
int cmd_execute_cmd(char **cmd) {

  // detect "cd" command
  if (strcmp(cmd[0], "cd") == 0) {

    char *path = cmd[1];
    
    // if user enter only cd command, go to home dir
    if (path == 0) {

      // if $HOME is set, use it
      if ((path = getenv("HOME")) == NULL) {

        /** 
          if not, use getuid to get the user id of the current user 
          and then getpwuid to get the password entry (which includes 
          the home directory) of that user
        */
        path = getpwuid(getuid())->pw_dir;
      }
    }

    // change working dir
    cd(path); 
    return 1;
  }

  // detect "quit" command
  if (strcmp(cmd[0], "quit") == 0 || strcmp(cmd[0], "quit ") == 0) {

    // return quit
    return 0;
  } 
  // detect "exit" command
  else if (strcmp(cmd[0], "exit") == 0 || strcmp(cmd[0], "exit ") == 0) {

    // we don't use "exit" in this shell, so notify an error
    fprintf(stderr, "SHELL: Please use quit instead.\n"); 

    // return not quit
    return 1;
  }

  // for another command, fork and execute it
  int pid = fork(); 
  int exitc = 0; 
  if (pid == 0) {
    execvp(cmd[0], cmd); 
    perror(cmd[0]);
    exit(0);
  } else {
    wait( & exitc); 
  }

  // return not quit
  return 1;
}

/**
    change working dir with no need to enter full path

    @param path of directory to be changed, could be either
    absolute path or just directory name
    @return void
*/
void cd(char *pth) {
    char path[1024]; 

    // changing status
    int status;

    strcpy(path, pth); 

    char cwd[1024]; 

    // if entered only dir name
    if (pth[0] != '/') {
      // make it absolute path and cd to it
      getcwd(cwd, sizeof(cwd)); 
      strcat(cwd, "/"); 
      strcat(cwd, path); 
      status = chdir(cwd); 
    } 
    // else if already absolute path, just cd to it
    else {
      status = chdir(pth); 
    }

    // if no dir named in input, show error
    if (status != 0) {
      perror(pth);
    }
}

/**
    handle command files

    @param name of command file
    @return quit status. if command file contain "quit"
    command, it will be 0 else 1
*/
int handle_batch_file(char *file) {
  int newfd, status = 1;
  int bufsize = CMD_READLINE_BUFSIZE;

  // try to open command file
  if ((newfd = open(file, O_RDONLY)) < 0) {
    perror(file);
    exit(EXIT_FAILURE);
  }

  printf("Reading command from file %s\n", file);
  
  // batch reading status
  int batch_finish = 1;


  while (batch_finish) {
    
    // allocate line buffer
    char *buffer = malloc(sizeof(char) * CMD_READLINE_BUFSIZE);
    
    // end of line status
    int read_status = 1;
    int position = 0;
    char c;
    int ret = 1;
    char **cmd_stream;

    // read one char at a time
    while(read_status) {
      int rd;
      rd = read(newfd, &c, 1); 

      // if detect EOF
      if (rd == 0) {

        // ends line
        buffer[position] = '\0';

        // set eol status to end
        read_status = 0;

        // set batch file status to EOF
        batch_finish = 0;
      } 
      // if detect newline
      else if (c == '\n') {
        
        // ends line
        buffer[position] = '\0';

        // set eol status to end
        read_status = 0;
      } 
      // if detect normal char
      else {

        // appends line buffer
        buffer[position] = c;
      }
      position++;

      // if line buffer not enough, reallocate it
      if (position >= bufsize) {
        bufsize += CMD_READLINE_BUFSIZE;
        buffer = realloc(buffer, bufsize);
      }
    }

    // split line into commands
    cmd_stream = cmd_split(buffer, TYPE_CMD_STREAM); 


    // loop done all commands
    int cmd_stream_indx = 0; 
    while (cmd_stream[cmd_stream_indx] != NULL) {
      char **cmd;
      int quit_cmd_status; 

      // split command into command itself and its arguments
      cmd = cmd_split(cmd_stream[cmd_stream_indx], TYPE_CMD_CMD); 
      quit_cmd_status = cmd_execute_cmd(cmd);

      // if detect "quit"
      if (quit_cmd_status == 0) {

        // set quit status to quit
        status = 0;

        // ends reading batch file
        batch_finish = 0;
        break;
      }
      cmd_stream_indx++; 
    }
    free(cmd_stream); 
    free(buffer);
  }
  close(newfd);
  return status;
}
