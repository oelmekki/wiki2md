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
  NODE_BLOCKLEVEL_TEMPLATE,
  NODE_STRONG,
  NODE_EMPHASIS,
  NODE_STRONG_AND_EMPHASIS,
  NODE_BULLET_LIST,
  NODE_BULLET_LIST_ITEM,
  NODE_NUMBERED_LIST,
  NODE_NUMBERED_LIST_ITEM,
  NODE_HORIZONTAL_RULE,
  NODE_INLINE_TEMPLATE,
  NODE_NOWIKI,
  NODE_DEFINITION_LIST,
  NODE_DEFINITION_LIST_TERM,
  NODE_DEFINITION_LIST_DEFINITION,
  NODE_PREFORMATTED_TEXT,
  NODE_PREFORMATTED_CODE_BLOCK,
  NODE_LINK,
  NODE_TABLE,
};

typedef struct _node_t {
  size_t type;
  size_t subtype;
  char *text_content;
  bool is_block_level;
  bool can_have_block_children;
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
 * Parse if a mediawiki has started.
 *
 */
static int
parse_block_start (node_t **current_node, char **reading_ptr)
{
  int err = 0;

  if (!(*current_node)->can_have_block_children)
    return err;

  /*
   * if there is not at least a markup character and a text character,
   * this can't be anything we're interested in.
   */
  if (strlen (*reading_ptr) < 2)
    return err;

  node_t *new_node = NULL;
  new_node = xalloc (sizeof *new_node);
  int new_child_node = 0;

  // NODE_HEADING
  if (strncmp (*reading_ptr, "==", 2) == 0)
    {
      new_node->type = NODE_HEADING;
      new_node->subtype = 1;
      *reading_ptr += 2;
      while (*reading_ptr[0] == '=' && new_node->subtype < 6)
        {
          (*reading_ptr)++;
          new_node->subtype++;
        }
    }
  // NODE_BLOCKLEVEL_TEMPLATE
  else if (strncmp (*reading_ptr, "{{", 2) == 0)
    {
      new_node->type = NODE_BLOCKLEVEL_TEMPLATE;
      *reading_ptr += 2;
    }
  // NODE_HORIZONTAL_RULE
  else if (strncmp (*reading_ptr, "----", 4) == 0)
    {
      new_node->type = NODE_HORIZONTAL_RULE;
      *reading_ptr += 4;
    }
  // NODE_BULLET_LIST and NODE_BULLET_LIST_ITEM
  else if (strncmp (*reading_ptr, "*", 1) == 0)
    {
      if ((*current_node)->type == NODE_BULLET_LIST)
        {
          new_node->type = NODE_BULLET_LIST_ITEM;
          new_node->subtype = 0;
          while (*reading_ptr[0] == '*')
            {
              (*reading_ptr)++;
              new_node->subtype++;
            }
        }
      else
        {
          new_node->type = NODE_BULLET_LIST;
          new_node->can_have_block_children = true;
          new_child_node = NODE_BULLET_LIST_ITEM;
        }
    }
  // NODE_NUMBERED_LIST and NODE_NUMBERED_LIST_ITEM
  else if (strncmp (*reading_ptr, "#", 1) == 0)
    {
      if ((*current_node)->type == NODE_NUMBERED_LIST)
        {
          new_node->type = NODE_NUMBERED_LIST_ITEM;
          new_node->subtype = 0;
          while (*reading_ptr[0] == '#')
            {
              (*reading_ptr)++;
              new_node->subtype++;
            }
        }
      else
        {
          new_node->type = NODE_NUMBERED_LIST;
          new_node->can_have_block_children = true;
          new_child_node = NODE_NUMBERED_LIST_ITEM;
        }
    }
  // NODE_DEFINITION_LIST and NODE_DEFINITION_LIST_TERM
  else if (strncmp (*reading_ptr, ";", 1) == 0)
    {
      new_node->type = NODE_DEFINITION_LIST;
      new_node->can_have_block_children = true;
      new_child_node = NODE_DEFINITION_LIST_TERM;
    }
  // NODE_DEFINITION_LIST and NODE_DEFINITION_LIST_DEFINITION
  else if (strncmp (*reading_ptr, ":", 1) == 0)
    {
      if ((*current_node)->type == NODE_DEFINITION_LIST)
        {
          new_node->type = NODE_DEFINITION_LIST_DEFINITION;
          (*reading_ptr)++;
        }
      else
        {
          new_node->type = NODE_DEFINITION_LIST;
          new_node->can_have_block_children = true;
          new_child_node = NODE_DEFINITION_LIST_DEFINITION;
        }
    }
  else
    new_node->type = NODE_PARAGRAPH;

  new_node->is_block_level = true;
  append_child (*current_node, new_node);

  *current_node = new_node;

  if (new_child_node)
    {
      node_t *list_item = xalloc (sizeof *list_item);
      list_item->type = new_child_node;
      list_item->subtype = 1;
      list_item->is_block_level = true;
      append_child (new_node, list_item);
      *current_node = list_item;
      (*reading_ptr)++;
    }

  return err;
}

/*
 * Parse if a mediawiki has ended.
 *
 */
static int
parse_block_end (node_t **current_node, char **reading_ptr, char *buffer, char **buffer_ptr)
{
  int err = 0;

  while (true)
    {
      if ((*current_node)->type == NODE_ROOT)
        return 0;

      node_t *block = *current_node;
      bool close_parent_too = false;
      while (block && !block->is_block_level)
        block = block->parent;

      if (!block)
        {
          fprintf (stderr, "parse_block_end() : apparently, there is no block level element. This is very wrong. Did someone divide by zero?\n");
          return 1;
        }

      switch (block->type)
        {
          case NODE_PARAGRAPH:
            if (strncmp (*reading_ptr, "\n\n", 2) != 0 && strncmp (*reading_ptr, "\n----", 5) != 0 && strncmp (*reading_ptr, "\n==", 3) != 0)
              return 0;

            break;

          case NODE_HEADING:
            {
              char closing_tag[10] = {0};
              for (size_t i = 0; i < block->subtype && i < 6; i++)
                closing_tag[i] = '=';
              closing_tag[strlen (closing_tag)] = '='; // because h1 is "==", we need one more.

              if (strncmp (*reading_ptr, closing_tag, strlen (closing_tag)) != 0)
                return 0;

              while (*reading_ptr[0] != '\n' && *reading_ptr[0] != 0)
                (*reading_ptr)++;

              if (*reading_ptr[0] == '\n')
                (*reading_ptr)++;

              break;
            }

          case NODE_BLOCKLEVEL_TEMPLATE:
            if (strncmp (*reading_ptr, "}}", 2) != 0 || ((*current_node)->type == NODE_INLINE_TEMPLATE))
              return 0;
            *reading_ptr += 2;
            break;

          case NODE_HORIZONTAL_RULE:
            if (*reading_ptr[0] != '\n' && *reading_ptr[0] != 0)
              return 0;
            break;

          case NODE_BULLET_LIST:
            if (strncmp (*reading_ptr, "\n\n", 2) != 0 && strncmp (*reading_ptr, "\n----", 5) != 0 && strncmp (*reading_ptr, "\n==", 3) != 0)
              return 0;
            break;

          case NODE_BULLET_LIST_ITEM:
            {
              bool is_end_of_list = strncmp (*reading_ptr, "\n\n", 2) == 0 || strncmp (*reading_ptr, "\n----", 5) == 0 || strncmp (*reading_ptr, "\n==", 3) == 0;
              bool is_end_of_item = strncmp (*reading_ptr, "\n*", 2) == 0 || strncmp (*reading_ptr, "\n----", 5) == 0 || strncmp (*reading_ptr, "\n==", 3) == 0;
              if (!is_end_of_list && !is_end_of_item)
                return 0;

              if (is_end_of_list)
                close_parent_too = true;

              break;
            }

          case NODE_NUMBERED_LIST:
            if (strncmp (*reading_ptr, "\n\n", 2) != 0 && strncmp (*reading_ptr, "\n----", 5) != 0 && strncmp (*reading_ptr, "\n==", 3) != 0)
              return 0;
            break;

          case NODE_NUMBERED_LIST_ITEM:
            {
              bool is_end_of_list = strncmp (*reading_ptr, "\n\n", 2) == 0 || strncmp (*reading_ptr, "\n----", 5) == 0 || strncmp (*reading_ptr, "\n==", 3) == 0;
              bool is_end_of_item = strncmp (*reading_ptr, "\n#", 2) == 0 || strncmp (*reading_ptr, "\n----", 5) == 0 || strncmp (*reading_ptr, "\n==", 3) == 0;
              if (!is_end_of_list && !is_end_of_item)
                return 0;

              if (is_end_of_list)
                close_parent_too = true;

              break;
            }

          case NODE_DEFINITION_LIST:
            if (strncmp (*reading_ptr, "\n\n", 2) != 0 && strncmp (*reading_ptr, "\n----", 5) != 0 && strncmp (*reading_ptr, "\n==", 3) != 0)
              return 0;
            break;

          case NODE_DEFINITION_LIST_TERM:
            {
              bool is_end_of_list = strncmp (*reading_ptr, "\n\n", 2) == 0 || strncmp (*reading_ptr, "\n----", 5) == 0 || strncmp (*reading_ptr, "\n==", 3) == 0;
              bool is_end_of_term = strncmp (*reading_ptr, "\n:", 2) == 0 || strncmp (*reading_ptr, "\n----", 5) == 0 || strncmp (*reading_ptr, "\n==", 3) == 0;
              if (!is_end_of_list && !is_end_of_term)
                return 0;

              if (is_end_of_list)
                close_parent_too = true;

              break;
            }

          case NODE_DEFINITION_LIST_DEFINITION:
            {
              bool is_end_of_list = strncmp (*reading_ptr, "\n\n", 2) == 0 || strncmp (*reading_ptr, "\n----", 5) == 0 || strncmp (*reading_ptr, "\n==", 3) == 0;
              bool is_end_of_definition = strncmp (*reading_ptr, "\n:", 2) == 0 || strncmp (*reading_ptr, "\n----", 5) == 0 || strncmp (*reading_ptr, "\n==", 3) == 0;
              if (!is_end_of_list && !is_end_of_definition)
                return 0;

              if (is_end_of_list)
                close_parent_too = true;

              break;
            }

          default:
            fprintf (stderr, "parse_block_end() : unknown node type : %ld\n", (*current_node)->type);
            return 1;
        }

      while (*reading_ptr[0] == '\n')
        (*reading_ptr)++;

      err = flush_text_buffer (*current_node, buffer, buffer_ptr);
      if (err)
        {
          fprintf (stderr, "parse_block_end() : error while append flushing text buffer.\n");
          return err;
        }

      *current_node = block->parent;
      if (close_parent_too)
        *current_node = (*current_node)->parent;
    }

  return err;
}

/*
 * Parse mediawiki inline tags opening.
 */
static int
parse_inline_start (node_t **current_node, char **reading_ptr, char *buffer, char **buffer_ptr)
{
  int err = 0;

  while (true)
    {
      bool current_node_is_emphasis = (*current_node)->type == NODE_STRONG_AND_EMPHASIS || (*current_node)->type == NODE_STRONG || (*current_node)->type == NODE_EMPHASIS;
      bool tag_matched = false;
      node_t *new_node = NULL;

      if (strlen (*reading_ptr) > 1)
        {
          // NODE_STRONG_AND_EMPHASIS
          if (strncmp (*reading_ptr, "'''''", 5) == 0 && !current_node_is_emphasis)
            {
              tag_matched = true;
              new_node = xalloc (sizeof *new_node);
              new_node->type = NODE_STRONG_AND_EMPHASIS;
              *reading_ptr += 5;
            }
          // NODE_STRONG
          else if (strncmp (*reading_ptr, "'''", 3) == 0 && !current_node_is_emphasis)
            {
              tag_matched = true;
              new_node = xalloc (sizeof *new_node);
              new_node->type = NODE_STRONG;
              *reading_ptr += 3;
            }
          // NODE_EMPHASIS
          else if (strncmp (*reading_ptr, "''", 2) == 0 && !current_node_is_emphasis)
            {
              tag_matched = true;
              new_node = xalloc (sizeof *new_node);
              new_node->type = NODE_EMPHASIS;
              *reading_ptr += 2;
            }
          // NODE_INLINE_TEMPLATE
          else if (strncmp (*reading_ptr, "{{", 2) == 0) // if we reach this point, it's not a block level template.
            {
              tag_matched = true;
              new_node = xalloc (sizeof *new_node);
              new_node->type = NODE_INLINE_TEMPLATE;
              *reading_ptr += 2;
            }

          if (!tag_matched)
            break;

          err = flush_text_buffer (*current_node, buffer, buffer_ptr);
          if (err)
            {
              fprintf (stderr, "parse_inline_start() : error while flushing text buffer.\n");
              return err;
            }

          append_child (*current_node, new_node);
          *current_node = new_node;
        }
    }

  return err;
}

/*
 * Parse mediawiki inline tags closing.
 */
static int
parse_inline_end (node_t **current_node, char **reading_ptr, char *buffer, char **buffer_ptr)
{
  int err = 0;

  if (strlen (*reading_ptr) > 1)
    {
      switch ((*current_node)->type)
        {
          case NODE_STRONG_AND_EMPHASIS:
            if (strncmp (*reading_ptr, "'''''", 5) != 0)
              return 0;

            *reading_ptr += 5;
            break;

          case NODE_STRONG:
            if (strncmp (*reading_ptr, "'''", 3) != 0)
              return 0;

            *reading_ptr += 3;
            break;

          case NODE_EMPHASIS:
            if (strncmp (*reading_ptr, "''", 2) != 0)
              return 0;

            *reading_ptr += 2;
            break;

          case NODE_INLINE_TEMPLATE:
            if (strncmp (*reading_ptr, "}}", 2) != 0)
              return 0;

            *reading_ptr += 2;
            break;

          default:
            return 0;
        }

      err = flush_text_buffer (*current_node, buffer, buffer_ptr);
      if (err)
        {
          fprintf (stderr, "parse_inline_start() : error while flushing text buffer.\n");
          return err;
        }
      *current_node = (*current_node)->parent;
    }

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
              fprintf (stderr, "build_representation() : error while parsing block end.\n");
              return err;
            }

          if (current_node->can_have_block_children)
            {
              node_t *initial_node = current_node;
              err = parse_block_start (&current_node, &reading_ptr);
              if (err)
                {
                  fprintf (stderr, "build_representation() : error while parsing block start.\n");
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
              fprintf (stderr, "build_representation() : error while parsing for inline tag start.\n");
              return err;
            }

          err = parse_inline_end (&current_node, &reading_ptr, buffer, &buffer_ptr);
          if (err)
            {
              fprintf (stderr, "build_representation() : error while parsing for inline tag end.\n");
              return err;
            }
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
      if (nowiki && buffer_ptr[0] == '\n')
        buffer_ptr[0] = ' ';

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

      case NODE_HEADING:
        for (size_t i = 0; i < node->subtype; i++)
          printf ("#");

        if (!node->children_len || node->children[0]->type != NODE_TEXT || !node->children[0]->text_content || !isspace (node->children[0]->text_content[0]))
          printf (" ");

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);

        puts ("\n");
        break;

      case NODE_BLOCKLEVEL_TEMPLATE:
        printf ("<pre>");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("</pre>\n\n");
        break;

      case NODE_HORIZONTAL_RULE:
        printf ("--\n\n");
        break;

      case NODE_BULLET_LIST:
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);

        printf ("\n");
        break;

      case NODE_BULLET_LIST_ITEM:
        for (size_t i = 0; i < node->subtype - 1; i++)
          printf ("  ");
        printf ("*");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("\n");
        break;

      case NODE_NUMBERED_LIST:
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);

        printf ("\n");
        break;

      case NODE_NUMBERED_LIST_ITEM:
        for (size_t i = 0; i < node->subtype - 1; i++)
          printf ("  ");
        printf ("#");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("\n");
        break;

      case NODE_DEFINITION_LIST:
        printf ("<dl>\n");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("</dl>\n");
        break;

      case NODE_DEFINITION_LIST_TERM:
        printf ("<dt>");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("</dt>\n");
        break;

      case NODE_DEFINITION_LIST_DEFINITION:
        printf ("<dd>");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("</dd>\n");
        break;

      case NODE_INLINE_TEMPLATE:
        printf ("<code>");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("</code>");
        break;

      case NODE_STRONG_AND_EMPHASIS:
        printf ("**_");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("_**");
        break;

      case NODE_STRONG:
        printf ("**");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("**");
        break;

      case NODE_EMPHASIS:
        printf ("_");
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i]);
        printf ("_");
        break;

      default:
        err = 1;
        fprintf (stderr, "dump() : unknown node type : %ld\n", node->type);
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
  root->can_have_block_children = true;
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
