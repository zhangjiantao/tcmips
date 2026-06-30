#include "wl_def.h"

int main()
{
    uint32_t offsets[60];
    struct {
	uint32_t offset[3];
	uint16_t size[3];
	uint16_t width;
	uint16_t height;
    } __attribute__((packed)) data;
    int i;
    uint16_t rlew;
    unsigned char *p;
    FILE *f = fopen("maphead." GAMEEXT, "rb");
    fread(&rlew, 2, 1, f);
    if (!f)
      return 1;
    fread(offsets, 4, 60, f);
    fclose(f);

    printf("#ifndef HOST\n");
    printf("#define RLEWtag 0x%x\n", rlew);
    printf("#else\n");
    printf("#include \"wl_def.h\"\n");
    printf("const maptype mapheaderseg[NUMMAPS] = {\n");
    f = fopen("gamemaps." GAMEEXT, "rb");
    for (i = 0; i < NUMMAPS; i++)
      {
	if (!offsets[i]) {
	    printf ("{{0, 0}, {0,0}},\n");
	    continue;
	}
	fseek(f, offsets[i], SEEK_SET);
	fread(&data, 2, 11, f);
	printf("{{%d, %d}, {%d, %d}},\n",
	       data.offset[0], data.offset[1], data.size[0], data.size[1]);
	if (data.width != 64 || data.height != 64)
	  {
	    fprintf(stderr, "Map %d not 64*64\n", i);
	    return 1;
	  }
      }
    printf("};\n");
    printf("#endif\n");
    return 0;
}

