#include "wl_def.h"

FILE *f;

static void writeplane(int map, int plane)
{
    int n;
    int len;
    char buf[16384];
    len = mapheaderseg[map].planelength[plane];
    fseek(f, mapheaderseg[map].planestart[plane], SEEK_SET);
    fread(buf, mapheaderseg[map].planelength[plane], 1, f);
    printf("static const byte MapData%02d_%d[%d] = {", map, plane, len);
    for (n = 0; n < len; n++) {
	if ((n & 15) == 0)
	    printf("\n");
	printf (" %d,", buf[n]);
    }
    printf("\n};\n");

}

int main()
{
    int i;

    printf("#include \"wl_def.h\"\n");
    f = fopen("gamemaps." GAMEEXT, "rb");
    if (!f)
      return 1;
    for (i = 0; i < NUMMAPS; i++) {
	writeplane(i, 0);
	writeplane(i, 1);
    }
    fclose(f);

    printf("const byte *const ROMAREA MapPlane[NUMMAPS * 2] = {\n");
    for (i = 0; i < NUMMAPS; i++) {
	printf("  MapData%02d_%d,\n", i, 0);
	printf("  MapData%02d_%d,\n", i, 1);
    }
    printf("};\n");
    return 0;
}

