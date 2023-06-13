# wiki2md

Wiki2md is a parser converting mediawiki syntax to markdown.

> !WORK IN PROGRESS! I'm currently building wiki2md in the open, it's
> not ready for production usage yet, I'm just building in the open.

## Why?

I use that to keep local copies on my favorite wikis as markdown files,
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
