#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "dumper.h"

#define MAX_LINE_LENGTH 10000
#define MAX_LINK_LENGTH 5000

#define SUPPORTED_IMAGE_FORMATS 7
const char *image_formats[SUPPORTED_IMAGE_FORMATS] = { ".jpg", ".jpeg", ".png", ".gif", ".webp", ".svg", ".tiff" };

typedef int (dumping_node_t) (dumping_params_t *params);
typedef struct {
  size_t type; // only used to make `dumpers` more readable.
  dumping_node_t *handler;
} dumper_def_t;

static bool 
contains_link (node_t *node)
{
  if (!node->is_block_level && (node->type == NODE_INTERNAL_LINK || node->type == NODE_EXTERNAL_LINK))
    return true;

  if (node->children_len == 0)
    return false;

  for (size_t i = 0; i < node->children_len; i++)
    if (contains_link (node->children[i]))
      return true;

  return false;
}

/*
 * Convert a mediawiki media link to markdown.
 *
 * More parsing is done here to match the various components
 * of the links.
 */
static int
dump_media (dumping_params_t *params)
{
  char link_def[MAX_LINK_LENGTH] = {0};
  char *link_ptr = link_def;
  size_t link_max_len = MAX_LINK_LENGTH;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = &link_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = &link_max_len,
      };

      int err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : dump_media() : error while processing link content.\n");
          return 1;
        }
    }

  if (strlen (link_def) == 0)
    {
      fprintf (stderr, "dumper.c : dump_media() : warning : empty link detected.\n");
      return 1;
    }

  char *first_pipe = strstr (link_def, "|");
  char *last_pipe = first_pipe;
  if (last_pipe)
    {
      last_pipe++;
      while (true)
        {
          char *next = strstr (last_pipe, "|");
          if (next)
            last_pipe = ++next;
          else
            break;
        }
    }
  char url[MAX_LINK_LENGTH] = {0};
  snprintf (url, (first_pipe ? (size_t) (first_pipe - link_def) : strlen (link_def)) + 1, "%s", link_def);

  if (!last_pipe || !strlen (last_pipe))
    last_pipe = url;

  bool is_image = false;
  char *lower_url = strdup (url);

  for (size_t i = 0; i < strlen (lower_url); i++)
    lower_url[i] = tolower (lower_url[i]);

  for (size_t i = 0; i < SUPPORTED_IMAGE_FORMATS; i++)
    {
      const char *format = image_formats[i];
      char *match = strstr (lower_url, format);
      if (match && strlen (match) == strlen (format))
        {
          is_image = true;
          break;
        }
    }

  free (lower_url);

  char markup[MAX_LINK_LENGTH] = {0};

  if (contains_link (params->node))
    {
      if (is_image)
        {
          if (snprintf (markup, MAX_LINK_LENGTH - 1, "![%s](%s)\n\n**%s**\n\n", url, url, last_pipe) >= MAX_LINK_LENGTH - 1)
            fprintf (stderr, "dumper.c : dump_media() : warning : link too long has been truncated : %s, %s\n", last_pipe, url);
        }
      else
        {
          if (snprintf (markup, MAX_LINK_LENGTH - 1, "[%s](%s)", last_pipe, url) >= MAX_LINK_LENGTH - 1)
            fprintf (stderr, "dumper.c : dump_media() : warning : link too long has been truncated : %s, %s\n", last_pipe, url);
        }
    }
  else
    {
      if (is_image)
        {
          if (snprintf (markup, MAX_LINK_LENGTH - 1, "![%s](%s)", last_pipe, url) >= MAX_LINK_LENGTH - 1)
            fprintf (stderr, "dumper.c : dump_media() : warning : link too long has been truncated : %s, %s\n", last_pipe, url);
        }
      else
        {
          if (snprintf (markup, MAX_LINK_LENGTH - 1, "[%s](%s)", last_pipe, url) >= MAX_LINK_LENGTH - 1)
            fprintf (stderr, "dumper.c : dump_media() : warning : link too long has been truncated : %s, %s\n", last_pipe, url);
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "%s", markup);
  *params->writing_ptr += strlen (markup);
  *params->max_len -= strlen (markup);

  return 0;
}

/*
 * Markdown doesn't accept parenthesis in urls, so we need to escape them.
 */
static void
escape_url_for_markdown (const char url[MAX_LINK_LENGTH], char escaped_url[MAX_LINK_LENGTH])
{
  char *read_ptr = (char *) url;
  char *write_ptr = escaped_url;
  size_t len = strlen (url);

  for (size_t i = 0; i < len; i++)
    {
      size_t written = write_ptr - escaped_url;
      if (read_ptr[0] == '(' && written + 4 < MAX_LINK_LENGTH - 1)
        {
          snprintf (write_ptr, 4, "%%28");
          write_ptr += 3;
        }
      else if (read_ptr[0] == ')' && written + 4 < MAX_LINK_LENGTH - 1)
        {
          snprintf (write_ptr, 4, "%%29");
          write_ptr += 3;
        }
      else if (written + 2 < MAX_LINK_LENGTH - 1)
        {
          write_ptr[0] = read_ptr[0];
          write_ptr++;
        }

      read_ptr++;
    }
}

/*
 * Generates markdown for NODE_BLOCKLEVEL_TEMPLATE.
 */
static int
template_block_dumper (dumping_params_t *params)
{
  int err = 0;

  snprintf (*params->writing_ptr, *params->max_len, "<pre>{{");
  *params->writing_ptr += 7;
  *params->max_len -= 7;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : template_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "}}<pre>\n\n");
  *params->writing_ptr += 9;
  *params->max_len -= 9;

  return err;
}

/*
 * Generates markdown for NODE_BULLET_LIST.
 */
static int
bullet_list_block_dumper (dumping_params_t *params)
{
  int err = 0;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : bullet_list_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n");
  (*params->writing_ptr)++;
  (*params->max_len)--;

  return err;
}

/*
 * Generates markdown for NODE_BULLET_LIST_ITEM.
 */
static int
bullet_list_item_block_dumper (dumping_params_t *params)
{
  int err = 0;

  for (size_t i = 0; i < params->node->subtype - 1; i++)
    {
      snprintf (*params->writing_ptr, *params->max_len, "  ");
      *params->writing_ptr += 2;
      *params->max_len -= 2;
    }

  snprintf (*params->writing_ptr, *params->max_len, "*");
  (*params->writing_ptr)++;
  (*params->max_len)--;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : bullet_list_item_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n");
  (*params->writing_ptr)++;
  (*params->max_len)--;

  return err;
}

/*
 * Generates markdown for NODE_DEFINITION_LIST.
 */
static int
definition_list_block_dumper (dumping_params_t *params)
{
  int err = 0;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : definition_list_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n");
  *params->writing_ptr += 1;
  *params->max_len -= 1;

  return err;
}

/*
 * Generates markdown for NODE_DEFINITION_LIST_TERM.
 */
static int
definition_list_term_block_dumper (dumping_params_t *params)
{
  int err = 0;

  if (*params->writing_ptr - params->start_of_buffer >= 2)
    if ((*params->writing_ptr - 1)[0] == '\n' && (*params->writing_ptr - 2)[0] != '\n')
      {
        snprintf (*params->writing_ptr, *params->max_len, "\n");
        *params->writing_ptr += 1;
        *params->max_len -= 1;
      }

  snprintf (*params->writing_ptr, *params->max_len, "**");
  *params->writing_ptr += 2;
  *params->max_len -= 2;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : definition_list_term_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "**\n\n");
  *params->writing_ptr += 4;
  *params->max_len -= 4;

  return err;
}

/*
 * Generates markdown for NODE_DEFINITION_LIST_DEFINITION.
 */
static int
definition_list_definition_block_dumper (dumping_params_t *params)
{
  int err = 0;
  snprintf (*params->writing_ptr, *params->max_len, "* ");
  *params->writing_ptr += 2;
  *params->max_len -= 2;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : definition_list_definition_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n");
  *params->writing_ptr += 1;
  *params->max_len -= 1;

  return err;
}

/*
 * Generates markdown for NODE_GALLERY.
 */
static int
gallery_block_dumper (dumping_params_t *params)
{
  int err = 0;

  snprintf (*params->writing_ptr, *params->max_len, "\n");
  (*params->writing_ptr)++;
  (*params->max_len)--;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : gallery_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n");
  (*params->writing_ptr)++;
  (*params->max_len)--;

  return err;
}

/*
 * Generates markdown for NODE_GALLERY_ITEM.
 */
static int
gallery_item_block_dumper (dumping_params_t *params)
{
  int err = 0;

  err = dump_media (params);
  if (err)
    {
      fprintf (stderr, "dumper.c : gallery_item_block_dumper() : error while dumping media.\n");
      return err;
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n");
  (*params->writing_ptr)++;
  (*params->max_len)--;

  return err;
}

/*
 * Generates markdown for NODE_GALLERY_ITEM.
 */
static int
heading_block_dumper (dumping_params_t *params)
{
  int err = 0;

  for (size_t i = 0; i < params->node->subtype; i++)
    {
      snprintf (*params->writing_ptr, *params->max_len, "#");
      (*params->writing_ptr)++;
      (*params->max_len)--;
    }

  if (!params->node->children_len || params->node->children[0]->type != NODE_TEXT || !params->node->children[0]->text_content || !isspace (params->node->children[0]->text_content[0]))
    {
      snprintf (*params->writing_ptr, *params->max_len, " ");
      (*params->writing_ptr)++;
      (*params->max_len)--;
    }

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : heading_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n\n");
  *params->writing_ptr += 2;
  *params->max_len -= 2;

  return err;
}

/*
 * Generates markdown for NODE_HORIZONTAL_RULE.
 */
static int
horizontal_rule_block_dumper (dumping_params_t *params)
{
  (void) params->node;
  snprintf (*params->writing_ptr, *params->max_len, "--\n\n");
  *params->writing_ptr += 4;
  *params->max_len -= 4;

  return 0;
}

/*
 * Generates markdown for NODE_NUMBERED_LIST.
 */
static int
numbered_list_block_dumper (dumping_params_t *params)
{
  int err = 0;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : numbered_list_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n");
  (*params->writing_ptr)++;
  (*params->max_len)--;

  return 0;
}

/*
 * Generates markdown for NODE_NUMBERED_LIST_ITEM.
 */
static int
numbered_list_item_block_dumper (dumping_params_t *params)
{
  int err = 0;

  for (size_t i = 0; i < params->node->subtype - 1; i++)
    {
      snprintf (*params->writing_ptr, *params->max_len, "  ");
      *params->writing_ptr += 2;
      *params->max_len -= 2;
    }

  snprintf (*params->writing_ptr, *params->max_len, "#");
  (*params->writing_ptr)++;
  (*params->max_len)--;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : numbered_list_item_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n");
  (*params->writing_ptr)++;
  (*params->max_len)--;

  return err;
}

/*
 * Generates markdown for NODE_PREFORMATTED_TEXT.
 */
static int
preformated_text_block_dumper (dumping_params_t *params)
{
  int err = 0;

  snprintf (*params->writing_ptr, *params->max_len, "<pre>\n");
  *params->writing_ptr += 6;
  *params->max_len -= 6;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : preformated_text_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "</pre>\n\n");
  *params->writing_ptr += 8;
  *params->max_len -= 8;

  return 0;
}

/*
 * Generates markdown for NODE_TABLE.
 */
static int
table_block_dumper (dumping_params_t *params)
{
  int err = 0;

  if (params->node->children_len == 0)
    {
      fprintf (stderr, "dumper.c : table_block_dumper() : empty table.\n");
      return 1;
    }

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      node_t *child = params->node->children[i];
      if (child->is_block_level && child->type == NODE_TABLE_CAPTION && child->children_len > 0 && !child->children[0]->is_block_level && child->children[0]->type == NODE_TEXT)
        {
          char caption[*params->max_len];
          memset (caption, 0, *params->max_len);
          snprintf (caption, *params->max_len - 1, "**%s**\n\n", child->children[0]->text_content);
          snprintf (*params->writing_ptr, *params->max_len, "%s", caption);
          *params->writing_ptr += strlen (caption);
          *params->max_len -= strlen (caption);
        }
    }

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      node_t *child = params->node->children[i];
      if ((!child->is_block_level && child->type == NODE_TEXT) || (child->is_block_level && child->type == NODE_TABLE_CAPTION))
        continue;

      dumping_params_t child_params = {
        .node = child,
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : table_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  return 0;
}

/*
 * Generates markdown for NODE_TABLE_CAPTION.
 *
 * NOOP: caption is managed in table_block_dumper.
 */
static int
table_caption_block_dumper (dumping_params_t *params)
{
  (void) params->node;
  (void) params->writing_ptr;
  (void) params->max_len;
  return 0;
}

/*
 * Generates markdown for NODE_TABLE_ROW.
 */
static int
table_row_block_dumper (dumping_params_t *params)
{
  int err = 0;
  bool is_header = false;
  size_t col_count = 0;

  if (params->node->children_len == 0)
    {
      fprintf (stderr, "dumper.c : table_row_block_dumper() : empty row.\n");
      return 1;
    }

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      node_t *child = params->node->children[i];

      if (!child->is_block_level && child->type == NODE_TEXT && strlen (child->text_content) == 0)
        continue;

      col_count++;

      if (!child->is_block_level && child->type == NODE_TABLE_HEADER)
        is_header = true;

      dumping_params_t child_params = {
        .node = child,
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : paragraph_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  if (is_header)
    {
      snprintf (*params->writing_ptr, *params->max_len, "\n--");
      *params->writing_ptr += 3;
      *params->max_len -= 3;

      if (col_count > 1)
        for (size_t i = 0; i < col_count - 1; i++)
          {
            snprintf (*params->writing_ptr, *params->max_len, "|--");
            *params->writing_ptr += 3;
            *params->max_len -= 3;
          }
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n");
  (*params->writing_ptr)++;
  (*params->max_len)--;

  return 0;
}

/*
 * Generates markdown for NODE_PARAGRAPH.
 */
static int
paragraph_block_dumper (dumping_params_t *params)
{
  int err = 0;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : paragraph_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "\n\n");
  *params->writing_ptr += 2;
  *params->max_len -= 2;

  return 0;
}

/*
 * Generates markdown for NODE_EMPHASIS.
 */
static int
emphasis_inline_dumper (dumping_params_t *params)
{
  int err = 0;

  snprintf (*params->writing_ptr, *params->max_len, "_");
  *params->writing_ptr += 1;
  *params->max_len -= 1;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : strong_and_emphasis_inline_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "_");
  *params->writing_ptr += 1;
  *params->max_len -= 1;

  return err;
}

/*
 * Generates markdown for NODE_EXTERNAL_LINK.
 *
 * More parsing is done here to match the various components
 * of the links.
 */
static int
external_link_inline_dumper (dumping_params_t *params)
{
  int err = 0;

  char link_def[MAX_LINK_LENGTH] = {0};
  char *link_ptr = link_def;
  size_t link_max_len = MAX_LINK_LENGTH;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = &link_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = &link_max_len,
      };

      int err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : external_link_inline_dumper() : error while processing link content.\n");
          return err;
        }
    }

  if (strlen (link_def) == 0)
    {
      fprintf (stderr, "dumper.c : external_link_inline_dumper() : warning : empty link detected.\n");
      return 1;
    }

  char *text = strstr (link_def, " ");
  char url[MAX_LINK_LENGTH] = {0};
  char escaped_url[MAX_LINK_LENGTH] = {0};
  snprintf (url, (text ? (size_t) (text - link_def) : strlen (link_def)) + 1, "%s", link_def);
  escape_url_for_markdown (url, escaped_url);

  if (text)
    while (text[0] == ' ')
      text++;

  if (!text || !strlen (text))
    text = url;

  size_t out_len = strlen (text) + strlen (escaped_url) + 4;
  snprintf (*params->writing_ptr, *params->max_len, "[%s](%s)", text, escaped_url);
  *params->writing_ptr += out_len;
  *params->max_len -= out_len;

  return err;
}

/*
 * Generates markdown for NODE_INLINE_TEMPLATE.
 */
static int
template_inline_dumper (dumping_params_t *params)
{
  int err = 0;

  snprintf (*params->writing_ptr, *params->max_len, "<code>{{");
  *params->writing_ptr += 8;
  *params->max_len -= 8;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : template_inline_dumper() : error while dumping child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "}}</code>");
  *params->writing_ptr += 9;
  *params->max_len -= 9;

  return err;
}

/*
 * Generates markdown for NODE_INTERNAL_LINK.
 *
 * More parsing is done here to match the various components
 * of the links.
 */
static int
internal_link_inline_dumper (dumping_params_t *params)
{
  int err = 0;
  char link_def[MAX_LINK_LENGTH] = {0};
  char *link_ptr = link_def;
  size_t link_max_len = MAX_LINK_LENGTH;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = &link_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = &link_max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : internal_link_inline_dumper() : error while processing link content.\n");
          return 1;
        }
    }

  if (strlen (link_def) == 0)
    {
      fprintf (stderr, "dumper.c : internal_link_inline_dumper() : warning : empty link detected.\n");
      return 1;
    }

  char *text = strstr (link_def, "|");
  char url[MAX_LINK_LENGTH] = {0};
  char escaped_url[MAX_LINK_LENGTH] = {0};
  snprintf (url, (text ? (size_t) (text - link_def) : strlen (link_def)) + 1, "%s", link_def);
  escape_url_for_markdown (url, escaped_url);

  if (text)
    text++;

  if (!text || !strlen (text))
    text = url;

  size_t out_len = strlen (text) + strlen (escaped_url) + 7;
  snprintf (*params->writing_ptr, *params->max_len, "[%s](%s.md)", text, escaped_url);
  *params->writing_ptr += out_len;
  *params->max_len -= out_len;

  return err;
}

/*
 * Generates markdown for NODE_MEDIA.
 */
static int
media_inline_dumper (dumping_params_t *params)
{
  return dump_media (params);
}

/*
 * Generates markdown for NODE_STRONG.
 */
static int
strong_inline_dumper (dumping_params_t *params)
{
  int err = 0;

  snprintf (*params->writing_ptr, *params->max_len, "**");
  *params->writing_ptr += 2;
  *params->max_len -= 2;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : strong_inline_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "**");
  *params->writing_ptr += 2;
  *params->max_len -= 2;

  return err;
}

/*
 * Generates markdown for NODE_STRONG_AND_EMPHASIS.
 */
static int
strong_and_emphasis_inline_dumper (dumping_params_t *params)
{
  int err = 0;

  snprintf (*params->writing_ptr, *params->max_len, "**_");
  *params->writing_ptr += 3;
  *params->max_len -= 3;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : strong_and_emphasis_inline_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*params->writing_ptr, *params->max_len, "_**");
  *params->writing_ptr += 3;
  *params->max_len -= 3;

  return err;
}

/*
 * Generates markdown for NODE_TABLE_HEADER.
 */
static int
table_header_inline_dumper (dumping_params_t *params)
{
  int err = 0;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : table_cell_inline_dumper() : can't dump child.\n");
          return err;
        }
    }

  if (params->node != params->node->parent->last_child)
    {
      snprintf (*params->writing_ptr, *params->max_len, "|");
      *params->writing_ptr += 1;
      *params->max_len -= 1;
    }


  return err;
}

/*
 * Generates markdown for NODE_TABLE_CELL.
 */
static int
table_cell_inline_dumper (dumping_params_t *params)
{
  int err = 0;

  for (size_t i = 0; i < params->node->children_len; i++)
    {
      dumping_params_t child_params = {
        .node = params->node->children[i],
        .writing_ptr = params->writing_ptr,
        .start_of_buffer = params->start_of_buffer,
        .max_len = params->max_len,
      };

      err = dump (&child_params);
      if (err)
        {
          fprintf (stderr, "dumper.c : table_cell_inline_dumper() : can't dump child.\n");
          return err;
        }
    }

  if (params->node != params->node->parent->last_child)
    {
      snprintf (*params->writing_ptr, *params->max_len, "|");
      *params->writing_ptr += 1;
      *params->max_len -= 1;
    }


  return err;
}

/*
 * Generates markdown for NODE_TEXT.
 */
static int
text_inline_dumper (dumping_params_t *params)
{
  int err = 0;

  size_t len = strlen (params->node->text_content);
  if (*params->max_len < len)
    {
      fprintf (stderr, "dumper.c : text_inline_dumper() : output content too long.\n");
      return 1;
    }

  snprintf (*params->writing_ptr, *params->max_len, "%s", params->node->text_content);
  *params->writing_ptr += len;
  *params->max_len -= len;

  return err;
}

dumper_def_t block_dumpers[BLOCK_LEVEL_NODES_COUNT] = {
  { .type = NODE_BLOCKLEVEL_TEMPLATE, .handler = template_block_dumper },
  { .type = NODE_BULLET_LIST, .handler = bullet_list_block_dumper },
  { .type = NODE_BULLET_LIST_ITEM, .handler = bullet_list_item_block_dumper },
  { .type = NODE_DEFINITION_LIST, .handler = definition_list_block_dumper },
  { .type = NODE_DEFINITION_LIST_TERM, .handler = definition_list_term_block_dumper },
  { .type = NODE_DEFINITION_LIST_DEFINITION, .handler = definition_list_definition_block_dumper },
  { .type = NODE_GALLERY, .handler = gallery_block_dumper },
  { .type = NODE_GALLERY_ITEM, .handler = gallery_item_block_dumper },
  { .type = NODE_HEADING, .handler = heading_block_dumper },
  { .type = NODE_HORIZONTAL_RULE, .handler = horizontal_rule_block_dumper },
  { .type = NODE_NUMBERED_LIST, .handler = numbered_list_block_dumper },
  { .type = NODE_NUMBERED_LIST_ITEM, .handler = numbered_list_item_block_dumper },
  { .type = NODE_PREFORMATTED_TEXT, .handler = preformated_text_block_dumper },
  { .type = NODE_TABLE, .handler = table_block_dumper },
  { .type = NODE_TABLE_CAPTION, .handler = table_caption_block_dumper },
  { .type = NODE_TABLE_ROW, .handler = table_row_block_dumper },
  { .type = NODE_PARAGRAPH, .handler = paragraph_block_dumper },
};

dumper_def_t inline_dumpers[INLINE_NODES_COUNT] = {
  { .type = NODE_EMPHASIS, .handler = emphasis_inline_dumper },
  { .type = NODE_EXTERNAL_LINK, .handler = external_link_inline_dumper },
  { .type = NODE_INLINE_TEMPLATE, .handler = template_inline_dumper },
  { .type = NODE_INTERNAL_LINK, .handler = internal_link_inline_dumper },
  { .type = NODE_MEDIA, .handler = media_inline_dumper },
  { .type = NODE_STRONG, .handler = strong_inline_dumper },
  { .type = NODE_STRONG_AND_EMPHASIS, .handler = strong_and_emphasis_inline_dumper },
  { .type = NODE_TABLE_HEADER, .handler = table_header_inline_dumper },
  { .type = NODE_TABLE_CELL, .handler = table_cell_inline_dumper },
  { .type = NODE_TEXT, .handler = text_inline_dumper },
};

/*
 * Convert given node to markdown and output it.
 *
 * The converted content will be dumped into `writing_ptr`, up
 * to max_len. `writing_ptr` will be advanced to the next position
 * of available space, and `max_len` will be reduced by the written
 * content length.
 */
int
dump (dumping_params_t *params)
{
  int err = 0;

  if (*params->max_len < 10)
    {
      fprintf (stderr, "dumper.c : dump() : output content too long.\n");
      return 1;
    }

  if (params->node->is_block_level)
    {
      if (params->node->type == NODE_ROOT)
        {
          for (size_t i = 0; i < params->node->children_len; i++)
            {
              dumping_params_t child_params = {
                .node = params->node->children[i],
                .writing_ptr = params->writing_ptr,
                .start_of_buffer = params->start_of_buffer,
                .max_len = params->max_len,
              };

              err = dump (&child_params);
              if (err)
                {
                  fprintf (stderr, "dumper.c : dump() : error while dumping top level node.\n");
                  return err;
                }
            }
        }
      else
        {
          bool found = false;
          for (size_t i = 0; i < BLOCK_LEVEL_NODES_COUNT; i++)
            {
              dumper_def_t def = block_dumpers[i];
              if (def.type == params->node->type)
                {
                  found = true;

                  err = def.handler (params);
                  if (err)
                    {
                      fprintf (stderr, "dumper.c : dump() : error while dumping block level node.\n");
                      return err;
                    }
                  break;
                }
            }

          if (!found)
            {
              fprintf (stderr, "dumper.c : dump() : unknown node type : %ld\n", params->node->type);
              return 1;
            }
        }
    }
  else
    {
      bool found = false;
      for (size_t i = 0; i < INLINE_NODES_COUNT; i++)
        {
          dumper_def_t def = inline_dumpers[i];
          if (def.type == params->node->type)
            {
              found = true;

              err = def.handler (params);
              if (err)
                {
                  fprintf (stderr, "dumper.c : dump() : error while dumping inline node.\n");
                  return err;
                }
              break;
            }
        }

      if (!found)
        {
          fprintf (stderr, "dumper.c : dump() : unknown inline node type : %ld\n", params->node->type);
          return 1;
        }
    }

  return err;
}
