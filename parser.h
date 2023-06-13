#ifndef _PARSER_H_
#define _PARSER_H_

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
  NODE_GALLERY,
  NODE_GALLERY_ITEM,
  NODE_HORIZONTAL_RULE,
  NODE_INLINE_TEMPLATE,
  NODE_NOWIKI,
  NODE_DEFINITION_LIST,
  NODE_DEFINITION_LIST_TERM,
  NODE_DEFINITION_LIST_DEFINITION,
  NODE_PREFORMATTED_TEXT,
  NODE_INTERNAL_LINK,
  NODE_EXTERNAL_LINK,
  NODE_MEDIA,
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

void free_node (node_t *node);
int parse (const char *filename, node_t *root);

#endif
