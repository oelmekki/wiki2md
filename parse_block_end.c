#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "utils.h"

typedef struct {
  node_t *current_node;
  node_t *block;
  node_t **next_item;
  char **reading_ptr;
  bool *close_parent_too;

  node_t *new_node;
} parsing_block_end_params_t;

typedef bool (parsing_block_end_t) (parsing_block_end_params_t *params);
typedef struct {
  size_t type;
  parsing_block_end_t *handler;
} parser_def_t;

/*
 * Parsing block end for NODE_BLOCKLEVEL_TEMPLATE.
 */
static bool
template_block_end_parser (parsing_block_end_params_t *params)
{
  if (strncmp (*params->reading_ptr, "}}", 2) == 0 && (params->current_node->type != NODE_INLINE_TEMPLATE))
    {
      *params->reading_ptr += 2;
      return true;
    }

  return false;
}

/*
 * Parsing block end for NODE_BULLET_LIST.
 */
static bool
bullet_list_block_end_parser (parsing_block_end_params_t *params)
{
  if (strncmp (*params->reading_ptr, "\n\n", 2) == 0 || strncmp (*params->reading_ptr, "\n----", 5) == 0 || strncmp (*params->reading_ptr, "\n==", 3) == 0)
    return true;

  return false;
}

/*
 * Parsing block end for NODE_BULLET_LIST_ITEM.
 */
static bool
bullet_list_item_block_end_parser (parsing_block_end_params_t *params)
{
  bool is_end_of_list = strncmp (*params->reading_ptr, "\n\n", 2) == 0 || strncmp (*params->reading_ptr, "\n----", 5) == 0 || strncmp (*params->reading_ptr, "\n==", 3) == 0;
  bool is_end_of_item = strncmp (*params->reading_ptr, "\n*", 2) == 0 || strncmp (*params->reading_ptr, "\n----", 5) == 0 || strncmp (*params->reading_ptr, "\n==", 3) == 0;

  if (is_end_of_list || is_end_of_item)
    {
      if (is_end_of_list)
        *params->close_parent_too = true;

      return true;
    }


  return false;
}

/*
 * Parsing block end for NODE_DEFINITION_LIST_TERM.
 */
static bool
definition_list_term_block_end_parser (parsing_block_end_params_t *params)
{
  bool is_end_of_term = *params->reading_ptr[0] == '\n';
  bool no_follow_up = is_end_of_term && strncmp (*params->reading_ptr, "\n:", 2) != 0;
  bool is_end_of_list = strncmp (*params->reading_ptr, "\n\n", 2) == 0 || no_follow_up;

  if (is_end_of_list || is_end_of_term)
    {
      if (is_end_of_list)
        *params->close_parent_too = true;

      return true;
    }

  return false;
}

/*
 * Parsing block end for NODE_DEFINITION_LIST.
 */
static bool
definition_list_block_end_parser (parsing_block_end_params_t *params)
{
  if (strncmp (*params->reading_ptr, "\n\n", 2) == 0 || strncmp (*params->reading_ptr, "\n----", 5) == 0 || strncmp (*params->reading_ptr, "\n==", 3) == 0)
    return true;

  return false;
}

/*
 * Parsing block end for NODE_DEFINITION_LIST_DEFINITION.
 */
static bool
definition_list_definition_block_end_parser (parsing_block_end_params_t *params)
{
  bool is_end_of_list = strncmp (*params->reading_ptr, "\n\n", 2) == 0 || strncmp (*params->reading_ptr, "\n----", 5) == 0 || strncmp (*params->reading_ptr, "\n==", 3) == 0;
  bool is_end_of_definition = strncmp (*params->reading_ptr, "\n:", 2) == 0 || strncmp (*params->reading_ptr, "\n----", 5) == 0 || strncmp (*params->reading_ptr, "\n==", 3) == 0;

  if (is_end_of_list || is_end_of_definition)
    {
      if (is_end_of_list)
        *params->close_parent_too = true;

      return true;
    }

  return false;
}

/*
 * Parsing block end for NODE_GALLERY.
 */
static bool
gallery_block_end_parser (parsing_block_end_params_t *params)
{
  if (strncmp (*params->reading_ptr, "</gallery>", 10) == 0)
    {
      *params->reading_ptr += 10;
      return true;
    }

  return false;
}

/*
 * Parsing block end for NODE_GALLERY_ITEM.
 */
static bool
gallery_item_block_end_parser (parsing_block_end_params_t *params)
{
  if (strncmp (*params->reading_ptr, "\n", 1) == 0 || strncmp (*params->reading_ptr, "</gallery>", 10) == 0)
    {
      if (strncmp (*params->reading_ptr, "\n", 1) == 0 && strncmp (*params->reading_ptr, "\n</gallery>", 11) != 0)
        {
          // not ideal to put it here, but since those list items are not prepended
          // by any markup, it makes things easier than to handle it in parse_block_start().

          *params->next_item = xalloc (sizeof **params->next_item);
          (*params->next_item)->type = NODE_GALLERY_ITEM;
          (*params->next_item)->is_block_level = true;
          append_child (params->current_node->parent, *params->next_item);
        }

      if (strncmp (*params->reading_ptr, "</gallery>", 10) == 0)
        *params->reading_ptr += 10;

      return true;
    }

  return false;
}

/*
 * Parsing block end for NODE_HEADING.
 */
static bool
heading_block_end_parser (parsing_block_end_params_t *params)
{
  char closing_tag[10] = {0};
  for (size_t i = 0; i < params->block->subtype && i < 6; i++)
    closing_tag[i] = '=';
  closing_tag[strlen (closing_tag)] = '='; // because h1 is "==", we need one more.

  if (strncmp (*params->reading_ptr, closing_tag, strlen (closing_tag)) == 0)
    {
      while (*params->reading_ptr[0] != '\n' && *params->reading_ptr[0] != 0)
        (*params->reading_ptr)++;

      if (*params->reading_ptr[0] == '\n')
        (*params->reading_ptr)++;

      return true;
    }

  return false;
}

/*
 * Parsing block end for NODE_HORIZONTAL_RULE.
 */
static bool
horizontal_rule_block_end_parser (parsing_block_end_params_t *params)
{
  if (*params->reading_ptr[0] == '\n' || *params->reading_ptr[0] == 0)
    return true;

  return false;
}

/*
 * Parsing block end for NODE_NUMBERED_LIST.
 */
static bool
numbered_list_block_end_parser (parsing_block_end_params_t *params)
{
  if (strncmp (*params->reading_ptr, "\n\n", 2) == 0 || strncmp (*params->reading_ptr, "\n----", 5) == 0 || strncmp (*params->reading_ptr, "\n==", 3) == 0)
    return true;

  return false;
}

/*
 * Parsing block end for NODE_NUMBERED_LIST_ITEM.
 */
static bool
numbered_list_item_block_end_parser (parsing_block_end_params_t *params)
{
  bool is_end_of_list = strncmp (*params->reading_ptr, "\n\n", 2) == 0 || strncmp (*params->reading_ptr, "\n----", 5) == 0 || strncmp (*params->reading_ptr, "\n==", 3) == 0;
  bool is_end_of_item = strncmp (*params->reading_ptr, "\n#", 2) == 0 || strncmp (*params->reading_ptr, "\n----", 5) == 0 || strncmp (*params->reading_ptr, "\n==", 3) == 0;

  if (is_end_of_list || is_end_of_item)
    {
      if (is_end_of_list)
        *params->close_parent_too = true;

      return true;
    }

  return false;
}

/*
 * Parsing block end for NODE_PREFORMATTED_TEXT.
 */
static bool
preformated_text_block_end_parser (parsing_block_end_params_t *params)
{
  if (strncmp (*params->reading_ptr, "\n", 1) == 0 && strncmp (*params->reading_ptr, "\n ", 2) != 0)
    return true;

  return false;
}

/*
 * Parsing block end for NODE_TABLE.
 */
static bool
table_block_end_parser (parsing_block_end_params_t *params)
{
  if (strncmp (*params->reading_ptr, "|}", 2) == 0)
    {
      *params->reading_ptr += 2;
      return true;
    }

  return false;
}

/*
 * Parsing block end for NODE_TABLE_CAPTION.
 */
static bool
table_caption_block_end_parser (parsing_block_end_params_t *params)
{
  if (strncmp (*params->reading_ptr, "\n", 1) == 0)
    return true;

  return false;
}

/*
 * Parsing block end for NODE_TABLE_CAPTION.
 */
static bool
table_row_block_end_parser (parsing_block_end_params_t *params)
{
  if (strncmp (*params->reading_ptr, "\n|-", 3) == 0 || strncmp (*params->reading_ptr, "|}", 2) == 0)
    return true;

  return false;
}

#define PARAGRAPH_ENDING_STRINGS_LEN 7
const char *paragraph_ending_strings[PARAGRAPH_ENDING_STRINGS_LEN] = { "\n\n", "\n----", "\n==", "\n*", "\n#", "\n:", "\n;"};

/*
 * Parsing block end for NODE_PARAGRAPH.
 */
static bool
paragraph_block_end_parser (parsing_block_end_params_t *params)
{
  for (size_t i = 0; i < PARAGRAPH_ENDING_STRINGS_LEN; i++)
    {
      const char *terminator = paragraph_ending_strings[i];
      if (strncmp (*params->reading_ptr, terminator, strlen (terminator)) == 0)
        return true;
    }

  if (is_inline_block_template (*params->reading_ptr))
    return true;

  return false;
}

parser_def_t block_end_parsers[BLOCK_LEVEL_NODES_COUNT] = {
  { .type = NODE_BLOCKLEVEL_TEMPLATE, .handler = template_block_end_parser },
  { .type = NODE_BULLET_LIST, .handler = bullet_list_block_end_parser },
  { .type = NODE_BULLET_LIST_ITEM, .handler = bullet_list_item_block_end_parser },
  { .type = NODE_DEFINITION_LIST_TERM, .handler = definition_list_term_block_end_parser },
  { .type = NODE_DEFINITION_LIST, .handler = definition_list_block_end_parser },
  { .type = NODE_DEFINITION_LIST_DEFINITION, .handler = definition_list_definition_block_end_parser },
  { .type = NODE_GALLERY, .handler = gallery_block_end_parser },
  { .type = NODE_GALLERY_ITEM, .handler = gallery_item_block_end_parser },
  { .type = NODE_HEADING, .handler = heading_block_end_parser },
  { .type = NODE_HORIZONTAL_RULE, .handler = horizontal_rule_block_end_parser },
  { .type = NODE_NUMBERED_LIST, .handler = numbered_list_block_end_parser },
  { .type = NODE_NUMBERED_LIST_ITEM, .handler = numbered_list_item_block_end_parser },
  { .type = NODE_PREFORMATTED_TEXT, .handler = preformated_text_block_end_parser },
  { .type = NODE_TABLE, .handler = table_block_end_parser },
  { .type = NODE_TABLE_CAPTION, .handler = table_caption_block_end_parser },
  { .type = NODE_TABLE_ROW, .handler = table_row_block_end_parser },
  { .type = NODE_PARAGRAPH, .handler = paragraph_block_end_parser },
};

/*
 * Parse if a mediawiki block has ended.
 *
 */
int
parse_block_end (node_t **current_node, char **reading_ptr, char *buffer, char **buffer_ptr)
{
  while (true)
    {
      if ((*current_node)->is_block_level && (*current_node)->type == NODE_ROOT)
        return 0;

      node_t *block = *current_node;
      node_t *next_item = NULL;
      bool close_parent_too = false;
      while (block && !block->is_block_level)
        block = block->parent;

      if (!block)
        {
          fprintf (stderr, "parse_block_end.c : parse_block_end() : apparently, there is no block level element. This is very wrong. Did someone divide by zero?\n");
          return 1;
        }

      bool found = false;
      for (size_t i = 0; i < BLOCK_LEVEL_NODES_COUNT; i++)
        {
          parser_def_t def = block_end_parsers[i];
          if (def.type == block->type)
            {
              found = true;
              parsing_block_end_params_t params = {
                .current_node = *current_node,
                .block = block,
                .next_item = &next_item,
                .reading_ptr = reading_ptr,
                .close_parent_too = &close_parent_too,
              };

              bool matched = def.handler (&params);
              if (!matched)
                return 0;

              break;
            } 
        }

      if (!found)
        {
          fprintf (stderr, "parse_block_end.c : parse_block_end() : unknown node type : %ld\n", (*current_node)->type);
          return 1;
        }

      while (*reading_ptr[0] == '\n')
        (*reading_ptr)++;

      int err = flush_text_buffer (*current_node, buffer, buffer_ptr);
      if (err)
        {
          fprintf (stderr, "parse_block_end.c : parse_block_end() : error while append flushing text buffer.\n");
          return err;
        }

      if (next_item)
        *current_node = next_item;
      else
        {
          *current_node = block->parent;

          if (close_parent_too)
            *current_node = (*current_node)->parent;
        }
    }

  return 0;
}
