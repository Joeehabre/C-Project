# rle_compressor

A binary-safe Run-Length Encoding (RLE) compressor and decompressor in C.

## How It Works

Each run of identical bytes is encoded as two bytes: `[count][byte]`.  
A run of 255 `A`s becomes `\xff A` (2 bytes instead of 255).  
Works on any binary file, not just text.

## Build

```bash
make        # produces ./rle
make clean
```

## Usage

```bash
# Compress
./rle c input.bin output.rle

# Decompress
./rle d output.rle restored.bin

# Use stdin / stdout with '-'
cat input.bin | ./rle c - - > output.rle
./rle d - restored.bin < output.rle
```

Stats are printed to stderr so piping stays clean:
```
compressed 14 -> 8 bytes (57.1%)
decompressed 8 -> 14 bytes
```

## Implementation Notes

- Single source file: `src/rle.c`
- Max run length: 255 (a new pair is emitted when the limit is reached)
- A run length of 0 in the input is treated as corrupt and rejected
- Output is flushed before close to catch late write errors
- Best suited for files with long repeated-byte sequences (e.g. bitmaps, sparse data)
