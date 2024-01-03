#ifndef _UTILS_H_
#define _UTILS_H_

bool is_empty_text_node (node_t *node);
void *xalloc (size_t len);
void *xrealloc (void *mem, size_t msize);
bool is_inline_block_template (char *reading_ptr);

#endif
