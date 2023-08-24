#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "msg.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>

#define MAX_MSG_SIZE 1024

void Usage(char *progname);
int LookupName(char *name, unsigned short port, struct sockaddr_storage *ret_addr, size_t *ret_addrlen);
int Connect(const struct sockaddr_storage *addr, const size_t addrlen, int *ret_fd);

int main(int argc, char **argv) {
  if (argc != 3) {
    Usage(argv[0]);
  }

  unsigned short port = 0;
  if (sscanf(argv[2], "%hu", &port) != 1) {
    Usage(argv[0]);
  }

  struct sockaddr_storage addr;
  size_t addrlen;
  if (!LookupName(argv[1], port, &addr, &addrlen)) {
    Usage(argv[0]);
  }

  int socket_fd;
  if (!Connect(&addr, addrlen, &socket_fd)) {
    Usage(argv[0]);
  }

  char operation[10];
  struct msg m;
  //while loop that runs until client chooses to quit
  while (1) {
    //asking for the operation
    printf("Enter operation (put, get, quit): ");
    scanf("%s", operation);

    //checking for put operation then executing
    if (strcmp(operation, "put") == 0) {
      printf("Enter name: ");
      scanf(" %[^\n]", m.rd.name);
      printf("Enter id: ");
      scanf("%d", &(m.rd.id));
      //setting operation type to put for the msg struct
      m.type = PUT;

      // Send the data
      //write to server socket the msg struct telling the server type is put
      if (write(socket_fd, &m, sizeof(m)) == -1)
      {
        perror("Failed to write message to server");
        break;
      }

      // Read the response from the server
      if (read(socket_fd, &m, sizeof(m)) == -1)
      {
        perror("Failed to read message from server");
        break;
      }
        //checking if put was successful
        if (m.type == SUCCESS)
        {
          printf("put success\n");
        } 
        else {
          printf("%c", m.type);
          printf("put failed\n");
        }
    } 
    //get operation and execution
    else if (strcmp(operation, "get") == 0) 
    {
      printf("Enter id: ");
      //setting the id to get 
      scanf("%d", &(m.rd.id));
      //setting operation type as get
      m.type = GET;

      //writing the operation and the ID to the server
      if (write(socket_fd, &m, sizeof(m)) == -1) {
        perror("Failed to write message to server");
        break;
      }

      // Read the response from the server
      if (read(socket_fd, &m, sizeof(m)) == -1) {
        perror("Failed to read message from server");
        break;
      }
      //checking if the get was successful
      if (m.type == SUCCESS) {
        //if successful prints the ID and Name of the the written ID
        printf("Name: %s, ID: %d\n", m.rd.name, m.rd.id);
      } else {
        printf("get failed\n");
      }
    }
    //checks for operation being quit and then breaks the while loop
    else if (strcmp(operation, "quit") == 0)
    {
      break;
    }
    else
    {
      printf("Invalid operation\n");
    }
  }

  close(socket_fd);
  return EXIT_SUCCESS;
}

void Usage(char *progname) {
  printf("usage: %s hostname port < inputfile\n", progname);
  exit(EXIT_FAILURE);
}

int LookupName(char *name, unsigned short port, struct sockaddr_storage *ret_addr, size_t *ret_addrlen) {
  struct addrinfo hints, *results;
  int retval;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  // Do the lookup by invoking getaddrinfo().
  if ((retval = getaddrinfo(name, NULL, &hints, &results))!= 0) {
fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retval));
return 0;
}

// Iterate over the results, looking for a valid address to use.
int found = 0;
struct addrinfo *cur;
for (cur = results; cur != NULL; cur = cur->ai_next) {
if (cur->ai_family == AF_INET || cur->ai_family == AF_INET6) {
// Found a valid address.
memcpy(ret_addr, cur->ai_addr, cur->ai_addrlen);
*ret_addrlen = cur->ai_addrlen;
  // Set the port number.
  if (ret_addr->ss_family == AF_INET) {
    ((struct sockaddr_in *)ret_addr)->sin_port = htons(port);
  } else {
    ((struct sockaddr_in6 *)ret_addr)->sin6_port = htons(port);
  }

  found = 1;
  break;
}
}

// Clean up and return.
freeaddrinfo(results);
return found;
}

int Connect(const struct sockaddr_storage *addr, const size_t addrlen, int *ret_fd) {
// Create the socket.
int sockfd = socket(addr->ss_family, SOCK_STREAM, 0);
if (sockfd == -1) {
perror("Failed to create socket");
return 0;
}

// Connect to the server.
if (connect(sockfd, (struct sockaddr *)addr, addrlen) == -1) {
perror("Failed to connect to server");
close(sockfd);
return 0;
}

// Set the return value and return success.
*ret_fd = sockfd;
return 1;
}