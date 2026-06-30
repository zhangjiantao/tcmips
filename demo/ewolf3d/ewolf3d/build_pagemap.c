#include "wl_def.h"

int main()
{
    uint16_t header[3];
    uint32_t offsets[1024];
    uint32_t size;
    uint32_t last;
    int i;
    FILE *f = fopen("vswap." GAMEEXT, "rb");
    if (!f)
      return 1;
    fread(header, 2, 3, f);

    printf("#include \"wl_def.h\"\n");
    printf("const myint PMSpriteStart = %d;\n", header[1]);
#ifdef ENABLE_AUDIO
    printf("const myint PMSoundStart = %d;\n", header[2]);
    printf("pool_id PageAddr[%d];\n", header[0]);
    last = header[0];
    printf("const PageListStruct PMPages[%d] = {\n", last);
#else
#ifdef ENABLE_COLOR
    printf("pool_id PageAddr[%d];\n", header[2]);
#else
    printf("pool_id PageAddr[%d];\n", header[1]);
    printf("#ifdef HOST\n");
#endif
    last = header[2];
    printf("const PageListStruct PMPages[%d] = {\n", last);
#endif
    for (i = 0; i < last; i++)
      {
	fread(&offsets[i], 4, 1, f);
	if (offsets[i] & 0xff0000ffu)
	  {
	    fprintf(stderr, "Bad page offset %x\n", offsets[i]);
	    return 1;
	  }
      }
    fseek(f, (header[0] - last) * 4, SEEK_CUR);
    for (i = 0; i < last; i++)
      {
	fread(&size, 2, 1, f);
	printf("{%d, %d},\n", offsets[i] >> 8, size);
      }
    printf("};\n");
#ifndef ENABLE_OLOR
    printf("#endif\n");
#endif
    return 0;
}

