#include <ev.h>
#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <iostream>

#define ASSERT(pred, code, msg) \
if(!(pred)){ \
printf("%s:%d %s\n", __FILE__, __LINE__, (msg)); \
exit(code); \
}

class HTTPServer;

class HTTPException : public std::exception
{
public:
  HTTPException(const char *message)
    : message(message)
  {
  }
  const char* what() const throw() override
  {
    return this->message;
  }
private:
 const char *message;
};

class HTTPConnectionHandler
{
public:
  static void Handle(HTTPServer *server, int fd, struct sockaddr_storage their_addr, socklen_t sin_size)
  {
    new HTTPConnectionHandler(server, fd,  their_addr, sin_size);
  }
private:
  HTTPConnectionHandler(HTTPServer *server, int fd, struct sockaddr_storage their_addr, socklen_t sin_size)
    : server(server), fd(fd), their_addr(their_addr), sin_size(sin_size)
  {
    printf("This is %p\n", this);
    EV_P = ev_default_loop(0);
    this->httpconnectionhandler_watcher.handler = this;
    ev_io_init(&this->httpconnectionhandler_watcher, HTTPConnectionHandler::readable, this->fd, EV_READ);
    ev_io_start(EV_A_ (struct ev_io*)&this->httpconnectionhandler_watcher);
  }
  struct httpconnectionhandler_watcher {
    EV_WATCHER_LIST (ev_io)

    int fd;     /* ro */
    int events; /* ro */

    HTTPConnectionHandler *handler;
  } httpconnectionhandler_watcher;
  void Close()
  {
    printf("Dead\n");
    EV_P = ev_default_loop(0);
    ev_io_stop (EV_A_ (struct ev_io*)&this->httpconnectionhandler_watcher);
    delete this;
  }
  void read()
  {
    puts("Reading data");
    char tmp_buf[4096];
    int n = ::read(fd, tmp_buf, sizeof tmp_buf);
    printf("Read %d bytes\n", n);
    if(n == 0)
    {
      puts("Done reading data.\n");
      Close();
    }
    else if (n < 0)
    {
      puts("Read error!\n");
      Close();
    }
    else
    {
      this->buffer.append(tmp_buf, n);
    }
    std::cout << this->buffer << std::endl;
  }
  static void readable(EV_P_ struct ev_io *generic_watcher, int revents)
  {
    ((struct httpconnectionhandler_watcher*) generic_watcher)->handler->read();
  }
  HTTPServer *server;
  int fd;
  struct sockaddr_storage their_addr;
  socklen_t sin_size;
  std::string buffer;
};

class HTTPServer
{
public:
  HTTPServer(EV_P_ uint16_t port, int backlog)
    : port(port), backlog(backlog), listenfd(-1)
  {
    struct addrinfo hints;
    struct addrinfo *servinfo;
    socklen_t sin_size;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    char port_str[16];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    sprintf(port_str, "%d", port);

    {
      int rv = getaddrinfo(NULL, port_str, &hints, &servinfo);
      if (rv != 0) {
        throw new HTTPException("Could not get address information.");
        return;
      }
    }

    {
      struct addrinfo *cursor;
      // loop through all the results and bind to the first we can
      for(cursor = servinfo; cursor != NULL; cursor = cursor->ai_next) {
        this->listenfd = socket(cursor->ai_family, cursor->ai_socktype, cursor->ai_protocol);
        if (this->listenfd == -1) {
            throw new HTTPException("Failed to create socket.");
            continue;
        }
        if (setsockopt(this->listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            throw new HTTPException("Failed to set socket options.");
            return;
        }
        if (bind(this->listenfd, cursor->ai_addr, cursor->ai_addrlen) == -1) {
            close(this->listenfd);
            continue;
        }
        break;
      }
      if (cursor == NULL)  {
        throw new HTTPException("Could not bind to port.");
        return;
      }
    }

    freeaddrinfo(servinfo); // all done with this structure

    printf("Listening on socket fd %d\n", this->listenfd);
    if (listen(this->listenfd, this->backlog) == -1) {
        throw new HTTPException("Could not listen to socket.");
        return;
    }

  this->httpserver_watcher.server = this;
  ev_io_init(&this->httpserver_watcher, HTTPServer::incomingConnection, this->listenfd, EV_READ);
  ev_io_start(EV_A_ (struct ev_io*)&this->httpserver_watcher);
  }
private:
  int port;
  int backlog;
  int listenfd;
  struct httpserver_watcher {
    EV_WATCHER_LIST (ev_io)

    int fd;     /* ro */
    int events; /* ro */

    HTTPServer *server;
  } httpserver_watcher;
  void HandleConnection(int fd, struct sockaddr_storage their_addr, socklen_t sin_size)
  {
    HTTPConnectionHandler::Handle(this, fd, their_addr, sin_size);
  }
  static void incomingConnection(EV_P_ struct ev_io *generic_watcher, int revents)
  {
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size = sizeof their_addr;
    printf("Accepting on socket fd %d\n", generic_watcher->fd);
    int fd = accept(generic_watcher->fd, (struct sockaddr *)&their_addr, &sin_size);
    printf("Accepted connection on fd %d\n", fd);
    ((struct httpserver_watcher*) generic_watcher)->server->HandleConnection(fd, their_addr, sin_size);
  }
};

int main (void)
{
  // use the default event loop unless you have special needs
  struct ev_loop *loop = ev_default_loop (0);
  HTTPServer server(EV_A_ 9999, 10);
  ev_loop (loop, 0);
  return 0;
}

