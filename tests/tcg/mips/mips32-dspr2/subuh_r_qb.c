#include<stdio.h>
#include<assert.h>

int main()
{
    int rd, rs, rt;
    int result;

    rs = 0x12345678;
    rt = 0x87654321;
    result = 0xC6E80A2C;

    __asm
        ("subuh_r.qb %0, %1, %2\n\t"
         : "=r"(rd)
         : "r"(rs), "r"(rt)
        );
    assert(rd == result);

    return 0;
}
