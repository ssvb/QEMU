#include<stdio.h>
#include<assert.h>

int main()
{
    int value, index, rd;
    int result;

    value  = 0xBCDEF389;
    index  = 28;
    result = value & 0xFF;
    __asm
        ("lw  $10, 28($fp)\n\t"
         "sw  %2,  28($fp)\n\t"
         "lbux %0, %1($fp)\n\t"
         "sw  $10, 28($fp)\n\t"
         : "=r"(rd)
         : "r"(index), "r"(value)
        );
    assert(rd == result);

    return 0;
}
