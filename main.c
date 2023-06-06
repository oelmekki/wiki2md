#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 10000

static void
usage (const char *progname)
{
  printf ("\
%s [-h|--help] <wikitext-file> \n\
\n\
Convert the provided file in mediawiki markup to markdown. \n\
  ", progname);
}

typedef struct {
  int template;
  bool nowiki;
  bool definition_list;
} parser_memory_t;

/*
 * Replace wiki bold and italic markdup with markdown markup.
 */
static void
parse_inline_tag (char line[MAX_LINE_LENGTH], const char *wiki_open, const char *wiki_close, const char *md_open, const char *md_close)
{
  char result[MAX_LINE_LENGTH] = {0};
  char *ptr = line;
  char *result_ptr = result;
  int wiki_open_len = strlen (wiki_open);
  int wiki_close_len = strlen (wiki_close);
  int md_open_len = strlen (md_open);
  int md_close_len = strlen (md_close);

  while (true)
    {
      char *start = strstr (ptr, wiki_open);
      if (!start)
        break;

      if ((size_t) (start - line) + wiki_open_len +  wiki_close_len >= strlen (line))
        break;

      char *end = strstr (start + wiki_open_len, wiki_close);
      if (!end)
        break;

      snprintf (result_ptr, start - ptr + 1, "%s", ptr);
      result_ptr += start - ptr;
      snprintf (result_ptr, md_open_len + 1, "%s", md_open);
      result_ptr += md_open_len;
      ptr += (start - ptr) + wiki_open_len;
      snprintf (result_ptr, end - ptr + 1, "%s", ptr);
      result_ptr += end - start - wiki_close_len;
      snprintf (result_ptr, md_close_len + 1, "%s", md_close);
      result_ptr += md_close_len;
      ptr += end - start;
    }

  if ((size_t) (ptr - line) < strlen (line))
    snprintf (result_ptr, strlen (line) - (ptr - line) + 1, "%s", ptr);

  if (snprintf (line, MAX_LINE_LENGTH - 1, "%s", result) > MAX_LINE_LENGTH)
    fprintf (stderr, "parse_inline_tag() : warning : truncated line : %s.\n", line);
}

/*
 * Replace wiki headings with markdown headings.
 */
static void
parse_heading (char line[MAX_LINE_LENGTH])
{
  if (strncmp (line, "==", 2) != 0)
    return;

  size_t head_level = 1;
  char *ptr = line + 2;
  while (ptr[0] == '=')
    {
      head_level++;
      ptr++;
    }

  if (head_level > 6)
    return;

  char tag[10] = {0};
  for (size_t i = 0; i < head_level; i++)
    tag[i] = '#';

  char rest[MAX_LINE_LENGTH] = {0};
  snprintf (rest, MAX_LINE_LENGTH - 1, "%s", line + 1 + head_level);
  size_t len = strlen (rest);
  bool start_removing = false;
  for (size_t i = len - 1; i > 0; i--)
    {
      if (rest[i] == '=')
        {
          rest[i] = 0;
          start_removing = true;
          continue;
        }

      if (rest[i] != '=' && start_removing)
        break;
    }

  char original[MAX_LINE_LENGTH] = {0};
  snprintf (original, MAX_LINE_LENGTH - 1, "%s", line);
  if (snprintf (line, MAX_LINE_LENGTH - 1, "%s %s\n\n", tag, rest) > MAX_LINE_LENGTH)
    fprintf (stderr, "parse_heading() : warning: truncated line : %s\n", rest);
}

static int
convert (const char *filename)
{
  int err = 0;
  FILE *file = NULL;

  file = fopen (filename, "r");
  if (!file)
    {
      fprintf (stderr, "convert() : can't open file.\n");
      goto cleanup;
    }

  char line[MAX_LINE_LENGTH] = {0};

  while (1)
    {
      if (!fgets (line, MAX_LINE_LENGTH - 1, file))
        break;

      parse_heading (line);
      parse_inline_tag (line, "'''''", "'''''", "**_", "_**");
      parse_inline_tag (line, "'''", "'''", "**", "**");
      parse_inline_tag (line, "''", "''", "_", "_");

      printf ("%s", line);
    }

  cleanup:
  if (file) fclose (file);
  return err;
}

int
main (int argc, char **argv)
{
  int err = 0;

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

  err = convert (filename);
  if (err)
    {
      fprintf (stderr, "main() : error while converting file.\n");
      goto cleanup;
    }

  cleanup:
  return err;
}
