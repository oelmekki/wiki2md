#ifndef _DUMPER_H_
#define _DUMPER_H_

#include "parser.h"

typedef struct {
  node_t *node;
  char **writing_ptr;
  const char *start_of_buffer;
  size_t *max_len;
} dumping_params_t;

int dump (dumping_params_t *params);

#endif
