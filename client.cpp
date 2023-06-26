#include <cstdint>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include "utils.h"

static int32_t query(int fd, const char *text) {
  uint32_t len = (uint32_t)strlen(text);
  if (len > k_max_msg) {
    return -1;
  }
  
  //copy len of text and text into wbuf
  char wbuf[4 + k_max_msg+ 1];
  memcpy(wbuf, &len, 4); // write len of message
  memcpy(&wbuf[4], text, len); // write message in to buff
  // write message to fd
  if (int32_t err = write_all(fd, wbuf, 4 + len)) {
    return err;
  }


  //read message from fd
  char rbuf[4 + k_max_msg + 1];
  errno = 0;
  int32_t err = read_full(fd, rbuf, 4);
  if (err) {
    if (errno == 0){
      msg("EOF");
    }else {
      msg("read() error");
    }
    return err;
  }

  // copy len of message into the first 4 bytes of rbuf
  memcpy(&len, rbuf, 4);
  if (len > k_max_msg){
    printf("len = %d", len);
    msg("too long");
    return -1;
  }

  // reply 
  err = read_full(fd, &rbuf[4], len);
  if (err) {
    msg("read() error");
      return err;
  }

  rbuf[4 + len] = '\0';
  printf("server says: %s\n", &rbuf[4]);
  return 0;

}


int main (int argc, char *argv[]) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
  int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv){
    die("connect");
  }

  int32_t err = query(fd, "Gort1");
  if (err) {
    close(fd);
    return 0;
  }
  err = query(fd, "Gort2");
  if (err) {
    close(fd);
    return 0;
  }
  err = query(fd, "Gort3");
  close(fd);
  return 0;
}
