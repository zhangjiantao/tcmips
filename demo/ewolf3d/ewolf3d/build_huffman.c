#include "wl_def.h"

int main()
{
    unsigned char data[1024];
    int i;
    unsigned char *p;
    FILE *f = fopen("vgadict." GAMEEXT, "rb");
    if (!f)
      return 1;
    fread(data, 4, 256, f);
    printf("typedef struct {\n");
    printf("  byte val[2];\n");
    printf("  boolean flag0:1;\n");
    printf("  boolean flag1:1;\n");
    printf("} huffnode;\n");
    printf("static const huffnode grhuffman[256] = {\n");
    p = data;
    for (i = 0; i < 256; i++) {
	printf("{{%d,%d},%d,%d},\n", p[0], p[2], p[1]!=0, p[3]!=0);
	p += 4;
    }
    printf("};\n");
    return 0;
}

