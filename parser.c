#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_block_end.h"
#include "parse_block_start.h"
#include "parse_inline_start.h"
#include "parse_inline_end.h"
#include "parser.h"
#include "utils.h"

/*
 * Add text to a text node.
 */
static int
append_text (node_t *text_node, const char *text)
{
  if (text_node->type != NODE_TEXT)
    {
      fprintf (stderr, "parser.c : append_text() : error : trying to add text to a non text node.\n");
      return 1;
    }

  size_t len = text_node->text_content ? strlen (text_node->text_content) : 0;
  text_node->text_content = xrealloc (text_node->text_content, len + strlen (text) + 1);
  snprintf (text_node->text_content + len, strlen (text) + 1, "%s", text);

  return 0;
}

/*
 * Add a child to a parent's memory.
 */
void
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
 * Write text buffer to text node.
 */
int
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
      fprintf (stderr, "parser.c : flush_text_buffer() : error while append text to text node.\n");
      return err;
    }

  memset (buffer, 0, BUFSIZ);
  *buffer_ptr = buffer;

  return err;
}

/*
 * Cleanup memory for a node.
 */
void
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
 * Build a representation of the document, so that it's
 * then easier to serialize.
 *
 * The result is stored in `root`. You should provide
 * the memory for it, and clean its content with `free_node()`.
 */
int
parse (const char *filename, node_t *root)
{
  int err = 0;
  char content[MAX_FILE_SIZE] = {0};
  FILE *file = NULL;

  file = fopen (filename, "r");
  if (!file)
    {
      fprintf (stderr, "parser.c : parse() : can't read file %s\n", filename);
      goto cleanup;
    }

  size_t content_len = fread (content, 1, MAX_FILE_SIZE - 1, file);
  if (content_len == MAX_FILE_SIZE - 1)
    fprintf (stderr, "parser.c : parse() : warning : input file has probably been truncated due to its size.\n");

  node_t *current_node = root;
  char *reading_ptr = content;
  char buffer[BUFSIZ] = {0};
  char *buffer_ptr = buffer;
  bool nowiki = false;

  while (true)
    {
      if (strncmp (reading_ptr, "<nowiki>", 8) == 0 && !nowiki)
        {
          nowiki = true;
          reading_ptr += 8;
        }

      if (strncmp (reading_ptr, "</nowiki>", 9) == 0 && nowiki)
        {
          nowiki = false;
          reading_ptr += 9;
        }

      if (!nowiki)
        {
          err = parse_block_end (&current_node, &reading_ptr, buffer, &buffer_ptr);
          if (err)
            {
              fprintf (stderr, "parser.c : parse() : error while parsing block end.\n");
              return err;
            }

          if (current_node->can_have_block_children)
            {
              node_t *initial_node = current_node;
              err = parse_block_start (&current_node, &reading_ptr);
              if (err)
                {
                  fprintf (stderr, "parser.c : parse() : error while parsing block start.\n");
                  return err;
                }

              if (!current_node)
                break;

              if (current_node != initial_node)
                continue;
            }

          err = parse_inline_start (&current_node, &reading_ptr, buffer, &buffer_ptr);
          if (err)
            {
              fprintf (stderr, "parser.c : parse() : error while parsing for inline tag start.\n");
              return err;
            }

          err = parse_inline_end (&current_node, &reading_ptr, buffer, &buffer_ptr);
          if (err)
            {
              fprintf (stderr, "parser.c : parse() : error while parsing for inline tag end.\n");
              return err;
            }
        }

      if (buffer_ptr - buffer == BUFSIZ - 1)
        {
          err = flush_text_buffer (current_node, buffer, &buffer_ptr);
          if (err)
            {
              fprintf (stderr, "parser.c : parse() : error while append flushing text buffer.\n");
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

