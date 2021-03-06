#include<stdio.h>
#include<assert.h>

int main()
{
    int rs, rt, dsp;
    int ach = 0, acl = 0;
    int resulth, resultl, resultdsp;

    rs        = 0x800000FF;
    rt        = 0x80000002;
    resulth   = 0x7FFFFFFF;
    resultl   = 0xFFFFFFFF;
    resultdsp = 0x01;
    __asm
        ("mthi        %0, $ac1\n\t"
         "mtlo        %0, $ac1\n\t"
         "dpaq_sa.l.w $ac1, %3, %4\n\t"
         "mfhi        %0,   $ac1\n\t"
         "mflo        %1,   $ac1\n\t"
         "rddsp       %2\n\t"
         : "+r"(ach), "+r"(acl), "=r"(dsp)
         : "r"(rs), "r"(rt)
        );
    dsp = (dsp >> 17) & 0x01;
    assert(dsp == resultdsp);
    assert(ach == resulth);
    assert(acl == resultl);

    return 0;
}
