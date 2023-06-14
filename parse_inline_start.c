#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "utils.h"

typedef struct {
  node_t *current_node;
  char **reading_ptr;
  node_t **new_node;
} parsing_inline_start_params_t;

typedef bool (parsing_inline_start_t) (parsing_inline_start_params_t *params);
typedef struct {
  size_t type; // only used to make `inline_start_parsers` more readable.
  parsing_inline_start_t *handler;
} parser_def_t;

/*
 * Parsing start of NODE_STRONG_AND_EMPHASIS.
 */
static bool
strong_and_emphasis_inline_start_parser (parsing_inline_start_params_t *params)
{
  bool current_node_is_emphasis = params->current_node->type == NODE_STRONG_AND_EMPHASIS || params->current_node->type == NODE_STRONG || params->current_node->type == NODE_EMPHASIS;
  if (strncmp (*params->reading_ptr, "'''''", 5) == 0 && !current_node_is_emphasis)
    {
      *params->new_node = xalloc (sizeof **params->new_node);
      (*params->new_node)->type = NODE_STRONG_AND_EMPHASIS;
      *params->reading_ptr += 5;
      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_STRONG.
 */
static bool
strong_inline_start_parser (parsing_inline_start_params_t *params)
{
  bool current_node_is_emphasis = params->current_node->type == NODE_STRONG_AND_EMPHASIS || params->current_node->type == NODE_STRONG || params->current_node->type == NODE_EMPHASIS;
  if (strncmp (*params->reading_ptr, "'''", 3) == 0 && !current_node_is_emphasis)
    {
      *params->new_node = xalloc (sizeof **params->new_node);
      (*params->new_node)->type = NODE_STRONG;
      *params->reading_ptr += 3;
      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_EMPHASIS.
 */
static bool
emphasis_inline_start_parser (parsing_inline_start_params_t *params)
{
  bool current_node_is_emphasis = params->current_node->type == NODE_STRONG_AND_EMPHASIS || params->current_node->type == NODE_STRONG || params->current_node->type == NODE_EMPHASIS;

  if (strncmp (*params->reading_ptr, "''", 2) == 0 && !current_node_is_emphasis)
    {
      *params->new_node = xalloc (sizeof **params->new_node);
      (*params->new_node)->type = NODE_EMPHASIS;
      *params->reading_ptr += 2;
      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_INTERNAL_LINK.
 */
static bool
internal_link_inline_start_parser (parsing_inline_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "[[", 2) == 0)
    {
      *params->new_node = xalloc (sizeof **params->new_node);
      (*params->new_node)->type = NODE_INTERNAL_LINK;
      *params->reading_ptr += 2;
      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_EXTERNAL_LINK.
 */
static bool
external_link_inline_start_parser (parsing_inline_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "[", 1) == 0)
    {
      *params->new_node = xalloc (sizeof **params->new_node);
      (*params->new_node)->type = NODE_EXTERNAL_LINK;
      *params->reading_ptr += 1;
      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_INLINE_TEMPLATE.
 */
static bool
template_inline_start_parser (parsing_inline_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "{{", 2) == 0) // if we reach this point, it's not a block level template.
    {
      *params->new_node = xalloc (sizeof **params->new_node);
      (*params->new_node)->type = NODE_INLINE_TEMPLATE;
      *params->reading_ptr += 2;
      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_MEDIA.
 */
static bool
media_inline_start_parser (parsing_inline_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "[[File:", 7) == 0)
    {
      *params->new_node = xalloc (sizeof **params->new_node);
      (*params->new_node)->type = NODE_MEDIA;
      *params->reading_ptr += 2;
      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_TEXT.
 *
 * Noop, as it's the default node.
 */
static bool
text_inline_start_parser (parsing_inline_start_params_t *params)
{
  (void) params;
  return false;
}

// don't sort this, as the order matters.
parser_def_t inline_start_parsers[INLINE_NODES_COUNT] = {
  { .type = NODE_STRONG_AND_EMPHASIS, .handler = strong_and_emphasis_inline_start_parser },
  { .type = NODE_STRONG, .handler = strong_inline_start_parser },
  { .type = NODE_EMPHASIS, .handler = emphasis_inline_start_parser },
  { .type = NODE_INTERNAL_LINK, .handler = internal_link_inline_start_parser },
  { .type = NODE_EXTERNAL_LINK, .handler = external_link_inline_start_parser },
  { .type = NODE_INLINE_TEMPLATE, .handler = template_inline_start_parser },
  { .type = NODE_MEDIA, .handler = media_inline_start_parser },
  { .type = NODE_TEXT, .handler = text_inline_start_parser },
};

/*
 * Parse mediawiki inline tags opening.
 */
int
parse_inline_start (node_t **current_node, char **reading_ptr, char *buffer, char **buffer_ptr)
{
  int err = 0;

  while (true)
    {
      bool tag_matched = false;
      node_t *new_node = NULL;

      if (strlen (*reading_ptr) < 2)
        break;

      for (size_t i = 0; i < INLINE_NODES_COUNT; i++)
        {
          parser_def_t def = inline_start_parsers[i];

          parsing_inline_start_params_t params = {
            .current_node = *current_node,
            .reading_ptr = reading_ptr,
            .new_node = &new_node,
          };

          tag_matched = def.handler (&params);
          if (tag_matched)
            break;
        }

      if (!tag_matched)
        break;

      err = flush_text_buffer (*current_node, buffer, buffer_ptr);
      if (err)
        {
          fprintf (stderr, "parse_inline_start.c : parse_inline_start() : error while flushing text buffer.\n");
          return err;
        }

      append_child (*current_node, new_node);
      *current_node = new_node;
    }

  return err;
}
