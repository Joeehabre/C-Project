#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static void die(const char *msg){ perror(msg); exit(1); }

static void compress(FILE *in, FILE *out){
    int prev = fgetc(in);
    if (prev == EOF) return;
    uint8_t run = 1;
    for(;;){
        int cur = fgetc(in);
        if (cur == prev && run < 255){
            run++;
        } else {
            if (fputc(run, out) == EOF) die("write run");
            if (fputc(prev, out) == EOF) die("write byte");
            run = 1;
        }
        if (cur == EOF) break;
        prev = cur;
    }
}

static void decompress(FILE *in, FILE *out){
    int run;
    while ((run = fgetc(in)) != EOF){
        int byte = fgetc(in);
        if (byte == EOF) die("corrupt .rle (odd length)");
        for (int i=0;i<run;i++)
            if (fputc(byte, out) == EOF) die("write");
    }
}

int main(int argc, char **argv){
    if (argc != 4 || (argv[1][0] != 'c' && argv[1][0] != 'd')){
        fprintf(stderr, "usage: %s c|d <in> <out>\n", argv[0]);
        return 1;
    }
    FILE *in = fopen(argv[2], "rb"); if(!in) die("open input");
    FILE *out= fopen(argv[3], "wb"); if(!out) die("open output");
    if (argv[1][0]=='c') compress(in,out); else decompress(in,out);
    fclose(in); fclose(out);
    return 0;
}
