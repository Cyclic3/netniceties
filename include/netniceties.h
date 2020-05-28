#pragma once

#define _GNU_SOURCE

#include <netinet/in.h>

#include <stdbool.h>

//                             Max ip length + '\0' + "[]" + ':' + "65536"
#define NNICE_SOCKADDR_STRLEN (INET6_ADDRSTRLEN     + 2    +  1  + 5)

int nnice_sockaddr2str(const struct sockaddr_in6* ep, char buf[NNICE_SOCKADDR_STRLEN]);
int nnice_str2sockaddr(struct sockaddr_in6* ep, const char* addr, const char* port);

struct nnice_sockfwd {
  int pipes[2];
  int buf_size;
  int flags;
};

int nnice_sockfwd_init(struct nnice_sockfwd*, int buf_size, bool nonblock);
void nnice_sockfwd_free(struct nnice_sockfwd*);
ssize_t nnice_sockfwd_fwd(const struct nnice_sockfwd*, int from, int to);
