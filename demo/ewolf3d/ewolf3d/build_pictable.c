#include "wl_def.h"

#ifndef EMBEDDED
#include "huffman.h"

void CAL_HuffExpand(const byte *source, byte *dest, long length, 
	const huffnode *htable)
{
	const huffnode *headptr;          
	const huffnode *nodeon;           
	byte      mask = 0x01;    
	word      path;             
	byte     *endoff = dest + length;    

	nodeon = headptr = htable + 254;

	do {
		if (*source & mask)
		  path = 1;
	        else
		  path = 0;
       		mask <<= 1;
	        if (mask == 0x00) {   
			mask = 0x01;
			source++;
	        } 
		if (path ? nodeon->flag1 : nodeon->flag0) {  
			nodeon = (htable + nodeon->val[path]);
		} else {
			*dest = nodeon->val[path];
			dest++;
			nodeon = headptr;
		}
	} while (dest != endoff);   
} 
#endif

int main()
{
#ifndef EMBEDDED
    byte *source;
    byte *p;
    int start;
    int end;
    int i;
    int size;
    int compressed;
    FILE *f = fopen("vgagraph." GAMEEXT, "rb");
    if (!f)
      return 1;

    start = grstarts[STRUCTPIC];
    compressed = grstarts[STRUCTPIC + 1] - start;
    source = malloc(compressed);
    fread(source, 1, compressed, f);
    fclose(f);

    size = source[0]|(source[1]<<8)|(source[2]<<16)|(source[3]<<24);
    source += 4;

    p = malloc(size);
    CAL_HuffExpand(source, p, size, grhuffman);
#endif

    printf("#include \"wl_def.h\"\n");
#ifndef EMBEDDED
    printf("#ifndef EMBEDDED\n");
    printf("const pictabletype pictable[NUMPICS] = {\n", NUMPICS);
    if (size / 4 < NUMPICS)
      printf("#error NUMPICS > %d\n", size / 4);
    for (i = 0; i < NUMPICS; i++)
      {
	printf("{%d,%d},\n", p[0] | (p[1] << 8), p[2] | (p[3] << 8));
	p += 4;
      }
    printf("};\n");
    printf("#endif\n");
#endif
    return 0;
}
