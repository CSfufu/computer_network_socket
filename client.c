#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_MAX_SIZE 2048
#define ARG_MAX_NUM 3

pthread_cond_t cond_t = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread_id;

int socket_fd = -1;
char buf[BUFFER_MAX_SIZE];

void PrintUsage();
static int ParseInput(char *input, char **args);
static int Connect(char *hostname, int port);
static void Disconnect(int *socket_fd);
static int IsFromClient(char *buf);
void *CommunicationHandler(void *arg);
static int Recv();


int main() {
  PrintUsage();
  char *args[ARG_MAX_NUM] = {0};
  int parse_res;

  while (1) {
    printf("> ");
    fgets(buf, BUFFER_MAX_SIZE, stdin);
    parse_res = ParseInput(buf, args);
    if (parse_res == 1) {
      printf("[ERROR] Parse Error!\n");
      continue;
    }

    if (strcmp(args[0], "-c") == 0) {
      if (socket_fd != -1) {
        printf("[ERROR] Already Connected!\n");
        continue;
      }
      if (args[1] == NULL || args[2] == NULL) {
        printf("[ERROR] Lack Args!\n");
        continue;
      }
      int port = atoi(args[2]);
      if (port == 0) {
        printf("[ERROR] Invalid Port!\n");
        continue;
      }
      socket_fd = Connect(args[1], port);
      if (socket_fd == -1) {
        printf("[ERROR] Connect Fail\n");
      } else {
        printf("[OK] Connect Success!\n");
      }
      // start a thread to listen
      pthread_create(&thread_id, NULL, CommunicationHandler, NULL);
    } else if (strcmp(args[0], "-e") == 0) {
      if (socket_fd != -1) {
        close(socket_fd);
        pthread_cancel(thread_id);
      }
      pthread_mutex_destroy(&mtx);
      pthread_cond_destroy(&cond_t);
      break;
    } else if (strcmp(args[0], "-q") == 0) {
      if (socket_fd == -1) {
        printf("[ERROR] Unconnected!\n");
        continue;
      }
      Disconnect(&socket_fd);
      pthread_cancel(thread_id);
    } else if (strcmp(args[0], "-t") == 0) {
      if (socket_fd == -1) {
        printf("[ERROR] Unconnected!\n");
        continue;
      }
      sprintf(buf, "time");
      write(socket_fd, buf, 5);
      Recv();
      printf("%s", buf);
    } else if (strcmp(args[0], "-n") == 0) {
      if (socket_fd == -1) {
        printf("[ERROR] Unconnected!\n");
        continue;
      }
      sprintf(buf, "name");
      write(socket_fd, buf, 5);
      Recv();
      printf("%s", buf);
    } else if (strcmp(args[0], "-l") == 0) {
      if (socket_fd == -1) {
        printf("[ERROR] Unconnected!\n");
        continue;
      }
      sprintf(buf, "list");
      write(socket_fd, buf, 5);
      Recv();
      printf("%s", buf);
    } else if (strcmp(args[0], "-s") == 0) {
      if (args[1] == NULL) {
        printf("[ERROR] Lack Args!\n");
        continue;
      }
      sprintf(buf, "%s %s", args[0], args[1]);
      write(socket_fd, buf, BUFFER_MAX_SIZE);
      Recv();
      if (strcmp(buf, "[HOST] Invalid Id!\n") == 0) {
        printf("%s", buf);
        continue;
      }

      while (1) {
        printf("Enter your message: ");
        fgets(buf, BUFFER_MAX_SIZE, stdin);
        write(socket_fd, buf, BUFFER_MAX_SIZE);
        if (strcmp(buf, "exit\n") == 0)
          break;
      }

    } else {
      printf("[ERROR] Invalid Instruction!\n");
    }
  }
}

void PrintUsage() {

  for (int i = 0; i < 80; i++) {
    printf("-");
  }
  printf("\n");

  printf("%-*s\n", 60,
         "------------------------------------ Client "
         "------------------------------------");

  for (int i = 0; i < 80; i++) {
    printf("-");
  }
  printf("\n");


  printf("\n");
  printf("%-*s\n", 60, "Usage:");
  printf("\n");

  printf("+-------------------------------------------------+-------------+\n");
  printf("|                  Command                        |   Operate   |\n");
  printf("+-------------------------------------------------+-------------+\n");
  printf("| connect [IP] [PORT]: connect to server          |      -c     |\n");
  printf("| quit: close the server                          |      -q     |\n");
  printf("| name: show client name                          |      -n     |\n");
  printf("| exit: shutdown the connection                   |      -t     |\n");
  printf("| list: show the clients connected to the server  |      -l     |\n");
  printf("| send [ID]: start to send [MSG] to client [ID]   |      -s     |\n");
  printf("+-------------------------------------------------+-------------+\n");

  printf("\n");

  for (int i = 0; i < 80; i++) {
    printf("-");
  }
  printf("\n");
}

static int ParseInput(char *input, char **args) {
    int index = 0;
    char *start = input;
    
    for(int i = 0; input[i]; i++) {
        if(input[i] == '\n') {
            input[i] = '\0';
            break;
        }
    }
    
    while(*start && index < 3) {
        while(*start == ' ') start++;
        if(!*start) break;
        
        args[index++] = start;
        
        while(*start && *start != ' ') start++;
        
        if(*start) *start++ = '\0';
    }
    
    while(index < 3) {
        args[index++] = NULL;
    }
    
    return 0;
}

static int Connect(char *hostname, int port) {
  int socket_fd = -1;
  unsigned long int host;
  struct sockaddr_in serv_addr;

  if ((host = inet_addr(hostname)) == -1) {
    printf("[ERROR] Invalid Host!\n");
    return -1;
  }

  if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    printf("[ERROR] Socket Error!\n");
    return -1;
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = host;

  if ((connect(socket_fd, (struct sockaddr *)&serv_addr,
               sizeof(struct sockaddr))) == -1) {
    return -1;
  }

  return socket_fd;
}

static void Disconnect(int *socket_fd) {
  if (*socket_fd != -1) {
    close(*socket_fd);
    printf("[OK] Connection closed.\n");
    *socket_fd = -1;
  } else {
    printf("[ERROR] Invalid socket descriptor.\n");
  }
}

static int IsFromClient(char *buf) {
  char buf2[BUFFER_MAX_SIZE];
  strncpy(buf2, buf, BUFFER_MAX_SIZE);
  char *p = strchr(buf2, ' ');
  if (p != NULL) {
    *p = '\0';
    if (strcmp(buf2, "[HOST]") != 0) { // client
      return 1;
    }
  }
  return 0;
}

void *CommunicationHandler(void *arg) {
  int recv_num;
  char buf_listen[BUFFER_MAX_SIZE];

  for(;;) {
    recv_num = read(socket_fd, buf_listen, BUFFER_MAX_SIZE);
    if (IsFromClient(buf_listen)) {
      printf("%s", buf_listen);
    } else { // server
      pthread_mutex_lock(&mtx);
      strncpy(buf, buf_listen, BUFFER_MAX_SIZE);
      pthread_cond_signal(&cond_t);
      pthread_mutex_unlock(&mtx);
    }
  }
  return NULL;
}

static int Recv() {
  pthread_mutex_lock(&mtx);
  pthread_cond_wait(&cond_t, &mtx);
  pthread_mutex_unlock(&mtx);
  return 0;
}

