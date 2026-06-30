#include "wl_def.h"

byte chunk[4096];
FILE *vswap;
int orgsize;

void recompile_sprite(int picnum)
{
    byte pixels[64 * 64];
    int pixeloffset[64 * 64];
    int posty[32 * 64];
    int poststart[32 * 64];
    int postlen[32 * 64];
    int postpixel[64 * 32];
    int xstart[65];
    const byte *src;
    int np;
    int numpixels;
    int width;
    int cmd;
    int x;
    int tex;
    int y0;
    int y1;
    int i;
    int limit;
    int commonpixels;
    int j;
    int n;
    int pixelpos;

    if (PMPages[picnum].length == 0) {
	printf("#define Sprite%d NULL\n", picnum - PMSpriteStart);
	return;
    }
    np = 0;
    numpixels = 0;
    limit = 0;
    commonpixels = 0;
    fseek(vswap, PMPages[picnum].offset << 8, SEEK_SET);
    fread(chunk, PMPages[picnum].length, 1, vswap);
    orgsize += PMPages[picnum].length;
    src = chunk;
    width = src[2] + 1 - src[0];
    /* left, right, (width+1) offsets.  */
    pixelpos = ((width * 3 + 4) / 2) + 2;
    for (x = 0; x < width; x++) {
	xstart[x] = np;
	cmd = src[x * 2 + 4] | (src[x * 2 + 5] << 8);
	while (src[cmd]) {
	    y1 = src[cmd] / 2;
	    y0 = src[cmd + 4] / 2;
	    posty[np] = y0;
	    postlen[np] = y1 - y0;
	    tex = y0 + (int16_t)(src[cmd + 2] + (src[cmd + 3] << 8));
	    poststart[np] = numpixels;
	    postpixel[np] = pixelpos;
	    pixelpos += 3;
	    for (i = 0; i < postlen[np]; i++) {
		pixeloffset[numpixels] = pixelpos;
		pixels[numpixels++] = src[tex++];
		if ((i & 1) == 0)
		  pixelpos++;
	    }
	    np++;
	    cmd += 6;
	    if (limit < tex)
	      limit = tex;
	}
    }
    postlen[np] = 0;
    postpixel[np] = pixelpos;
    pixelpos++;
    np++;

    /* Share duplicated pixels.  */
    for (i = np - 2; i >= 0; i--) {
	int found;
	int newstart;

	found = 0;
	newstart = 0xffff;
	for (j = i + 1; j < np; j++) {
	    byte *p;
	    byte *q;
	    if (postlen[i] == 0)
	      continue;
	    if (postlen[j] < postlen[i])
	      continue;
	    for (n = 0; n <= postlen[j] - postlen[i]; n+=2) {
		if (pixeloffset[poststart[i]] + 255
		    < pixeloffset[poststart[j] + n])
		  break;
		p = &pixels[poststart[i]];
		q = &pixels[poststart[j] + n];
		if (memcmp(p, q, postlen[i]) == 0) {
		    found = 1;
		    newstart = poststart[j] + n;
		    break;
		}
	    }
	    if (found)
	      break;
	}

	if (found) {
	    n = (postlen[i] + 1) >> 1;
	    for (j = poststart[i]; j < poststart[i] + postlen[i]; j++) {
		pixeloffset[j] = 0xffff;
	    }
	    for (j = poststart[i] + postlen[i]; j < numpixels; j++) {
		pixeloffset[j] -= n;
	    }
	    for (j = i + 1; j < np; j++) {
		postpixel[j] -= n;
	    }
	    poststart[i] = newstart;
	    commonpixels += n;
	}
    }

    printf("static const byte ROMAREA NOPAD Sprite%d[] = {\n",
	   picnum - PMSpriteStart);
    xstart[width] = np - 1;
    printf("%d, %d,\n", src[0], src[2]);
    for (x = 0; x < width; x += 2) {
	int start0;
	int start1;
	int tmp;
	start0 = postpixel[xstart[x]];
	start1 = postpixel[xstart[x + 1]];
	printf ("%d, %d, %d, ", start0 & 0xff,
		(start0 >> 8) | ((start1 & 0xf) << 4), start1 >> 4);
    }
    if (x == width)
      printf("%d, %d,\n", postpixel[np - 1] & 0xff, postpixel[np - 1] >> 8);
    else
      printf("\n");
    x = 1;
    for (i = 0; i < np; i++) {
	if (postlen[i] == 0) {
	    continue;
	}
	if (i == xstart[x]) {
	    x++;
	    printf("\n");
	}
	n = pixeloffset[poststart[i]] - (postpixel[i] + 3);
	printf("%d, %d, %d, ", postlen[i], posty[i], n);
	if (n == 0) {
	    byte *p;
	    n = (postlen[i] + 1) >> 1;
	    p = &pixels[poststart[i]];
	    for (j = 0; j < n ; j++) {
		printf("0x%02x, ", (pal4bit[p[0]] << 4) | pal4bit[p[1]]);
		p += 2;
	    }
	}
    }
    printf ("};\n");
}

int main()
{
  int i;
  vswap = fopen("vswap." GAMEEXT, "rb");
  printf("#include \"wl_def.h\"\n");
  for (i = PMSpriteStart; i < PMSpriteStart + SPR_TOTAL; i++) {
      recompile_sprite(i);
  }
  printf ("const byte *const ROMAREA SpriteChunks[SPR_TOTAL] = {\n");
  for (i = 0; i < SPR_TOTAL; i++) {
      printf("  Sprite%d,\n", i);
  }
  printf("}; /* %d */\n", orgsize);
  fclose(vswap);
  return 0;
}

