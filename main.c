#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* reverse_str(char *str)
{
      char *p1, *p2;

      if (! str || ! *str)
            return str;
      for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
      {
            *p1 ^= *p2;
            *p2 ^= *p1;
            *p1 ^= *p2;
      }
      return str;
}

int main() {
    char str[] = "test/test2/test3";

    int strSize = strlen(str);
    char* fileName = malloc(sizeof(char) * strSize);
    memset(fileName, '\0', strSize);
    int fileName_ctr = 0;

    for (int i = strSize - 1; i >= 0; --i) {
        if (str[i] != '/') {
            fileName[fileName_ctr++] = str[i];
            str[i] = '\0';
        } else {
            str[i] = '\0';
            fileName = reverse_str(fileName);
            break;
        }
    }

    printf("First: %s\nSecond: %s\n", str, fileName);
    free(fileName);
}