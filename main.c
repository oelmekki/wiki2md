/*
 * All int returning functions returns non-zero in case of error, unless
 * explicitly mentioned.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#define MAX_LINE_LENGTH 10000
#define MAX_FILE_SIZE 500000

enum {
  NODE_ROOT,
  NODE_TEXT,
  NODE_PARAGRAPH,
  NODE_HEADING,
  NODE_STRONG,
  NODE_EMPHASIS,
  NODE_STRONG_AND_EMPHASIS,
  NODE_BULLET_LIST,
  NODE_BULLET_LIST_ITEM,
  NODE_NUMBERED_LIST,
  NODE_NUMBERED_LIST_ITEM,
  NODE_HORIZONTAL_RULE,
  NODE_NOWIKI,
  NODE_DEFINITION_LIST,
  NODE_DEFINITION_LIST_TERM,
  NODE_DEFINITION_LIST_DEFINITION,
  NODE_INDENT,
  NODE_PREFORMATTED_TEXT,
  NODE_PREFORMATTED_CODE_BLOCK,
  NODE_LINK,
  NODE_TABLE,
};

typedef struct _node_t {
  int type;
  char *text_content;
  struct _node_t **children;
  size_t children_len;
  struct _node_t *parent;
  struct _node_t *last_child;
  struct _node_t *previous_sibling;
  struct _node_t *next_sibling;
} node_t;

/*
 * Add a child to a parent's memory.
 */
static void
append_child (node_t *parent, node_t *child)
{
  parent->children_len++;
  parent->children = xrealloc (parent->children, parent->children_len * sizeof (*child));
  parent->children[parent->children_len - 1] = child;

  child->parent = parent;
  parent->last_child = child;

  if (parent->children_len > 1)
    parent->children[parent->children_len - 2]->next_sibling = child;
}

/*
 * Add text to a text node.
 */
static int
append_text (node_t *text_node, const char *text)
{
  if (text_node->type != NODE_TEXT)
    {
      fprintf (stderr, "append_text() : error : trying to add text to a non text node.\n");
      return 1;
    }

  size_t len = text_node->text_content ? strlen (text_node->text_content) : 0;
  text_node->text_content = xrealloc (text_node->text_content, len + strlen (text) + 1);
  snprintf (text_node->text_content + len, strlen (text) + 1, "%s", text);

  return 0;
}

/*
 * Cleanup memory for a node.
 */
static void
free_node (node_t *node)
{
  if (node->text_content)
    free (node->text_content);

  if (node->children)
    {
      for (size_t i = 0; i < node->children_len; i++)
        free_node (node->children[i]);

      free (node->children);
    }

  free (node);
}

/*
 * Write text buffer to text node.
 */
static int
flush_text_buffer (node_t *current_node, char *buffer, char **buffer_ptr)
{
  int err = 0;

  if (!current_node->children || current_node->last_child->type != NODE_TEXT)
    {
      node_t *text_node = xalloc (sizeof *text_node);
      text_node->type = NODE_TEXT;
      append_child (current_node, text_node);
    }

  err = append_text (current_node->last_child, buffer);
  if (err)
    {
      fprintf (stderr, "flush_text_buffer() : error while append text to text node.\n");
      return err;
    }

  *buffer_ptr = buffer;

  return err;
}

/*
 * Parse mediawiki paragraph.
 *
 * A paragraph is a block of text ending in "\n\n".
 *
 */
static int
parse_paragraph_end (node_t **current_node, char **reading_ptr, char *buffer, char **buffer_ptr)
{
  int err = 0;

  if (strncmp (*reading_ptr, "\n\n", 2) != 0)
    return err;

  err = flush_text_buffer (*current_node, buffer, buffer_ptr);
  if (err)
    {
      fprintf (stderr, "parse_strong_and_emphasis() : error while append flushing text buffer.\n");
      return err;
    }

  if (strlen (*reading_ptr) > 2)
    {
      node_t *new_paragraph = xalloc (sizeof *new_paragraph);
      new_paragraph->type = NODE_PARAGRAPH;
      append_child ((*current_node)->parent, new_paragraph);
      *current_node = new_paragraph;
    }

  *reading_ptr += 2;

  return err;
}

/*
 * Parse mediawiki tag adding both strong and emphasis :
 *
 *    '''''content'''''
 */
static int
parse_strong_and_emphasis (node_t **current_node, char **reading_ptr, char *buffer, char **buffer_ptr)
{
  int err = 0;

  if (strncmp (*reading_ptr, "'''''", 5) != 0)
    return err;

  err = flush_text_buffer (*current_node, buffer, buffer_ptr);
  if (err)
    {
      fprintf (stderr, "parse_strong_and_emphasis() : error while append flushing text buffer.\n");
      return err;
    }

  if ((*current_node)->type == NODE_STRONG_AND_EMPHASIS)
    *current_node = (*current_node)->parent;
  else
    {
      node_t *strong_and_emphasis = xalloc (sizeof *strong_and_emphasis);
      strong_and_emphasis->type = NODE_STRONG_AND_EMPHASIS;
      append_child (*current_node, strong_and_emphasis);
      *current_node = strong_and_emphasis;
    }

  *reading_ptr += 5;

  return err;
}

/*
 * Build a representation of the document, so that it's
 * then easier to serialize.
 *
 * The result is stored in `root`. You should provide
 * the memory for it, and clean its content with `free_node()`.
 */
static int
build_representation (const char *filename, node_t *root)
{
  int err = 0;
  char content[MAX_FILE_SIZE] = {0};
  FILE *file = NULL;

  file = fopen (filename, "r");
  if (!file)
    {
      fprintf (stderr, "build_representation() : can't read file %s\n", filename);
      goto cleanup;
    }

  size_t content_len = fread (content, 1, MAX_FILE_SIZE - 1, file);
  if (content_len == MAX_FILE_SIZE - 1)
    fprintf (stderr, "build_representation() : warning : input file has probably been truncated due to its size.\n");

  node_t *current_node = xalloc (sizeof *current_node);
  current_node->type = NODE_PARAGRAPH;
  append_child (root, current_node);
  char *reading_ptr = content;
  char buffer[BUFSIZ] = {0};
  char *buffer_ptr = buffer;

  while (true)
    {
      err = parse_paragraph_end (&current_node, &reading_ptr, buffer, &buffer_ptr);
      if (err)
        {
          fprintf (stderr, "build_representation() : error while parsing paragraph end.\n");
          return err;
        }

      err = parse_strong_and_emphasis (&current_node, &reading_ptr, buffer, &buffer_ptr);
      if (err)
        {
          fprintf (stderr, "build_representation() : error while parsing strong and emphasis.\n");
          return err;
        }

      if (buffer_ptr - buffer == BUFSIZ - 1)
        {
          err = flush_text_buffer (current_node, buffer, &buffer_ptr);
          if (err)
            {
              fprintf (stderr, "build_representation() : error while append flushing text buffer.\n");
              return err;
            }
        }

      buffer_ptr[0] = reading_ptr[0];
      buffer_ptr[1] = 0;
      buffer_ptr++;
      reading_ptr++;

      if ((size_t) (reading_ptr - content) >= content_len - 1)
        {
          flush_text_buffer (current_node, buffer, &buffer_ptr);
          break;
        }
    }

  cleanup:
  if (file) fclose (file);
  return err;
}

/*
 * Convert given node to markdown and output it.
 */
static int
dump (node_t *node)
{
  int err = 0;
  switch (node->type)
    {
      case NODE_ROOT:
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        break;

      case NODE_TEXT:
        printf ("%s", node->text_content);
        break;

      case NODE_PARAGRAPH:
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);

        puts ("\n");
        break;

      case NODE_STRONG_AND_EMPHASIS:
        printf ("**_");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("_**");
        break;

      default:
        err = 1;
        fprintf (stderr, "dump() : unknown node type : %d\n", node->type);
        goto cleanup;
    }

  cleanup:
  return err;
}

int
main (int argc, char **argv)
{
  int err = 0;
  node_t *root = NULL;

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
  err = build_representation (filename, root);
  if (err)
    {
      fprintf (stderr, "main() : error while building representation of file.\n");
      goto cleanup;
    }

  err = dump (root);
  if (err)
    {
      fprintf (stderr, "main() : error while dumping markdown.\n");
      goto cleanup;
    }

  cleanup:
  if (root) free_node (root);
  return err;
}
