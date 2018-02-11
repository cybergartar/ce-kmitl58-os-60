#include <stdlib.h> 
#include <stdio.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/wait.h> 
#include <fcntl.h> 
#include <signal.h> 
#include <ncurses.h> 
#include <readline/readline.h> 

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
char **cmd_split_line(char *line, int split_type); 
int cmd_execute_cmd(char **cmd);
void cd(char *pth); 
int handle_batch_file(char *file);

int main(int argc, char **argv) {
  signal(SIGINT, sigint_handler);
  if (argc == 2) {
    int status;
    status = handle_batch_file(argv[1]);
    shell_loop(status); 
  } else if (argc == 1) {
    shell_loop(1); 
  } else {
    printf("SHELL: Error running shell. Usage %s <cmd_batch file>\n", argv[0]);
  }
  printf("Exited\n");
  return EXIT_SUCCESS; 
}

void sigint_handler(int signal) {
  printf("\nExited ungracefully\n");
  exit(EXIT_FAILURE);
}

void shell_loop(int run_status) {
  int status = run_status; 
  char path[1024]; 
  if (status == 0) {
    return;
  }
  do {
    char *line; 
    char **cmd_stream; 
    getcwd(path, 1024); 
    int i; 
    for (i = strlen(path)-1; path[i] != '/'; i--); 
    char prompt_path[1024]; 
    strncpy(prompt_path, path + i + 1, strlen(path) - i); 
    if (strlen(prompt_path) == 0) {
      prompt_path[0] = '/'; 
      prompt_path[1] = 0; 
    }
    printf("[%s]> ", prompt_path); 

    line = cmd_read_line(); 
    cmd_stream = cmd_split_line(line, TYPE_CMD_STREAM); 

    int cmd_stream_indx = 0; 
    while (cmd_stream[cmd_stream_indx] != NULL) {
      char **cmd; 
      int stdinFd = -1, stdoutFd = -1, inpFd, optFd;
      cmd = cmd_split_line(cmd_stream[cmd_stream_indx], TYPE_CMD_CMD);
      int j = 0;
      while(cmd[j] != NULL) {
        if (strcmp(cmd[j], ">") == 0 && cmd[j+1] != NULL) {
          stdoutFd = dup(1);
          if ((optFd = open(cmd[j+1], O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {
            perror(cmd[j+1]);
            cmd[0] = 0;
          }
          dup2(optFd, 1);
          cmd[j] = 0;
        } else if (strcmp(cmd[j], "<") == 0 && cmd[j+1] != NULL && strcmp(cmd[j], ">") != 0) {
          stdinFd = dup(0);
          if ((inpFd = open(cmd[j+1], O_RDONLY)) < 0) {
            printf("SHELL: Error, no file named %s\n", cmd[j+1]);
            cmd[0] = 0;
          }
          dup2(inpFd, 0);
          cmd[j] = 0;
        } else if ( (strcmp(cmd[j], ">") == 0 && cmd[j+1] == NULL) || (strcmp(cmd[j], "<") == 0 && ( cmd[j+1] == NULL || strcmp(cmd[j], ">") == 0)) ){
          printf("SHELL: Error, unable to redirect file\n");
          cmd[0] = 0;
        }
        j++;
      }
      status = cmd_execute_cmd(cmd);
      if (status == 0) 
        break;

      if (stdoutFd != -1) {
        close(optFd);
        dup2(stdoutFd, 1);
      }
      if (stdinFd != -1) {
        close(inpFd);
        dup2(stdinFd, 0);
      }
      cmd_stream_indx++; 
    }
    free(line); 
    free(cmd_stream); 
  }while (status != 0); 
}

char *cmd_read_line() {
  char *line = NULL; 
  ssize_t bufsize = 0; // have getline allocate a buffer for us
  getline( &line,  &bufsize, stdin); 
  return line; 
}

char **cmd_split_line(char *line, int split_type) {
  int bufsize = CMD_TOK_BUFSIZE, position = 0; 
  char **tokens = malloc(bufsize *sizeof(char *)); 
  char *token; 
  
  if ( ! tokens) {
    fprintf(stderr, "shell: allocation error\n"); 
    exit(EXIT_FAILURE); 
  }

  char *delim = (split_type == TYPE_CMD_STREAM ? CMD_STREAM_DELIM : CMD_TOK_DELIM); 

  token = strtok(line, delim); 
  while (token != NULL) {
    tokens[position] = token; 
    if (position != 0) {
      char *lastTok = tokens[position - 1];
      if (lastTok[strlen(lastTok) -1] == '\\') {
        strcpy(&lastTok[strlen(lastTok) - 1], " ");
        strcat(lastTok, tokens[position]);
        position--;
      }
    }
    position++; 

    if (position >= bufsize) {
      bufsize += CMD_TOK_BUFSIZE; 
      tokens = realloc(tokens, bufsize *sizeof(char *)); 
      if ( ! tokens) {
        fprintf(stderr, "shell: allocation error\n"); 
        exit(EXIT_FAILURE); 
      }
    }

    token = strtok(NULL, delim); 
  }
  tokens[position] = NULL; 
  return tokens; 
}

int cmd_execute_cmd(char **cmd) {
  if (strcmp(cmd[0], "cd") == 0) {
    cd(cmd[1]); 
    return 1;
  }

  if (strcmp(cmd[0], "quit") == 0 || strcmp(cmd[0], "quit ") == 0) {
    return 0;
  } else if (strcmp(cmd[0], "exit") == 0 || strcmp(cmd[0], "exit ") == 0) {
    printf("SHELL: Please use quit instead.\n"); 
    return 1;
  }

  int pid = fork(); 
  int exitc = 0; 
  if (pid == 0) {
    execvp(cmd[0], cmd); 
    perror(cmd[0]);
    exit(0);
  } else {
    wait( & exitc); 
  }
  return 1;
}

void cd(char *pth) {
    char path[1024]; 
    int status; 
    strcpy(path, pth); 

    char cwd[1024]; 
    if (pth[0] != '/') {// true for the dir in cwd
      getcwd(cwd, sizeof(cwd)); 
              strcat(cwd, "/"); 
              strcat(cwd, path); 
              status = chdir(cwd); 
    } else {//true for dir w.r.t. /
      status = chdir(pth); 
    }

    if (status != 0) {
      printf("SHELL: no directory named %s.\n", path); 
    }
}

int handle_batch_file(char *file) {
  int newfd, status = 1;
  int bufsize = CMD_READLINE_BUFSIZE;
  if ((newfd = open(file, O_RDONLY)) < 0) {
    perror(file);
    exit(EXIT_FAILURE);
  }
  printf("Reading command from file %s\n", file);
  int batch_finish = 1;
  while (batch_finish) {
    char *buffer = malloc(sizeof(char) * CMD_READLINE_BUFSIZE);
    int read_status = 1;
    int position = 0;
    char c;
    int ret = 1;
    char **cmd_stream;
    while(read_status) {
      int rd;
      rd = read(newfd, &c, 1); 
      if (rd == 0) {
        buffer[position] = '\0';
        read_status = 0;
        batch_finish = 0;
      } else if (c == '\n') {
        buffer[position] = '\0';
        read_status = 0;
      } else {
        buffer[position] = c;
      }
      position++;

      if (position >= bufsize) {
        bufsize += CMD_READLINE_BUFSIZE;
        buffer = realloc(buffer, bufsize);
      }
    }
    cmd_stream = cmd_split_line(buffer, TYPE_CMD_STREAM); 

    int cmd_stream_indx = 0; 
    while (cmd_stream[cmd_stream_indx] != NULL) {
      char **cmd;
      int quit_cmd_status; 
      cmd = cmd_split_line(cmd_stream[cmd_stream_indx], TYPE_CMD_CMD); 
      quit_cmd_status = cmd_execute_cmd(cmd);
      if (quit_cmd_status == 0) {
        status = 0;
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
