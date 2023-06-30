#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <cassert>
#include <vector>
#include "utils.h"
#include "poll.h"
#include "sys/epoll.h"


#define MAX_EVENTS 10



static int32_t one_request(int connfd) {

  // 4 bytes header
  char rbuf[4 + k_max_msg + 1];
  errno = 0;
  int32_t err = read_full(connfd, rbuf, 4);
  if (err) {
    if (errno == 0) {
       msg("EOF");
    } else {
       msg("read error");
    }
    return err;
  }

  uint32_t len = 0;
  memcpy(&len, rbuf, 4);
  if (len > k_max_msg) {
    printf("len = %d", len);
    msg("too long");
      return -1;
  }
  
  // request body
  err = read_full(connfd, &rbuf[4], len);
  if (err) {
    msg("read() error");
    return err;
  }

  rbuf[4 + len] = '\0';
  printf("client says: %s\n", &rbuf[4]);

  // reply using proticol
  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)];
  len = (uint32_t)strlen(reply);
  memcpy(wbuf, &len, 4);
  memcpy(&wbuf[4], reply, len);

  return write_all(connfd, wbuf, 4 + len);

}




static bool try_flush_buffer(Conn *conn){
  ssize_t rv = 0;
  do{
    size_t remain = conn->wbuf_size - conn->wbuf_sent;
    rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
  } while(rv < 0 && errno == EINTR);
  if (rv < 0 && errno == EAGAIN) {
    //got EAGAIN, stop
    return false;
  }
  if (rv < 0) {
    msg("write() error");
    conn->state = STATE_END;
    return false;
  }
  conn->wbuf_sent += (size_t)rv;
  assert(conn->wbuf_sent <= conn->wbuf_size);
  if (conn->wbuf_sent == conn->wbuf_size) {
    //response was fully sent, change state back
    conn->state = STATE_REQ;
    conn->wbuf_sent = 0;
    conn->wbuf_size = 0;
    return false;
  }
  return true;

}
static void state_res(Conn *conn){
    while (try_flush_buffer(conn)) {}
}

static bool try_one_request(Conn *conn) {
    //parse request
    if (conn->rbuf_size < 4) {
      // not enough data, will retry next iteration
      return false;
    }
    uint32_t len = 0;
    memcpy(&len, &conn->rbuf, 4);
    if (len > k_max_msg) {
      msg("too long");
      conn->state = STATE_END;
      return false;
    }
    if ( 4 + len > conn->rbuf_size){
      // not enough data, will retry next iteration
      return false;
    }

    // get one request, do something with it
    printf("client says: %.*s\n", len, &conn->rbuf[4]);

    // generating echoing response
    memcpy(&conn->wbuf[0], &len, 4);
    memcpy(&conn->wbuf[4], &conn->rbuf[4], len);
    conn->wbuf_size = 4 + len;

    //remove request from the try_fill_buffer
    //note: frequent memove is inneficent
    //note: production code requires better solution
    size_t remain = conn->rbuf_size - 4 - len;
    if (remain) {
      memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
    }
    conn->rbuf_size = remain;

    //change state
    conn->state = STATE_RES;
    state_res(conn);

    //continue outer loop if the requst was fully proccessed
    return (conn->state == STATE_REQ);


}

static bool try_fill_buffer(Conn *conn) {
  // try to fill try_fill_buffer
  assert(conn->rbuf_size < sizeof(conn->rbuf));
  ssize_t rv = 0;
  do {
    size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
    rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap);
  } while(rv < 0 && errno == EINTR);
  if (rv < 0 && errno == EAGAIN){
      //got EAGAIN, stop
      return false;
  }
  if (rv == 0) {
    if (conn->rbuf_size > 0) {
      msg("Unexpected EOF");
    } else {
      msg("EOF");
    }
    conn->state = STATE_END;
    return false;
  }

  conn->rbuf_size += (size_t)rv;
  assert(conn->rbuf_size <= sizeof(conn->rbuf));
  while (try_one_request(conn)) {}
  return (conn->state == STATE_REQ);
}


static void state_req(Conn *conn){
    while (try_fill_buffer(conn)) {}
}

static void connection_io(Conn *conn){
    if (conn->state == STATE_REQ){
      state_req(conn);
    }else if (conn->state == STATE_RES){
      state_res(conn);
    } else{
      assert(0); // no bueno
    }
}

static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn) {
  if (fd2conn.size() <= (size_t)conn->fd) {
    fd2conn.resize(conn->fd + 1);
  }
  fd2conn[conn->fd] = conn;
}

static int32_t accept_new_conn(std::vector<Conn *> &fd2conn, int fd) {
  //accept
  struct sockaddr_in client_addr = {};
  socklen_t socklen = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
  if (connfd < 0) {
    msg("accept() error");
    return -1;
  }
  //set conn to nonblocking mode
  fd_set_nb(connfd);
  
  //generate a Conn struct for conn
  struct Conn *conn = (struct Conn *)malloc(sizeof(struct Conn));
  if (!conn) {
    close(connfd);
    return -1;
  }
  conn->fd = connfd;
  conn->state = STATE_REQ;
  conn->rbuf_size = 0;
  conn->wbuf_size = 0;
  conn->wbuf_sent = 0;
  conn_put(fd2conn, conn);

  return 0;
}




int main (int argc, char *argv[]) {

  int fd = socket(AF_INET, SOCK_STREAM, 0);

  std::cout << fd << "\n";
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));


  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(0);
  int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
  if (rv) {
    die("bind()");
  }

  rv = listen(fd, SOMAXCONN);
  if (rv) {
    die("listen()");
  }
  
  // map of client connections
  std::vector<Conn *> fd2conn;

  // set the listen fd to nonblocking mode
  fd_set_nb(fd); 

  // event loop
  std::vector<struct pollfd> poll_args;
  

  int event_count;
  struct epoll_event event, events[MAX_EVENTS];
  event.events = EPOLLIN | EPOLLOUT;
  event.data.fd = fd;

  int epolfd = epoll_create(0);
  if (epoll_ctl(epolfd, EPOLL_CTL_ADD, fd, &event)){
      close(epolfd);
      die("Epoll add");
      }

  while (true) {
    
    // listening fd is in first posiiton
    if(epolfd){
      die("epol");
    }

    event_count = epoll_wait(epolfd, events, 10, 3000);
    if (event_count < 0) {
      die("poll"); 
    }

    // connection fds
    for (Conn *conn : fd2conn) {
      if (!conn) {
        continue;
      }
      struct pollfd pfd = {};

      pfd.fd = conn->fd;
      pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
      pfd.events = pfd.events | POLLERR;
      poll_args.push_back(pfd);
    }

 
    for (size_t i = 1; i < event_count; ++i) {
        if (epoll_ctl(epolfd, EPOLL_CTL_ADD, events[i].data.fd, &event)){
            close(epolfd);
            die("Epoll add");
            }

        if (poll_args[i].revents) {
          Conn *conn = fd2conn[poll_args[i].fd];
          connection_io(conn);
          // error or client closed normally
          if (conn->state == STATE_END) {
            fd2conn[conn->fd] = NULL;
            (void)close(conn->fd);
            free(conn);

          }
        
       }
    }
    // try to accept a new connection if the listening fd is active
    if (poll_args[0].revents) {
      (void)accept_new_conn(fd2conn, fd);
      }
    }


  return 0;
}


