#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "utils.h"

typedef struct {
  node_t *current_node;
  char **reading_ptr;
  node_t *new_node;
  int *new_child_node;
  size_t *list_item_markup_len;
} parsing_block_start_params_t;

typedef bool (parsing_block_start_t) (parsing_block_start_params_t *params);
typedef struct {
  size_t type; // only used to make `block_start_parsers` more readable.
  parsing_block_start_t *handler;
} parser_def_t;

/*
 * Parsing start of NODE_BLOCKLEVEL_TEMPLATE.
 */
static bool
template_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "{{", 2) == 0)
    {
      params->new_node->type = NODE_BLOCKLEVEL_TEMPLATE;
      *params->reading_ptr += 2;
      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_BULLET_LIST
 */
static bool
bullet_list_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "*", 1) == 0)
    {
      if (params->current_node->type != NODE_BULLET_LIST)
        {
          params->new_node->type = NODE_BULLET_LIST;
          params->new_node->can_have_block_children = true;
          *params->new_child_node = NODE_BULLET_LIST_ITEM;
          *params->list_item_markup_len = 1;

          return true;
        }
    }

  return false;
}

/*
 * Parsing start of NODE_BULLET_LIST_ITEM.
 */
static bool
bullet_list_item_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "*", 1) == 0)
    {
      if (params->current_node->type == NODE_BULLET_LIST)
        {
          params->new_node->type = NODE_BULLET_LIST_ITEM;
          params->new_node->subtype = 0;
          while (*params->reading_ptr[0] == '*')
            {
              (*params->reading_ptr)++;
              params->new_node->subtype++;
            }

          return true;
        }
    }

  return false;
}

/*
 * Parsing start of NODE_DEFINITION_LIST_TERM.
 */
static bool
definition_list_term_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, ";", 1) == 0)
    {
      params->new_node->type = NODE_DEFINITION_LIST;
      params->new_node->can_have_block_children = true;
      *params->new_child_node = NODE_DEFINITION_LIST_TERM;
      *params->list_item_markup_len = 1;
      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_DEFINITION_LIST.
 */
static bool
definition_list_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, ":", 1) == 0)
    {
      if (params->current_node->type != NODE_DEFINITION_LIST)
        {
          params->new_node->type = NODE_DEFINITION_LIST;
          params->new_node->can_have_block_children = true;
          *params->new_child_node = NODE_DEFINITION_LIST_DEFINITION;
          *params->list_item_markup_len = 1;
          return true;
        }
    }

  return false;
}

/*
 * Parsing start of NODE_DEFINITION_DEFINITION.
 */
static bool
definition_list_definition_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, ":", 1) == 0)
    {
      if (params->current_node->type == NODE_DEFINITION_LIST)
        {
          params->new_node->type = NODE_DEFINITION_LIST_DEFINITION;
          (*params->reading_ptr)++;
          return true;
        }
    }

  return false;
}

/*
 * Parsing start of NODE_GALLERY.
 */
static bool
gallery_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "<gallery>", 9) == 0)
    {
      params->new_node->type = NODE_GALLERY;
      params->new_node->can_have_block_children = true;
      *params->new_child_node = NODE_GALLERY_ITEM;
      *params->reading_ptr += 9;
      *params->list_item_markup_len = 0;

      while (*params->reading_ptr[0] == '\n')
        (*params->reading_ptr)++;

      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_GALLERY_ITEM.
 *
 * Noop, as creating a new NODE_GALLERY_ITEM is done either
 * in gallery_block_start_parser() or in gallery_item_block_end_parser().
 */
static bool
gallery_item_block_start_parser (parsing_block_start_params_t *params)
{
  (void) params;
  return false;
}

/*
 * Parsing start of NODE_HEADING.
 */
static bool
heading_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "==", 2) == 0)
    {
      params->new_node->type = NODE_HEADING;
      params->new_node->subtype = 1;
      *params->reading_ptr += 2;
      while (*params->reading_ptr[0] == '=' && params->new_node->subtype < 6)
        {
          (*params->reading_ptr)++;
          params->new_node->subtype++;
        }

      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_HORIZONTAL_RULE.
 */
static bool
horizontal_rule_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "----", 4) == 0)
    {
      params->new_node->type = NODE_HORIZONTAL_RULE;
      *params->reading_ptr += 4;
      return true;
    }

  return false;
}

/*
 * Parsing start of NODE_NUMBERED_LIST.
 */
static bool
numbered_list_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "#", 1) == 0)
    {
      if ((params->current_node)->type != NODE_NUMBERED_LIST)
        {
          params->new_node->type = NODE_NUMBERED_LIST;
          params->new_node->can_have_block_children = true;
          *params->new_child_node = NODE_NUMBERED_LIST_ITEM;
          *params->list_item_markup_len = 1;

          return true;
        }
    }

  return false;
}

/*
 * Parsing start of NODE_NUMBERED_LIST_ITEM.
 */
static bool
numbered_list_item_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, "#", 1) == 0)
    {
      if ((params->current_node)->type == NODE_NUMBERED_LIST)
        {
          params->new_node->type = NODE_NUMBERED_LIST_ITEM;
          params->new_node->subtype = 0;
          while (*params->reading_ptr[0] == '#')
            {
              (*params->reading_ptr)++;
              params->new_node->subtype++;
            }

          return true;
        }
    }

  return false;
}

/*
 * Parsing start of NODE_PREFORMATTED_TEXT.
 */
static bool
preformated_text_block_start_parser (parsing_block_start_params_t *params)
{
  if (strncmp (*params->reading_ptr, " ", 1) == 0)
    {
      params->new_node->type = NODE_PREFORMATTED_TEXT;
      (*params->reading_ptr)++;

      return true;
    }

  return false;
}

/*
 * If we reached that point in the parsing pipeline,
 * it's a NODE_PARAGRAPH.
 */
static bool
paragraph_block_start_parser (parsing_block_start_params_t *params)
{
  params->new_node->type = NODE_PARAGRAPH;
  return true;
}

parser_def_t block_start_parsers[BLOCK_LEVEL_NODES_COUNT] = {
  { .type = NODE_BLOCKLEVEL_TEMPLATE, .handler = template_block_start_parser },
  { .type = NODE_BULLET_LIST, .handler = bullet_list_block_start_parser },
  { .type = NODE_BULLET_LIST_ITEM, .handler = bullet_list_item_block_start_parser },
  { .type = NODE_DEFINITION_LIST_TERM, .handler = definition_list_term_block_start_parser },
  { .type = NODE_DEFINITION_LIST, .handler = definition_list_block_start_parser },
  { .type = NODE_DEFINITION_LIST_DEFINITION, .handler = definition_list_definition_block_start_parser },
  { .type = NODE_GALLERY, .handler = gallery_block_start_parser },
  { .type = NODE_GALLERY_ITEM, .handler = gallery_item_block_start_parser },
  { .type = NODE_HEADING, .handler = heading_block_start_parser },
  { .type = NODE_HORIZONTAL_RULE, .handler = horizontal_rule_block_start_parser },
  { .type = NODE_NUMBERED_LIST, .handler = numbered_list_block_start_parser },
  { .type = NODE_NUMBERED_LIST_ITEM, .handler = numbered_list_item_block_start_parser },
  { .type = NODE_PREFORMATTED_TEXT, .handler = preformated_text_block_start_parser },
  { .type = NODE_PARAGRAPH, .handler = paragraph_block_start_parser },
};

/*
 * Parse if a mediawiki block has started.
 */
int
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
  size_t list_item_markup_len = 0;

  for (size_t i = 0; i < BLOCK_LEVEL_NODES_COUNT; i++)
    {
      parser_def_t def = block_start_parsers[i];

      parsing_block_start_params_t params = {
        .current_node = *current_node,
        .reading_ptr = reading_ptr,
        .new_node = new_node,
        .new_child_node = &new_child_node,
        .list_item_markup_len = &list_item_markup_len,
      };

      bool matched = def.handler (&params);
      if (matched)
        break;
    }

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
      *reading_ptr += list_item_markup_len;
    }

  return err;
}
