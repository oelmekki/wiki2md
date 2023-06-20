#ifndef _PARSER_H_
#define _PARSER_H_

#define MAX_FILE_SIZE 500000

// block level nodes
enum {
  NODE_BLOCKLEVEL_TEMPLATE,
  NODE_BULLET_LIST,
  NODE_BULLET_LIST_ITEM,
  NODE_DEFINITION_LIST,
  NODE_DEFINITION_LIST_DEFINITION,
  NODE_DEFINITION_LIST_TERM,
  NODE_GALLERY,
  NODE_GALLERY_ITEM,
  NODE_HEADING,
  NODE_HORIZONTAL_RULE,
  NODE_NUMBERED_LIST,
  NODE_NUMBERED_LIST_ITEM,
  NODE_PARAGRAPH,
  NODE_PREFORMATTED_TEXT,
  NODE_TABLE,
  NODE_TABLE_CAPTION,
  NODE_TABLE_ROW,
  BLOCK_LEVEL_NODES_COUNT,
  NODE_ROOT, // that one is special
};

// inline nodes
enum {
  NODE_TEXT,
  NODE_EMPHASIS,
  NODE_EXTERNAL_LINK,
  NODE_INLINE_TEMPLATE,
  NODE_INTERNAL_LINK,
  NODE_MEDIA,
  NODE_STRONG,
  NODE_STRONG_AND_EMPHASIS,
  NODE_TABLE_HEADER,
  NODE_TABLE_CELL,
  INLINE_NODES_COUNT,
  NODE_NOWIKI, // that one is a bit peculiar too
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

void append_child (node_t *parent, node_t *child);
int flush_text_buffer (node_t *current_node, char *buffer, char **buffer_ptr);
void free_node (node_t *node);
int parse (const char *filename, node_t *root);

#endif
