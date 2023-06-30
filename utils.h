#include <asm-generic/errno-base.h>
#include <cstdint>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <cassert>
#include <fcntl.h>



const size_t k_max_msg = 4096;
static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static int32_t read_full(int fd, char *buf, size_t n){
  while (n > 0) {
    ssize_t rv = read(fd, buf, n);
    if (rv <= 0) {
      return -1;
    }
    assert((size_t)rv <= n);
  
    n -= (size_t)rv;
    buf += rv;
      
  }
  return 0;
}

static int32_t write_all(int fd, char *buf, size_t n){
  while (n > 0) {
    ssize_t rv = write(fd, buf, n);
    if (rv <= 0) {
      return -1;
    }
    assert((size_t)rv <= n);
  
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}

static void fd_set_nb(int fd) {
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno){
    die("fcntl error");
      return;
  }
  flags |= O_NONBLOCK;

  errno = 0;
  (void)fcntl(fd, F_SETFL, flags);
  if (errno) {
    die("fcntl error");
  }
}

enum {
  STATE_REQ = 0,
  STATE_RES = 1,
  STATE_END = 2,
};
















