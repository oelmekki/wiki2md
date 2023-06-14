/*
 * All int returning functions returns non-zero in case of error, unless
 * explicitly mentioned.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dumper.h"
#include "parser.h"
#include "utils.h"

static void
usage (const char *progname)
{
  printf ("\
%s [-h|--help] <wikitext-file> \n\
\n\
Convert the provided file in mediawiki markup to markdown, printed on stdout. \n\
  ", progname);
}

int
main (int argc, char **argv)
{
  int err = 0;
  node_t *root = NULL;
  char *content = NULL;

  if (argc > 1 && (strncmp (argv[1], "-h", 10) == 0 || strncmp (argv[1], "--help", 10) == 0))
    {
      usage (argv[0]);
      goto cleanup;
    }

  if (argc != 2)
    {
      err = 1;
      usage (argv[0]);
      goto cleanup;
    }

  const char *filename = argv[1];
  err = access (filename, F_OK);
  if (err)
    {
      fprintf (stderr, "No such file : %s\n", filename);
      usage (argv[0]);
      goto cleanup;
    }

  root = xalloc (sizeof *root);
  root->type = NODE_ROOT;
  root->is_block_level = true;
  root->can_have_block_children = true;
  err = parse (filename, root);
  if (err)
    {
      fprintf (stderr, "main.c : main() : error while building representation of file.\n");
      goto cleanup;
    }

  content = xalloc (MAX_FILE_SIZE);
  size_t max_len = MAX_FILE_SIZE - 1;
  char *writing_ptr = content;
  err = dump (root, &writing_ptr, &max_len);
  if (err)
    {
      fprintf (stderr, "main.c : main() : error while dumping markdown.\n");
      goto cleanup;
    }

  puts (content);

  cleanup:
  if (root) free_node (root);
  if (content) free (content);
  return err;
}
