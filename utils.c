#include <stdlib.h>
#include <stdio.h>

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
