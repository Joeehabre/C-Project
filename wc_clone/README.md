# wc_clone

A reimplementation of the Unix `wc` utility in C.

## Features

- `-l` — count lines
- `-w` — count words
- `-c` — count bytes
- Multiple flags can be combined: `-lw`, `-lwc`
- Default (no flags): prints all three counts
- Multiple files: prints per-file counts plus a `total` row
- Reads from stdin when no file is given

## Build

```bash
make        # produces ./wc_clone
make clean
```

## Usage

```bash
# Count everything (lines, words, bytes)
./wc_clone file.txt

# Count only lines
./wc_clone -l file.txt

# Count words across multiple files
./wc_clone -w *.txt

# Read from stdin
echo "hello world" | ./wc_clone

# Multiple files get a totals row
./wc_clone -lwc a.txt b.txt c.txt
```

Example output:
```
       3       9      42 file.txt
       2       5      28 other.txt
       5      14      70 total
```
