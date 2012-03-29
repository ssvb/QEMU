#include<stdio.h>
#include<assert.h>

int main()
{
    int value, index, rd;
    int result;

    value  = 0xBCDEF389;
    index  = 28;
    result = 0xFFFFF389;
    __asm
        ("lw  $10, 28($fp)\n\t"
         "sw  %2,  28($fp)\n\t"
         "lhx %0, %1($fp)\n\t"
         "sw  $10, 28($fp)\n\t"
         : "=r"(rd)
         : "r"(index), "r"(value)
        );
    assert(rd == result);

    return 0;
}
