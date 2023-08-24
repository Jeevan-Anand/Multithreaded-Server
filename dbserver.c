#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>
#include "msg.h"

void Usage(char *progname);
int  Listen(char *portnum, int *sock_family);
void *ThreadHandleClient(void *arg);

void *ThreadHandleClient(void *arg)
{
    int c_fd = *((int*)arg);

    // Print out client connection.
    printf("\nNew client connection \n" );
    // Open the file descriptor / create the "database"
    int32_t fd = open("database", O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);

    // Loop, reading data and handling it, until the client closes the connection.
    while (1)
    {
        //creating the msg struct
        struct msg req;
        //reading from the client socket
        ssize_t res = read(c_fd, &req, sizeof(req));
        //check if client is disconnected
        if (res == 0)
        {
            printf("[The client disconnected.] \n");
            break;
        }

        //check for error of client connection
        if (res == -1)
        {
            if ((errno == EAGAIN) || (errno == EINTR))
                continue;
            printf("Error on client socket: %s\n", strerror(errno));
            break;
        }
        
        //switch statement for put and get by using the type input from the client socket
        switch (req.type)
        {
            //code for put
            case PUT:
            {
                //set the offset at which to write to in the file
                lseek(fd, 0, SEEK_END);
                //write to the fd 
                res = write(fd, &req.rd, sizeof(req.rd));
                //creating a struct to write back responses to the client
                struct msg response;
                //checking if there is any errors reading from the socket
                if (res <= 0)
                {
                    printf("Error reading from socket");
                    response.type = FAIL;
                } else {
                  response.type = SUCCESS;
                }
                //write back to the client 
                write(c_fd, &response, sizeof(response));
                break;
            }

            //code for get
            case GET: 
            {
                //creating the id we will be searching for in the get
                int id;
                id = req.rd.id;
                //creating a flag
                bool found = false;
                //creating and offset for the end of the file descriptor inorder to break out of the while loop if there is no ID found
                off_t endDb = lseek(fd, 0, SEEK_END);
                //moving through the fd 
                int move = 0;
                //setting to the beginning of the fd so we can parse through the entire fd
                lseek(fd, move, SEEK_SET);
                //reading through the fd
                while (read(fd, &req.rd, sizeof(req.rd)) != -1) 
                {
                    //checking if the id is found within the file descriptor
                    if (req.rd.id == id) 
                    {
                        found = true;
                        break;
                    }
                    move++;
                    //if the id is not found then break out of the while loop
                    if(lseek(fd, move * sizeof(req.rd), SEEK_SET) >= endDb)
                      break;
                }
                //if the id is found then the server writes a SUCCESS message to the client write FAIL if id is not found
                if (found)
                {
                    req.type = SUCCESS;
                }
                else
                {
                    req.type = FAIL;
                }
                //write SUCCESS or FAIL to the client
                write(c_fd, &req, sizeof(req));
                break;
            }

            default:
                break;
        }
    }
    //close the client socket
    close(c_fd);
    //close the file descriptor
    close(fd);
    //exit the thread
    pthread_exit(NULL);
}

int main(int argc, char **argv) 
{
  // Expect the port number as a command line argument.
  if (argc != 2) {
    Usage(argv[0]);
  }

  int sock_family;
  int listen_fd = Listen(argv[1], &sock_family);
  if (listen_fd <= 0) {
    // We failed to bind/listen to a socket.  Quit with failure.
    printf("Couldn't bind to any addresses.\n");
    return EXIT_FAILURE;
  }

  // Loop forever, accepting a connection from a client and doing
  // an echo trick to it.
      while (1) {
        struct sockaddr_storage caddr;
        socklen_t caddr_len = sizeof(caddr);

        int *client_fd = malloc(sizeof(int)); // Allocate memory for client_fd to pass it to the thread
        *client_fd = accept(listen_fd,
                            (struct sockaddr *)(&caddr),
                            &caddr_len);

        if (*client_fd < 0) {
            free(client_fd); // Make sure to free the memory if accept() fails

            // ... your existing code ...
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, ThreadHandleClient, client_fd) != 0)
        {
            printf("Failed to create thread\n");
            return EXIT_FAILURE;
        }
        // Optionally, you may want to detach the newly created thread
        // so that its resources are automatically freed when it finishes.
        pthread_detach(thread_id);
    }

  // Close socket
  close(listen_fd);
  return EXIT_SUCCESS;
}

void Usage(char *progname) {
  printf("usage: %s port \n", progname);
  exit(EXIT_FAILURE);
}


int Listen(char *portnum, int *sock_family)
{

  // Populate the "hints" addrinfo structure for getaddrinfo().
  // ("man addrinfo")
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;       // IPv6 (also handles IPv4 clients)
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  // getaddrinfo() returns a list of
  // address structures via the output parameter "result".
  struct addrinfo *result;
  int res = getaddrinfo(NULL, portnum, &hints, &result);

  // Did addrinfo() fail?
  if (res != 0)
  {
	printf( "getaddrinfo failed: %s", gai_strerror(res));
    return -1;
  }

  // Loop through the returned address structures until we are able
  // to create a socket and bind to one.  The address structures are
  // linked in a list through the "ai_next" field of result.
  int listen_fd = -1;
  struct addrinfo *rp;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    listen_fd = socket(rp->ai_family,
                       rp->ai_socktype,
                       rp->ai_protocol);
    if (listen_fd == -1) {
      // Creating this socket failed.  So, loop to the next returned
      // result and try again.
      //printf("socket() failed:%d \n ", strerror(errno));
      listen_fd = -1;
      continue;
    }

    // Configure the socket; we're setting a socket "option."  In
    // particular, we set "SO_REUSEADDR", which tells the TCP stack
    // so make the port we bind to available again as soon as we
    // exit, rather than waiting for a few tens of seconds to recycle it.
    int optval = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    // Try binding the socket to the address and port number returned
    // by getaddrinfo().
    if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      // Bind worked!  Print out the information about what
      // we bound to.
      //PrintOut(listen_fd, rp->ai_addr, rp->ai_addrlen);

      // Return to the caller the address family.
      *sock_family = rp->ai_family;
      break;
    }

    // The bind failed.  Close the socket, then loop back around and
    // try the next address/port returned by getaddrinfo().
    close(listen_fd);
    listen_fd = -1;
  }

  // Free the structure returned by getaddrinfo().
  freeaddrinfo(result);

  // If we failed to bind, return failure.
  if (listen_fd == -1)
    return listen_fd;

  // Success. Tell the OS that we want this to be a listening socket.
  if (listen(listen_fd, SOMAXCONN) != 0) {
    //printf("Failed to mark socket as listening:%d \n ", strerror(errno));
    close(listen_fd);
    return -1;
  }

  // Return to the client the listening file descriptor.
  return listen_fd;
}