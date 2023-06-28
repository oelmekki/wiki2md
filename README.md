# wiki2md

> Note to Github users : development is happening on [Gitlab](https://gitlab.com/oelmekki/wiki2md),
> please go there if you want to open issues or submit merge request.

Wiki2md is a parser converting mediawiki syntax to markdown.

## Why?

I use that to keep local copies of my favorite wikis as markdown files,
since I've already implemented support for markdown in my browser and I'm
familiar with reading the format in plain text. Probably an edge case, but
hey, if you need to convert wiki markup to markdown, it's here. There's a
FOSS project for that™.

## Dependencies

* make
* gcc (you can use an other compatible compiler using the `CC` env variable)

## Installation

To install it:

```shell
make                          # the binary is in ./wiki2md
make install                  # install in /usr/local/bin
make install PREFIX=~/bin     # if you want to install somewhere else
```

## Usage

```shell
wiki2md file.wiki > file.md
```

## Limitations / Todo

* [ ] wiki2md does not handle embedded mixed type lists, like putting a
  numbered list into the item of an unordered list, or a ordered list into
  a definition list.
* [ ] wiki2md does not handle references from
  [Extension:Cite](https://www.mediawiki.org/wiki/Special:MyLanguage/Extension:Cite)
* [ ] wiki2md does not handle templates

I'm not sure yet if it will ever handle templates. Showing template code is
actually more useful than trying to parse it and failing, like the software
I use previously did (they're not to blame, users can be wild with how they
stretch mediawiki features, especially on Fandom). On the other hand, maybe
I can detect it will fail and dump the raw code only in that case. We'll
see when we get there.
