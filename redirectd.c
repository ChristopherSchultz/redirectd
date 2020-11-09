#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_LOG_LEVEL 8 /* syslog levels + 8=TRACE */
#define DEFAULT_PORT 8080
#define DEFAULT_BACKLOG 100
#define DEFAULT_RESPONSE_CODE 302
#define DEFAULT_ACCEPT_TIMEOUT 10000

#define trace(...) \
            do { if (loglevel >= 8) fprintf(stderr, "TRACE: " __VA_ARGS__); } while (0)

/* Global 'quit' variable for signal handling */
volatile sig_atomic_t quit = 0;
volatile sig_atomic_t loglevel = DEFAULT_LOG_LEVEL;

void signal_handler(int sig) {
  if(SIGUSR1 == sig) {
    /* Technically, this isn't safe as printf() and friends aren't reentrant */
    trace("Received SIGURS1\n");
    quit = 1;
  }
}

int main(int argc, char *argv[], char *env[]) {
  int opt_on = 1;

  /* Socket stuff */
  int sockfd;
  int clientfd;
  int port = DEFAULT_PORT;
  int backlog = DEFAULT_BACKLOG;
  struct sockaddr_in addr;
  struct sockaddr_in client;
  socklen_t clientlen = sizeof client;

  /* poll stuff */
  int acceptTimeout = DEFAULT_ACCEPT_TIMEOUT;
  struct pollfd readfds[1];

  /* HTTP stuff */
  int responseCode = DEFAULT_RESPONSE_CODE;
  char *responseMessage = "Found";
  char *redirect = "https://github.com/ChristopherSchultz/redirectd";
  char *response;
  int rv;

  /*
     Set up the HTTP response.
     This will be the same for all clients, so we can pre-compute it
     and re-use it over and over again.
  */

  /* "HTTP/1.0 " + code + " " + msg + "\n" + "Location: " + location + "\n" + "Content-Length: 0\n" + "Connection: close\n" + "\n" + \0 */
  int responselen = 9 + ceil(log10(responseCode)) + 1 + strlen(responseMessage) + 1 + 10 + strlen(redirect) + 1 + 18 + 18 + 1 + 1;
  response = malloc(responselen * sizeof(char));
  if(NULL == response) {
    perror("malloc");

    return 1;
  }

  trace("Allocated %d bytes for response\n", responselen);

  rv = snprintf(response, responselen, "HTTP/1.0 %d %s\nLocation: %s\nContent-Length: 0\nConnection: close\n\n",
    responseCode, responseMessage, redirect);

  if(responselen < rv) {
    fprintf(stderr, "Message overflowed by %d characters\n", (rv - responselen));

    return 1;
  }

  trace("Built stock response\n");

  /*
    Set up the socket.

    Create the socket, configure the fd for re-use, bind to a port, and listen.
  */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(-1 == sockfd) {
    perror("socket");

    return 1;
  }

  trace("Created socket on fd %d\n", sockfd);

  rv = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                 (char *)&opt_on, sizeof(opt_on));
  if (rv < 0)
  {
    perror("setsockopt");
    close(sockfd);

    return 1;
  }

  bzero(&addr, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if(-1 == bind(sockfd, (void*)&addr, sizeof addr)) {
    perror("bind");
    close(sockfd);

    return 1;
  }

  trace("Bound fd %d to socket\n", sockfd);

  if(-1 == listen(sockfd, backlog)) {
    perror("listen");
    close(sockfd);

    return 1;
  }

  trace("Listening on port %d\n", port);

  /*
    Install a signal handler to gracefully quit.
  */
  signal(SIGUSR1, signal_handler);

  trace("Installed signal handler for SIGUSR1\n");

  /*
    Get ready to poll().

    Use poll() instead of select() because it is slightly more flexible and
    "modern" than select().

    poll/select is used because we want to be able to handle signals
    modifying the state of this program (e.g. setting "quit" to TRUE).
  */
  bzero(readfds, sizeof(readfds));
  readfds[0].fd = sockfd;
  readfds[0].events = POLLIN;
  trace("poll timeout is %d\n", acceptTimeout);

  /*
    Accept connections until we are quitting.
  */
  while(0 == quit) {
    if(-1 == (rv = poll(readfds, 1, acceptTimeout))) {
      if(EINTR == errno) {
        trace("poll was interrupted, quit=%d\n", quit);
      } else {
        perror("poll");
        close(sockfd);

        return 2;
      }
    }

    trace("poll() returned %d\n", rv);

    if(0 < rv) {
      /*
        Handle a single incoming connection.
      */
      clientfd = accept(sockfd, (void*)&client, &clientlen);

      trace("accept() returned client fd %d\n", clientfd);

      if(-1 == clientfd) {
        perror("accept");

        return 2;
      }

      /*
        Dump the stock response to the client, ignoring any input, and close
        the connection.
      */
      rv = write(clientfd, response, responselen);

      trace("Wrote %d bytes to response\n", rv);

      close(clientfd);
    }
  }
  trace("Closing fd %d\n", sockfd);
  close(sockfd);
}

