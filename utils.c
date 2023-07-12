#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"


/*
 * Tell if given node is a text node containing an empty string.
 */
bool
is_empty_text_node (node_t *node)
{
  return !node->is_block_level && node->type == NODE_TEXT && strnlen (node->text_content, 1) == 0;
}

/*
 * Safely allocates memory.
 */
void *
xalloc (size_t len)
{
  void *mem = calloc (1, len);
  if (!mem)
    {
      fprintf (stderr, "utils.c : xalloc() : can't allocated memory\n");
      exit (1);
    }

  return mem;
}

/*
 * Safely reallocates memory.
 */
void *
xrealloc (void *mem, size_t msize)
{
  mem = realloc (mem, msize);
  if (!mem)
    {
      fprintf (stderr, "utils.c : xrealloc() : can't allocate %zu bytes of memory\n", msize);
      exit (1);
    }

  return mem;
}
