#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "%s: file name not given\n", argv[0]);
        exit(1);
    }

    for (int i = 1; i < argc; ++i) {
        FILE *f;
        int c;

        f = fopen(argv[i], "r");
        if (!f) {
            perror(argv[i]);
            exit(1);
        }

        while ((c = fgetc(f)) != EOF) {
            if (putchar(c) < 0) {
                exit(1);
            }
        }
        fclose(f);
    }
    exit(0);
}
