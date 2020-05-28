#include "netniceties.h"

#include <arpa/inet.h>

#include <errno.h>

#include <fcntl.h>

#include <netinet/tcp.h>
#include <netinet/in.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int nnice_sockaddr2str(const struct sockaddr_in6* ep, char buf[NNICE_SOCKADDR_STRLEN]) {
  char ip_str[INET6_ADDRSTRLEN + 2] = {0};
  uint16_t port;

  // Reset errno so we know what's going on
  errno = 0;
  // Because IPv6 has an annoying syntax, we use [::]:0 notation
  // Set first char
  ip_str[0] = '[';
  // Skip bracket and fill up to length - 2 (INET6_ADDRSTRLEN) chars,
  // leaving the maximum length being INET6_ADDRSTRLEN (as INET6_ADDRSTRLEN - 1 is the longest an output could be)
  inet_ntop(AF_INET6, &((struct sockaddr_in6*)ep)->sin6_addr, ip_str + 1, sizeof(ip_str) - 2);
  if (errno)
    return errno;
  // Set the next char (max INET6_ADDRSTRLEN + 1) as ']'
  ip_str[strlen(ip_str)] = ']';
  // This leaves 1 byte minimum for the null terminator
  if (errno)
    return errno;
  port = ntohs(((struct sockaddr_in6*)ep)->sin6_port);
  // Why is he using snprintf?
  //
  //               because I'm bloody paranoid
  return snprintf(buf, NNICE_SOCKADDR_STRLEN, "%s:%hu", ip_str, port);
}

int nnice_str2sockaddr(struct sockaddr_in6* ep, const char* addr, const char* port) {
  memset(ep, 0, sizeof(*ep));
  // Unified address decoding requires us to keep track of where we put the address
  //
  // We could just replicate the code for each one:
  // that saves a single assignment, but costs many bytes
  void* addr_ptr;

  ep->sin6_family = AF_INET6;
  struct in_addr v4_tmp;
  int family;

  // Simple address type detection
  //
  // We need to check this way around, as ::ffff:0.0.0.0 is valid IPv6 but not v4
  if (strchr(addr, ':')) {
    family = AF_INET6;
    addr_ptr = &ep->sin6_addr;
  }
  else {
    family = AF_INET;
    addr_ptr = &v4_tmp;
  }

  // Now we know what form the address is in, we can fill it out, and error if necessary
  if (!inet_pton(family, addr, addr_ptr))
    return 1;

  // Now we do some IPv6 dark magic
  //
  // DONE: sacrifice a goat to get this thing to actually bind
  if (family == AF_INET) {
    ep->sin6_addr.s6_addr[10] = 0xFF;
    ep->sin6_addr.s6_addr[11] = 0xFF;
    memcpy(&ep->sin6_addr.s6_addr[12], &v4_tmp.s_addr, 4);
  }

  // We need to check it is in range BEFORE we truncate it to an int
  errno = 0;
  unsigned long port_maybe = strtoul(port, NULL, 10);
  // We allow 0 port for some cases, so don't block it
  if (errno || port_maybe >= 65536)
    return 2;

  ep->sin6_port = htons(port_maybe);

  // Wow! Nothing broke!
  return 0;
}

int nnice_sockfwd_init(struct nnice_sockfwd* s, int buf_size, bool nonblock) {
  int err;
  // We need a separate condition for the pipes, as we shouldn't close unopened pipes
  if ((err = nonblock ? pipe2(s->pipes, O_NONBLOCK):pipe(s->pipes)))
    return err;
  // If we fail to set up, clear up and then return an error
  if ((err = fcntl(s->pipes[0], F_SETPIPE_SZ, buf_size)) ||
      (err = fcntl(s->pipes[1], F_SETPIPE_SZ, buf_size))) {
    nnice_sockfwd_free(s);
  }
  // If init succeeds, fill the struct
  else {
    s->buf_size = buf_size;
    s->flags = nonblock ? (SPLICE_F_NONBLOCK | SPLICE_F_MOVE) : SPLICE_F_MOVE;
  }
  // Will contain 0 if we haven't errored so far
  return err;
}
void nnice_sockfwd_free(struct nnice_sockfwd* s) {
  close(s->pipes[0]); close(s->pipes[1]);
}
ssize_t nnice_sockfwd_fwd(const struct nnice_sockfwd* s, int from, int to) {
  ssize_t in_res = splice(from, NULL, s->pipes[1], NULL, s->buf_size, s->flags);
  // If we have an error, return
  if (in_res < 0)
    return in_res;
  // If we have data, pump it though
  if (in_res > 0)
    return splice(s->pipes[0], NULL, to, NULL, s->buf_size, s->flags);
  // Otherwise, just return 0, as no data moved
  return 0;
}
