#include <stdio.h>
#include <stdlib.h>

extern char **environ;

int main() {
    char **p;
    for (p = environ; *p; ++p) {
        printf("%s\n", *p);
    }
    exit(0);
}
