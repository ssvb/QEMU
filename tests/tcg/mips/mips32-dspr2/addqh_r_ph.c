#include<stdio.h>
#include<assert.h>

int main()
{
    int rd, rs, rt;
    int result;

    rs     = 0x706A13FE;
    rt     = 0x13065174;
    result = 0x41B832B9;
    __asm
        ("addqh_r.ph %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(rd == result);

    rs     = 0x01000100;
    rt     = 0x02000100;
    result = 0x01800100;
    __asm
        ("addqh_r.ph %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(rd == result);

    return 0;
}
