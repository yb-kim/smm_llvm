#include <stdio.h>

typedef struct {int x;} X;

int x;

int main() {
    int a = 0;
    int *b = &a;
    int i;
    a++;
    int k = 2;
    X my_x;
    printf("x: %d, a: %d, k: %d, *b: %d\n", x, a, k, *b);
    printf("a+k: %d\n", a+k);
    for(i=0; i<10; i++) {
        printf("%d\n", my_x.x);
        printf("%d ", i);
    }
    return 0;
}
