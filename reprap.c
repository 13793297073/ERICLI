#include "reprap.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "serial.h"
#include "gcode.h"

typedef struct blocknode {
  struct blocknode *next;
  void *cbdata;
  const char *block;
  size_t blocksize;
} blocknode;

struct rr_dev_t {
  rr_proto proto;
  int fd;
  unsigned long lineno;

  blocknode *sendhead[RR_PRIO_COUNT];
  blocknode *sendtail[RR_PRIO_COUNT];
  char sendbuf[SENDBUFSIZE];
  rr_prio sending_prio;
  size_t sendbuf_fill;
  size_t bytes_sent;
  
  char *recvbuf;
  size_t recvbufsize;
  size_t recvbuf_fill;
  
  blocknode **sentcache;
  size_t sentcachesize;
  
  rr_sendcb onsend;
  rr_recvcb onrecv;
  rr_boolcb want_writable;
  void *onsend_data, *onrecv_data, *ww_data;
};

int rr_open(rr_dev *deviceptr,
            rr_proto proto,
            rr_sendcb onsend, void *onsend_data,
            rr_recvcb onrecv, void *onrecv_data,
            rr_boolcb want_writable, void *ww_data,
            const char *port, long speed,
            size_t resend_cache_size) {
  *deviceptr = malloc(sizeof(struct rr_dev_t));
  rr_dev device = *deviceptr;

  device->sentcache = calloc(resend_cache_size, sizeof(blocknode*));
  device->sentcachesize = resend_cache_size;
  unsigned i;
  for(i = 0; i < resend_cache_size; ++i) {
    device->sentcache[i] = NULL;
  }
  for(i = 0; i < RR_PRIO_COUNT; ++i) {
    device->sendhead[i] = NULL;
    device->sendtail[i] = NULL;
  }
  device->recvbuf = calloc(RECVBUFSIZE, sizeof(char));
  device->recvbufsize = RECVBUFSIZE;
  
  device->fd = serial_open(port, speed);
  if(device->fd < 0) {
    return -1;
  }
  
  device->proto = proto;
  device->onsend = onsend;
  device->onsend_data = onsend_data;
  device->onrecv = onrecv;
  device->onrecv_data = onrecv_data;
  device->want_writable = want_writable;
  device->ww_data = ww_data;
  device->lineno = 0;
  return 0;
}

int rr_close(rr_dev device) {
  /* Deallocate buffers */
  unsigned i;
  for(i = 0; i < RR_PRIO_COUNT; ++i) {
    blocknode *j = device->sendhead[i];
    while(j != NULL) {
      blocknode *next = j->next;
      free(j);
      j = next;
    }
    free(device->sendhead[i]);
  }
  free(device->recvbuf);
  for(i = 0; i < device->sentcachesize; ++i) {
    if(device->sentcache[i]) {
      free(device->sentcache[i]);
    }
  }
  free(device->sentcache);
  
  /* Close FD */
  return close(device->fd);
}

ssize_t fmtblock_simple(char *buf, const char *block) {
  char work[SENDBUFSIZE+1];
  int result;
  result = snprintf(work, SENDBUFSIZE+1, "%s\r\n", block);
  if(result >= SENDBUFSIZE+1) {
    return RR_E_BLOCK_TOO_LARGE;
  }
  size_t len = (result > SENDBUFSIZE) ? SENDBUFSIZE : result;
  strncpy(buf, work, len);

  return len;
}

ssize_t fmtblock_fived(char *buf, const char *block, unsigned long line) {
  char work[SENDBUFSIZE+1];
  int result;
  char checksum = 0;
  result = snprintf(work, GCODE_BLOCKSIZE+1, "N%ld %s", line, block);
  if(result >= GCODE_BLOCKSIZE+1) {
    return RR_E_BLOCK_TOO_LARGE;
  }
  ssize_t i;
  for(i = 0; i < result; ++i) {
    checksum ^= work[i];
  }
  /* TODO: Is this whitespace needed? */
  result = snprintf(work, SENDBUFSIZE+1, "N%ld %s *%d\r\n", line, block, checksum);
  if(result >= SENDBUFSIZE+1) {
    return RR_E_BLOCK_TOO_LARGE;
  }

  size_t len = (result > SENDBUFSIZE) ? SENDBUFSIZE : result;
  strncpy(buf, work, len);

  return len;
}

ssize_t fmtblock(rr_dev device, const char *block, size_t nbytes) {
  char *terminated = calloc(nbytes+1, sizeof(char));
  strncpy(terminated, block, nbytes);
  terminated[nbytes] = '\0';

  ssize_t result;
  switch(device->proto) {
  case RR_PROTO_SIMPLE:
    result = fmtblock_simple(device->sendbuf, terminated);
    break;

  case RR_PROTO_FIVED:
    result = fmtblock_fived(device->sendbuf, terminated, device->lineno);
    break;

  default:
    result = RR_E_UNSUPPORTED_PROTO;
    break;
  }

  free(terminated);
  return result;
}

void rr_enqueue(rr_dev device, rr_prio priority, void *cbdata, const char *block, size_t nbytes) {
  blocknode *node = malloc(sizeof(blocknode));
  node->next = NULL;
  node->cbdata = cbdata;
  node->block = block;
  node->blocksize = nbytes;
  
  if(!device->sendhead[priority]) {
    device->sendhead[priority] = node;
  } else {
    device->sendtail[priority]->next = node;
  }
}

int rr_handle_readable(rr_dev device) {
  /* Grow receive buffer if it's full */
  if(device->recvbuf_fill == device->recvbufsize) {
    device->recvbuf = realloc(device->recvbuf, 2*device->recvbufsize);
  }

  ssize_t result;
  size_t scan = device->recvbuf_fill;
  size_t start = 0;
  do {
    result = read(device->fd, device->recvbuf + device->recvbuf_fill, device->recvbufsize - device->recvbuf_fill);
  } while(result < 0 && errno == EINTR);

  if(result < 0) {
    return result;
  }

  /* Scan for complete reply */
  for(; scan < device->recvbuf_fill; ++scan) {
    if(0 == strncmp(device->recvbuf + scan, REPLY_TERMINATOR, strlen(REPLY_TERMINATOR))) {
      /* We have a terminator */
      handle_reply(device, device->recvbuf + start, scan - start);
      scan += strlen(REPLY_TERMINATOR);
      start = scan;
    }
  }

  /* Move incomplete reply to beginning of buffer */
  memmove(device->recvbuf, device->recvbuf+start, device->recvbuf_fill - start);

  return 0;
}

int rr_handle_writable(rr_dev device) {
  int prio;
  ssize_t result;
  if(device->bytes_sent > SENDBUFSIZE) {
    /* Last block is gone; prepare to send a new block */
    for(prio = RR_PRIO_COUNT - 1; prio >= 0; --prio) {
      blocknode *node = device->sendhead[prio];
      if(node) {
        /* We have a block to send! Get it ready. */
        device->bytes_sent = 0;
        result = fmtblock(device, node->block, node->blocksize);
        if(result < 0) {
          /* Make at least some attempt to leave device in a
           * recoverable state */
          device->sendbuf_fill = 0;
          return result;
        }
        device->sendbuf_fill = result;
        device->sending_prio = prio;
      }
    }
  }

  /* Perform write */
  do {
    result = write(device->fd, device->sendbuf + device->bytes_sent, device->sendbuf_fill - device->bytes_sent);
  } while(result < 0 && errno == EINTR);
  
  if(result < 0) {
    return result;
  }

  device->bytes_sent += result;

  if(device->bytes_sent == device->sendbuf_fill) {
    blocknode *node = device->sendhead[device->sending_prio];
    /* We've sent the complete block. */
    device->onsend(device, device->onsend_data, node->cbdata, device->sendbuf, device->sendbuf_fill);
    device->sendhead[device->sending_prio] = node->next;
    free(node);
    /* Indicate that we're ready for the next. */
    device->bytes_sent = SENDBUFSIZE + 1;
  }

  return result;
}
