#include<stdio.h>
#include<assert.h>

int main()
{
    int rd, rs, rt;
    int result;

    rs     = 0x00FF00FF;
    rt     = 0x00010001;
    result = 0x00000000;
    __asm
        ("addu.qb %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(rd == result);

    rs     = 0xFFFF1111;
    rt     = 0x00020001;
    result = 0xFF011112;
    __asm
        ("addu.qb %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(rd == result);

    return 0;
}
