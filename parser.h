#ifndef _PARSER_H_
#define _PARSER_H_

#define MAX_FILE_SIZE 500000

// block level nodes
enum {
  NODE_BLOCKLEVEL_TEMPLATE,               // 0
  NODE_BULLET_LIST,                       // 1
  NODE_BULLET_LIST_ITEM,                  // 2
  NODE_DEFINITION_LIST,                   // 3
  NODE_DEFINITION_LIST_DEFINITION,        // 4
  NODE_DEFINITION_LIST_TERM,              // 5
  NODE_GALLERY,                           // 6
  NODE_GALLERY_ITEM,                      // 7
  NODE_HEADING,                           // 8
  NODE_HORIZONTAL_RULE,                   // 9
  NODE_NUMBERED_LIST,                     // 10
  NODE_NUMBERED_LIST_ITEM,                // 11
  NODE_PARAGRAPH,                         // 12
  NODE_PREFORMATTED_TEXT,                 // 13
  NODE_TABLE,                             // 14
  NODE_TABLE_CAPTION,                     // 15
  NODE_TABLE_ROW,                         // 16
  BLOCK_LEVEL_NODES_COUNT,
  NODE_ROOT, // that one is special - 18
};

// inline nodes
enum {
  NODE_TEXT,                    // 0
  NODE_EMPHASIS,                // 1
  NODE_EXTERNAL_LINK,           // 2
  NODE_INLINE_TEMPLATE,         // 3
  NODE_INTERNAL_LINK,           // 4
  NODE_MEDIA,                   // 5
  NODE_STRONG,                  // 6
  NODE_STRONG_AND_EMPHASIS,     // 7
  NODE_TABLE_HEADER,            // 8
  NODE_TABLE_CELL,              // 9
  INLINE_NODES_COUNT,
  NODE_NOWIKI, // that one is a bit peculiar too - 11
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
