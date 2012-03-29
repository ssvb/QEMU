#include<stdio.h>
#include<assert.h>

int main()
{
    int rd, rs, rt;
    int result;

    rs     = 0x0000000F;
    rt     = 0x00000001;
    result = 0x00000010;
    __asm
        ("addsc %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(rd == result);

    rs     = 0xFFFF0FFF;
    rt     = 0x00010111;
    result = 0x00001110;
    __asm
        ("addsc %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(rd == result);

    return 0;
}
