#include <stdio.h>
#include <ctype.h>

static void count(FILE *f, const char *name){
    unsigned long lines=0, words=0, bytes=0;
    int c, inword=0;
    while ((c=fgetc(f)) != EOF){
        bytes++;
        if (c=='\n') lines++;
        if (isspace((unsigned char)c)) inword=0;
        else if (!inword){ inword=1; words++; }
    }
    printf("%8lu %8lu %8lu %s\n", lines, words, bytes, name);
}

int main(int argc, char **argv){
    if (argc<2){
        count(stdin, "-");
        return 0;
    }
    for (int i=1;i<argc;i++){
        FILE *f = fopen(argv[i], "rb");
        if(!f){ perror(argv[i]); continue; }
        count(f, argv[i]); fclose(f);
    }
    return 0;
}
