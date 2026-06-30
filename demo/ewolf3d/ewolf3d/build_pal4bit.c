#include "objs.c"

int main()
{
    int i;
    int val;

    printf("#include \"wl_def.h\"\n");
    //printf("#ifdef LUMINARY\n");
    printf("const byte pal4bit[256] = {\n");
    for (i = 0; i < 256; i++) {
	val = gamepal[i * 3] + gamepal[i * 3 + 1] + gamepal[i * 3 + 2];
	val = (val / 3) >> 2;
	printf("0x%02x,\n", val);
    }
    printf("};\n");
    //printf("#endif\n");
    return 0;
}

