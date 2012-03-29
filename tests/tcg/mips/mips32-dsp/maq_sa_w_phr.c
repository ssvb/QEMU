#include<stdio.h>
#include<assert.h>

int main()
{
    int rt, rs;
    int achi, acli;
    int acho, aclo;
    int resulth, resultl;

    achi = 0x05;
    acli = 0xB4CB;
    rs  = 0xFF06;
    rt  = 0xCB00;
    resulth = 0xFFFFFFFF;
    resultl = 0x80000000;

    __asm
        ("mthi %2, $ac1\n\t"
         "mtlo %3, $ac1\n\t"
         "maq_sa.w.phr $ac1, %4, %5\n\t"
         "mfhi %0, $ac1\n\t"
         "mflo %1, $ac1\n\t"
         : "=r"(acho), "=r"(aclo)
         : "r"(achi), "r"(acli), "r"(rs), "r"(rt)
        );
    assert(resulth == acho);
    assert(resultl == aclo);

    return 0;
}
