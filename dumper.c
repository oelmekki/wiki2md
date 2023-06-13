#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

#define MAX_LINE_LENGTH 10000
#define MAX_LINK_LENGTH 5000

#define SUPPORTED_IMAGE_FORMATS 7
const char *image_formats[SUPPORTED_IMAGE_FORMATS] = { ".jpg", ".jpeg", ".png", ".gif", ".webp", ".svg", ".tiff" };

int dump (node_t *node, char **writing_ptr, size_t *max_len);

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
          fprintf (stderr, "dump_media() : error while processing link content.\n");
          return 1;
        }
    }

  if (strlen (link_def) == 0)
    {
      fprintf (stderr, "dump_media() : warning : empty link detected.\n");
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

/*
 * Convert a mediawiki internal link to markdown.
 *
 * More parsing is done here to match the various components
 * of the links.
 */
static int
dump_internal_link (node_t *node, char **writing_ptr, size_t *max_len)
{
  char link_def[MAX_LINK_LENGTH] = {0};
  char *link_ptr = link_def;
  size_t link_max_len = MAX_LINK_LENGTH;

  for (size_t i = 0; i < node->children_len; i++)
    {
      int err = dump (node->children[i], &link_ptr, &link_max_len);
      if (err)
        {
          fprintf (stderr, "dump_internal_link() : error while processing link content.\n");
          return 1;
        }
    }

  if (strlen (link_def) == 0)
    {
      fprintf (stderr, "dump_internal_link() : warning : empty link detected.\n");
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

  return 0;
}

/*
 * Convert a mediawiki external link to markdown.
 *
 * More parsing is done here to match the various components
 * of the links.
 */
static int
dump_external_link (node_t *node, char **writing_ptr, size_t *max_len)
{
  char link_def[MAX_LINK_LENGTH] = {0};
  char *link_ptr = link_def;
  size_t link_max_len = MAX_LINK_LENGTH;

  for (size_t i = 0; i < node->children_len; i++)
    {
      int err = dump (node->children[i], &link_ptr, &link_max_len);
      if (err)
        {
          fprintf (stderr, "dump_external_link() : error while processing link content.\n");
          return 1;
        }
    }

  if (strlen (link_def) == 0)
    {
      fprintf (stderr, "dump_external_link() : warning : empty link detected.\n");
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

  return 0;
}

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
  if (*max_len < 10)
    {
      fprintf (stderr, "dump() : output content too long.\n");
      return 1;
    }

  switch (node->type)
    {
      case NODE_ROOT:
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);
        break;

      case NODE_TEXT:
        {
          size_t len = strlen (node->text_content);
          if (*max_len < len)
            {
              fprintf (stderr, "dump() : output content too long.\n");
              return 1;
            }

          snprintf (*writing_ptr, *max_len, "%s", node->text_content);
          *writing_ptr += len;
          *max_len -= len;
          break;
        }

      case NODE_PARAGRAPH:
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "\n\n");
        *writing_ptr += 2;
        *max_len -= 2;
        break;

      case NODE_HEADING:
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
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "\n\n");
        *writing_ptr += 2;
        *max_len -= 2;
        break;

      case NODE_BLOCKLEVEL_TEMPLATE:
        snprintf (*writing_ptr, *max_len, "<pre>{{");
        *writing_ptr += 7;
        *max_len -= 7;

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "}}<pre>\n\n");
        *writing_ptr += 9;
        *max_len -= 9;
        break;

      case NODE_HORIZONTAL_RULE:
        snprintf (*writing_ptr, *max_len, "--\n\n");
        *writing_ptr += 4;
        *max_len -= 4;
        break;

      case NODE_BULLET_LIST:
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        printf ("\n");
        break;

      case NODE_BULLET_LIST_ITEM:
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
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "\n");
        (*writing_ptr)++;
        (*max_len)--;
        break;

      case NODE_NUMBERED_LIST:
        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "\n");
        (*writing_ptr)++;
        (*max_len)--;
        break;

      case NODE_NUMBERED_LIST_ITEM:
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
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "\n");
        (*writing_ptr)++;
        (*max_len)--;
        break;

      case NODE_DEFINITION_LIST:
        snprintf (*writing_ptr, *max_len, "<dl>\n");
        *writing_ptr += 5;
        *max_len -= 5;

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "</dl>\n");
        *writing_ptr += 6;
        *max_len -= 6;
        break;

      case NODE_DEFINITION_LIST_TERM:
        snprintf (*writing_ptr, *max_len, "<dt>");
        *writing_ptr += 4;
        *max_len -= 4;

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "</dt>\n");
        *writing_ptr += 6;
        *max_len -= 6;
        break;

      case NODE_DEFINITION_LIST_DEFINITION:
        snprintf (*writing_ptr, *max_len, "<dd>");
        *writing_ptr += 4;
        *max_len -= 4;

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "</dd>\n");
        *writing_ptr += 6;
        *max_len -= 6;
        break;

      case NODE_PREFORMATTED_TEXT:
        snprintf (*writing_ptr, *max_len, "<pre>\n");
        *writing_ptr += 6;
        *max_len -= 6;

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "</pre>\n\n");
        *writing_ptr += 8;
        *max_len -= 8;
        break;

      case NODE_GALLERY:
        snprintf (*writing_ptr, *max_len, "\n");
        (*writing_ptr)++;
        (*max_len)--;

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "\n");
        (*writing_ptr)++;
        (*max_len)--;
        break;

      case NODE_GALLERY_ITEM:
        dump_media (node, writing_ptr, max_len);
        snprintf (*writing_ptr, *max_len, "\n");
        (*writing_ptr)++;
        (*max_len)--;
        break;

      case NODE_INLINE_TEMPLATE:
        snprintf (*writing_ptr, *max_len, "<code>{{");
        *writing_ptr += 8;
        *max_len -= 8;

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "}}</code>");
        *writing_ptr += 9;
        *max_len -= 9;
        break;

      case NODE_STRONG_AND_EMPHASIS:
        snprintf (*writing_ptr, *max_len, "**_");
        *writing_ptr += 3;
        *max_len -= 3;

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "_**");
        *writing_ptr += 3;
        *max_len -= 3;
        break;

      case NODE_STRONG:
        snprintf (*writing_ptr, *max_len, "**");
        *writing_ptr += 2;
        *max_len -= 2;

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "**");
        *writing_ptr += 2;
        *max_len -= 2;
        break;

      case NODE_EMPHASIS:
        snprintf (*writing_ptr, *max_len, "_");
        *writing_ptr += 1;
        *max_len -= 1;

        for (size_t i = 0; i < node->children_len; i++)
          dump (node->children[i], writing_ptr, max_len);

        snprintf (*writing_ptr, *max_len, "_");
        *writing_ptr += 1;
        *max_len -= 1;
        break;

      case NODE_MEDIA:
        dump_media (node, writing_ptr, max_len);
        break;

      case NODE_INTERNAL_LINK:
        dump_internal_link (node, writing_ptr, max_len);
        break;

      case NODE_EXTERNAL_LINK:
        dump_external_link (node, writing_ptr, max_len);
        break;

      default:
        fprintf (stderr, "dump() : unknown node type : %ld\n", node->type);
        return 1;
    }

  return 0;
}
