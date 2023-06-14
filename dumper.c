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

/*
 * Convert a mediawiki media link to markdown.
 *
 * More parsing is done here to match the various components
 * of the links.
 */
static int
dump_media (node_t *node, char **writing_ptr, size_t *max_len)
{
  char link_def[MAX_LINK_LENGTH] = {0};
  char *link_ptr = link_def;
  size_t link_max_len = MAX_LINK_LENGTH;

  for (size_t i = 0; i < node->children_len; i++)
    {
      int err = dump (node->children[i], &link_ptr, &link_max_len);
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

  size_t out_len = 0;

  if (is_image)
    {
      out_len = strlen (last_pipe) + strlen (url) + 5;
      snprintf (*writing_ptr, *max_len, "![%s](%s)", last_pipe, url);
    }
  else
    {
      out_len = strlen (last_pipe) + strlen (url) + 4;
      snprintf (*writing_ptr, *max_len, "[%s](%s)", last_pipe, url);
    }

  *writing_ptr += out_len;
  *max_len -= out_len;

  return 0;
}

typedef int (dumping_node_t) (node_t *node, char **writing_ptr, size_t *max_len);
typedef struct {
  size_t type; // only used to make `dumpers` more readable.
  dumping_node_t *handler;
} dumper_def_t;

/*
 * Generates markdown for NODE_BLOCKLEVEL_TEMPLATE.
 */
static int
template_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  snprintf (*writing_ptr, *max_len, "<pre>{{");
  *writing_ptr += 7;
  *max_len -= 7;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : template_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "}}<pre>\n\n");
  *writing_ptr += 9;
  *max_len -= 9;

  return err;
}

/*
 * Generates markdown for NODE_BULLET_LIST.
 */
static int
bullet_list_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : bullet_list_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  printf ("\n");
  return err;
}

/*
 * Generates markdown for NODE_BULLET_LIST_ITEM.
 */
static int
bullet_list_item_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  for (size_t i = 0; i < node->subtype - 1; i++)
    {
      snprintf (*writing_ptr, *max_len, "  ");
      *writing_ptr += 2;
      *max_len -= 2;
    }

  snprintf (*writing_ptr, *max_len, "*");
  (*writing_ptr)++;
  (*max_len)--;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : bullet_list_item_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "\n");
  (*writing_ptr)++;
  (*max_len)--;

  return err;
}

/*
 * Generates markdown for NODE_DEFINITION_LIST.
 */
static int
definition_list_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;
  snprintf (*writing_ptr, *max_len, "<dl>\n");
  *writing_ptr += 5;
  *max_len -= 5;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : definition_list_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "</dl>\n");
  *writing_ptr += 6;
  *max_len -= 6;

  return err;
}

/*
 * Generates markdown for NODE_DEFINITION_LIST_TERM.
 */
static int
definition_list_term_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  snprintf (*writing_ptr, *max_len, "<dt>");
  *writing_ptr += 4;
  *max_len -= 4;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : definition_list_term_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "</dt>\n");
  *writing_ptr += 6;
  *max_len -= 6;

  return err;
}

/*
 * Generates markdown for NODE_DEFINITION_LIST_DEFINITION.
 */
static int
definition_list_definition_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;
  snprintf (*writing_ptr, *max_len, "<dd>");
  *writing_ptr += 4;
  *max_len -= 4;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : definition_list_definition_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "</dd>\n");
  *writing_ptr += 6;
  *max_len -= 6;

  return err;
}

/*
 * Generates markdown for NODE_GALLERY.
 */
static int
gallery_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  snprintf (*writing_ptr, *max_len, "\n");
  (*writing_ptr)++;
  (*max_len)--;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : gallery_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "\n");
  (*writing_ptr)++;
  (*max_len)--;

  return err;
}

/*
 * Generates markdown for NODE_GALLERY_ITEM.
 */
static int
gallery_item_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  err = dump_media (node, writing_ptr, max_len);
  if (err)
    {
      fprintf (stderr, "dumper.c : gallery_item_block_dumper() : error while dumping media.\n");
      return err;
    }

  snprintf (*writing_ptr, *max_len, "\n");
  (*writing_ptr)++;
  (*max_len)--;

  return err;
}

/*
 * Generates markdown for NODE_GALLERY_ITEM.
 */
static int
heading_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  for (size_t i = 0; i < node->subtype; i++)
    {
      snprintf (*writing_ptr, *max_len, "#");
      (*writing_ptr)++;
      (*max_len)--;
    }

  if (!node->children_len || node->children[0]->type != NODE_TEXT || !node->children[0]->text_content || !isspace (node->children[0]->text_content[0]))
    {
      snprintf (*writing_ptr, *max_len, " ");
      (*writing_ptr)++;
      (*max_len)--;
    }

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : heading_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "\n\n");
  *writing_ptr += 2;
  *max_len -= 2;

  return err;
}

/*
 * Generates markdown for NODE_HORIZONTAL_RULE.
 */
static int
horizontal_rule_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  (void) node;
  snprintf (*writing_ptr, *max_len, "--\n\n");
  *writing_ptr += 4;
  *max_len -= 4;

  return 0;
}

/*
 * Generates markdown for NODE_NUMBERED_LIST.
 */
static int
numbered_list_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : numbered_list_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "\n");
  (*writing_ptr)++;
  (*max_len)--;

  return 0;
}

/*
 * Generates markdown for NODE_NUMBERED_LIST_ITEM.
 */
static int
numbered_list_item_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  for (size_t i = 0; i < node->subtype - 1; i++)
    {
      snprintf (*writing_ptr, *max_len, "  ");
      *writing_ptr += 2;
      *max_len -= 2;
    }

  snprintf (*writing_ptr, *max_len, "#");
  (*writing_ptr)++;
  (*max_len)--;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : numbered_list_item_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "\n");
  (*writing_ptr)++;
  (*max_len)--;

  return err;
}

/*
 * Generates markdown for NODE_PREFORMATTED_TEXT.
 */
static int
preformated_text_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  snprintf (*writing_ptr, *max_len, "<pre>\n");
  *writing_ptr += 6;
  *max_len -= 6;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : preformated_text_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "</pre>\n\n");
  *writing_ptr += 8;
  *max_len -= 8;

  return 0;
}

/*
 * Generates markdown for NODE_PARAGRAPH.
 */
static int
paragraph_block_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : paragraph_block_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "\n\n");
  *writing_ptr += 2;
  *max_len -= 2;

  return 0;
}

/*
 * Generates markdown for NODE_EMPHASIS.
 */
static int
emphasis_inline_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  snprintf (*writing_ptr, *max_len, "_");
  *writing_ptr += 1;
  *max_len -= 1;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : strong_and_emphasis_inline_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "_");
  *writing_ptr += 1;
  *max_len -= 1;

  return err;
}

/*
 * Generates markdown for NODE_EXTERNAL_LINK.
 *
 * More parsing is done here to match the various components
 * of the links.
 */
static int
external_link_inline_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  char link_def[MAX_LINK_LENGTH] = {0};
  char *link_ptr = link_def;
  size_t link_max_len = MAX_LINK_LENGTH;

  for (size_t i = 0; i < node->children_len; i++)
    {
      int err = dump (node->children[i], &link_ptr, &link_max_len);
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
  snprintf (url, (text ? (size_t) (text - link_def) : strlen (link_def)) + 1, "%s", link_def);

  if (text)
    while (text[0] == ' ')
      text++;

  if (!text || !strlen (text))
    text = url;

  size_t out_len = strlen (text) + strlen (url) + 4;
  snprintf (*writing_ptr, *max_len, "[%s](%s)", text, url);
  *writing_ptr += out_len;
  *max_len -= out_len;

  return err;
}

/*
 * Generates markdown for NODE_INLINE_TEMPLATE.
 */
static int
template_inline_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  snprintf (*writing_ptr, *max_len, "<code>{{");
  *writing_ptr += 8;
  *max_len -= 8;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : template_inline_dumper() : error while dumping child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "}}</code>");
  *writing_ptr += 9;
  *max_len -= 9;

  return err;
}

/*
 * Generates markdown for NODE_INTERNAL_LINK.
 *
 * More parsing is done here to match the various components
 * of the links.
 */
static int
internal_link_inline_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;
  char link_def[MAX_LINK_LENGTH] = {0};
  char *link_ptr = link_def;
  size_t link_max_len = MAX_LINK_LENGTH;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], &link_ptr, &link_max_len);
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
  snprintf (url, (text ? (size_t) (text - link_def) : strlen (link_def)) + 1, "%s", link_def);

  if (text)
    text++;

  if (!text || !strlen (text))
    text = url;

  size_t out_len = strlen (text) + strlen (url) + 7;
  snprintf (*writing_ptr, *max_len, "[%s](%s.md)", text, url);
  *writing_ptr += out_len;
  *max_len -= out_len;

  return err;
}

/*
 * Generates markdown for NODE_MEDIA.
 */
static int
media_inline_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  return dump_media (node, writing_ptr, max_len);
}

/*
 * Generates markdown for NODE_STRONG.
 */
static int
strong_inline_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  snprintf (*writing_ptr, *max_len, "**");
  *writing_ptr += 2;
  *max_len -= 2;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : strong_inline_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "**");
  *writing_ptr += 2;
  *max_len -= 2;

  return err;
}

/*
 * Generates markdown for NODE_STRONG_AND_EMPHASIS.
 */
static int
strong_and_emphasis_inline_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  snprintf (*writing_ptr, *max_len, "**_");
  *writing_ptr += 3;
  *max_len -= 3;

  for (size_t i = 0; i < node->children_len; i++)
    {
      err = dump (node->children[i], writing_ptr, max_len);
      if (err)
        {
          fprintf (stderr, "dumper.c : strong_and_emphasis_inline_dumper() : can't dump child.\n");
          return err;
        }
    }

  snprintf (*writing_ptr, *max_len, "_**");
  *writing_ptr += 3;
  *max_len -= 3;

  return err;
}

/*
 * Generates markdown for NODE_TEXT.
 */
static int
text_inline_dumper (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  size_t len = strlen (node->text_content);
  if (*max_len < len)
    {
      fprintf (stderr, "dumper.c : text_inline_dumper() : output content too long.\n");
      return 1;
    }

  snprintf (*writing_ptr, *max_len, "%s", node->text_content);
  *writing_ptr += len;
  *max_len -= len;

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
dump (node_t *node, char **writing_ptr, size_t *max_len)
{
  int err = 0;

  if (*max_len < 10)
    {
      fprintf (stderr, "dumper.c : dump() : output content too long.\n");
      return 1;
    }

  if (node->is_block_level)
    {
      if (node->type == NODE_ROOT)
        {
          for (size_t i = 0; i < node->children_len; i++)
            {
              err = dump (node->children[i], writing_ptr, max_len);
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
              if (def.type == node->type)
                {
                  found = true;
                  err = def.handler (node, writing_ptr, max_len);
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
              fprintf (stderr, "dumper.c : dump() : unknown node type : %ld\n", node->type);
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
          if (def.type == node->type)
            {
              found = true;
              err = def.handler (node, writing_ptr, max_len);
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
          fprintf (stderr, "dumper.c : dump() : unknown inline node type : %ld\n", node->type);
          return 1;
        }
    }

  return err;
}
