#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "utils.h"

typedef bool (parsing_inline_end_t) (char  **reading_ptr);
typedef struct {
  size_t type;
  parsing_inline_end_t *handler;
} parser_def_t;

/*
 * Parsing inline end for NODE_EMPHASIS.
 */
static bool
emphasis_inline_end_parser (char **reading_ptr)
{
  if (strncmp (*reading_ptr, "''", 2) != 0)
    return false;

  *reading_ptr += 2;
  return true;
}

/*
 * Parsing inline end for NODE_EXTERNAL_LINK.
 */
static bool
external_link_inline_end_parser (char **reading_ptr)
{
  if (strncmp (*reading_ptr, "]", 1) != 0)
    return false;

  *reading_ptr += 1;
  return true;
}

/*
 * Parsing inline end for NODE_INLINE_TEMPLATE.
 */
static bool
template_inline_end_parser (char **reading_ptr)
{
  if (strncmp (*reading_ptr, "}}", 2) != 0)
    return false;

  *reading_ptr += 2;
  return true;
}

/*
 * Parsing inline end for NODE_INTERNAL_LINK.
 */
static bool
internal_link_inline_end_parser (char **reading_ptr)
{
  if (strncmp (*reading_ptr, "]]", 2) != 0)
    return false;

  *reading_ptr += 2;
  return true;
}

/*
 * Parsing inline end for NODE_MEDIA.
 */
static bool
media_inline_end_parser (char **reading_ptr)
{
  if (strncmp (*reading_ptr, "]]", 2) != 0)
    return false;

  *reading_ptr += 2;
  return true;
}

/*
 * Parsing inline end for NODE_STRONG.
 */
static bool
strong_inline_end_parser (char **reading_ptr)
{
  if (strncmp (*reading_ptr, "'''", 3) != 0)
    return false;

  *reading_ptr += 3;
  return true;
}

/*
 * Parsing inline end for NODE_STRONG_AND_EMPHASIS.
 */
static bool
strong_and_emphasis_inline_end_parser (char **reading_ptr)
{
  if (strncmp (*reading_ptr, "'''''", 5) != 0)
    return false;

  *reading_ptr += 5;
  return true;
}

/*
 * Parsing inline end for NODE_TEXT.
 *
 * This is a noop, as this is the default behavior.
 */
static bool
text_inline_end_parser (char **reading_ptr)
{
  (void) reading_ptr;
  return false;
}

parser_def_t inline_end_parsers[INLINE_NODES_COUNT] = {
  { .type = NODE_EMPHASIS, .handler = emphasis_inline_end_parser },
  { .type = NODE_EXTERNAL_LINK, .handler = external_link_inline_end_parser },
  { .type = NODE_INLINE_TEMPLATE, .handler = template_inline_end_parser },
  { .type = NODE_INTERNAL_LINK, .handler = internal_link_inline_end_parser },
  { .type = NODE_MEDIA, .handler = media_inline_end_parser },
  { .type = NODE_STRONG, .handler = strong_inline_end_parser },
  { .type = NODE_STRONG_AND_EMPHASIS, .handler = strong_and_emphasis_inline_end_parser },
  { .type = NODE_TEXT, .handler = text_inline_end_parser },
};

/*
 * Parse mediawiki inline tags closing.
 */
int
parse_inline_end (node_t **current_node, char **reading_ptr, char *buffer, char **buffer_ptr)
{
  int err = 0;

  while (true)
    {
      if (strlen (*reading_ptr) < 2)
        return 0;

      if ((*current_node)->is_block_level)
        return 0;

      for (size_t i = 0; i < INLINE_NODES_COUNT; i++)
        {
          parser_def_t def = inline_end_parsers[i];
          if (def.type == (*current_node)->type)
            {
              bool matched = def.handler (reading_ptr);
              if (!matched)
                return 0;

              break;
            }
        }

      err = flush_text_buffer (*current_node, buffer, buffer_ptr);
      if (err)
        {
          fprintf (stderr, "parse_inline_end.c : parse_inline_end() : error while flushing text buffer.\n");
          return err;
        }
      *current_node = (*current_node)->parent;
    }

  return err;
}
