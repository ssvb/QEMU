#include<stdio.h>
#include<assert.h>

int main()
{
    int rd, rs, rt;
    int result;

    rs     = 0xFFFFFFFF;
    rt     = 0x10101010;
    result = 0x100F100F;
    __asm
        ("addq_s.ph   %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(result == rd);

    rs     = 0x3712847D;
    rt     = 0x0031AF2D;
    result = 0x37438000;
    __asm
        ("addq_s.ph   %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(result == rd);

    return 0;
}
