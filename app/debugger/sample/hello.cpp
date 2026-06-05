#include <cstdio>

int add(int a, int b)
{
    int s = a + b;
    return s;
}

int main()
{
    int x = add(2, 3);
    std::printf("%d\n", x);
    return x;
}
