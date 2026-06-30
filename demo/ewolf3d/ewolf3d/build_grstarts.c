#include "wl_def.h"

int main()
{
    byte d[(NUMCHUNKS + 1) * 3];
    int i;
    FILE *f = fopen("vgahead." GAMEEXT, "rb");
    if (!f)
      return 1;
    fread(d, 3, NUMCHUNKS + 1, f);
    fclose(f);


    printf("#include \"wl_def.h\"\n");
    printf("#ifndef EMBEDDED\n");
    printf("const uint32_t grstarts[NUMCHUNKS + 1] = {\n");
    for (i = 0; i < NUMCHUNKS + 1; i++)
      {
	printf("0x%x,\n", (d[i*3+0]<<0)|(d[i*3+1]<<8)|(d[i*3+2]<<16));
      }
    printf("};\n");
    printf("#endif\n");
    return 0;
}

