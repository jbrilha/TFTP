#include "utils.h"

char *str_tolower(char *str) {
    for (char *s = str; *s; s++)
        *s = tolower(*s);

    return str;
}

char *str_toupper(char *str) {
    for (char *s = str; *s; s++)
        *s = toupper(*s);

    return str;
}

// https://stackoverflow.com/questions/656542/trim-a-string-in-c
char *ltrim(char *s) {
    while (isspace(*s))
        s++;
    return s;
}

char *rtrim(char *s) {
    char *back = s + strlen(s);
    while (isspace(*--back))
        ;
    *(back + 1) = '\0';
    return s;
}

char *trim(char *s) { return rtrim(ltrim(s)); }
