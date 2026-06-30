#include "wl_def.h"

// FIXME: don't hardcode first sprite offset.
#define S 106
int cc[] = 
{
  /* Doors.  */
#if 0
  S-8, S-7, S-6, S-5, S-4, S-3, S-2, S-1,
  /* Static sprites.  */
  S+SPR_STAT_0,
  S+SPR_STAT_1,
  S+SPR_STAT_2,
  S+SPR_STAT_3,
  S+SPR_STAT_4,
  S+SPR_STAT_5,
  S+SPR_STAT_6,
  S+SPR_STAT_7,
  S+SPR_STAT_8,
  S+SPR_STAT_9,
  S+SPR_STAT_10,
  S+SPR_STAT_11,
  S+SPR_STAT_12,
  S+SPR_STAT_13,
  S+SPR_STAT_14,
  S+SPR_STAT_15,
  S+SPR_STAT_16,
  S+SPR_STAT_17,
  S+SPR_STAT_18,
  S+SPR_STAT_19,
  S+SPR_STAT_20,
  S+SPR_STAT_21,
  S+SPR_STAT_22,
  S+SPR_STAT_23,
  S+SPR_STAT_24,
  S+SPR_STAT_25,
  S+SPR_STAT_26,
  S+SPR_STAT_27,
  S+SPR_STAT_28,
  S+SPR_STAT_29,
  S+SPR_STAT_30,
  S+SPR_STAT_31,
  S+SPR_STAT_32,
  S+SPR_STAT_33,
  S+SPR_STAT_34,
  S+SPR_STAT_35,
  S+SPR_STAT_36,
  S+SPR_STAT_37,
  S+SPR_STAT_38,
  S+SPR_STAT_39,
  S+SPR_STAT_40,
  S+SPR_STAT_41,
  /* Weapon sprites.  */
  S+SPR_PISTOLREADY,
  S+SPR_PISTOLATK1,
  S+SPR_PISTOLATK2,
  S+SPR_PISTOLATK3,
  S+SPR_PISTOLATK4,
  S+SPR_MACHINEGUNREADY,
  S+SPR_MACHINEGUNATK1,
  S+SPR_MACHINEGUNATK2,
  S+SPR_MACHINEGUNATK3,
  S+SPR_MACHINEGUNATK4,
  S+SPR_CHAINREADY,
  S+SPR_CHAINATK1,
  S+SPR_CHAINATK2,
  S+SPR_CHAINATK3,
  S+SPR_CHAINATK4,
#endif
};

#define cc_num (sizeof(cc) / 4)

int main()
{
    int start;
    int size;
    int i;
    int j;
    byte buf[4096];
    FILE *f = fopen ("vswap." GAMEEXT, "rb");
    if (!f)
      return 1;

    printf ("#include \"wl_def.h\"\n");
    for (i = 0; i < cc_num; i++)
      {
	start = (int)PMPages[cc[i]].offset << 8;
	size = PMPages[cc[i]].length;
	fseek (f, start, SEEK_SET);
	fread (buf, size, 1, f);
	printf ("static const byte ROMAREA RomChunk%d[]= {\n", cc[i]);
	for (j = 0; j < size; j++)
	  {
	    printf (" 0x%x,", buf[j]);
	    if ((j & 7) == 7)
	      printf ("\n");
	  }
	printf("};\n");
      }
    fclose(f);
    printf("const byte * const ROMAREA RomChunks[%d] = {\n", ChunksInFile);
    for (i = 0; i < cc_num; i++)
      {
	printf("[%d] = RomChunk%d,\n", cc[i], cc[i]);
      }
    printf ("};\n");
    return 0;
}
