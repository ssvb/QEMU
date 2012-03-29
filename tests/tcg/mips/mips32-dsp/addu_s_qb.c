#include<stdio.h>
#include<assert.h>

int main()
{
    int rd, rs, rt;
    int result;

    rs     = 0x10FF01FF;
    rt     = 0x10010001;
    result = 0x20FF01FF;
    __asm
        ("addu_s.qb %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(rd == result);

    rs     = 0xFFFF1111;
    rt     = 0x00020001;
    result = 0xFFFF1112;
    __asm
        ("addu_s.qb %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(rd == result);

    return 0;
}
