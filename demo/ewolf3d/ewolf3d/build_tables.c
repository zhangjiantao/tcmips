#define finetangent const_finetangent
#define sintable const_sintable
#include "wl_def.h"
#undef finetangent
#undef sintable

fixed finetangent[FINEANGLES/4];
fixed sintable[ANGLES+ANGLES/4+1];

static const double radtoint = (double)FINEANGLES/2.0/PI;
#ifdef EMBEDDED
static myshort pa[128];
#endif

int main()
{
	myint i;
	long    intang;
	myint     halfview;
	double tang, angle, anglestep, facedist;
	fixed value;
	fixed scale;

	/* FIXME: Use linear interpolation to reduce size of trig tables. */
/* calculate fine tangents */

	finetangent[0] = 0;
	for (i = 1; i < FINEANGLES/8; i++) {
		tang = tan((double)i/radtoint);
		finetangent[i] = tang*TILEGLOBAL;
		finetangent[FINEANGLES/4-1-i] = TILEGLOBAL/tang;
	}
	
	/* fight off asymptotic behaviour at 90 degrees */
	finetangent[FINEANGLES/4-1] = finetangent[FINEANGLES/4-2]+1;
	
	printf ("#include \"wl_def.h\"\n");
	printf ("const fixed finetangent[] = {\n");
	printf ("  0");
	for (i = 1; i < FINEANGLES/4; i++) {
	    printf (",\n  0x%x", finetangent[i]);
	}
	printf ("\n};\n");
//
// costable overlays sintable with a quarter phase shift
// ANGLES is assumed to be divisable by four
//

	angle = 0.0;
	anglestep = PI/2.0/ANGLEQUAD;
	printf ("const uint16_t sintable[] = {\n");
	printf ("  0");
	for (i = 0; i < ANGLEQUAD; i++) {
	    value = GLOBAL1*sin(angle);
	    printf (",\n  0x%x", value);
	    angle += anglestep;
	}
	printf ("\n};\n");

#ifdef EMBEDDED
	facedist = 0x5800+MINDIST;
	halfview = 64;               /* half view in pixels */

	if (MAXVIEWWIDTH != 128)
	  printf("#error MAXVIEWWIDTH %d, expected 128\n", MAXVIEWWIDTH);
/*
 calculate scale value for vertical height calculations
 and sprite x calculations
*/
	scale = halfview*facedist/(VIEWGLOBAL/2);

/* calculate the angle offset from view angle of each pixel's ray */
	for (i = 0; i < halfview; i++) {
		tang = ((double)i)*VIEWGLOBAL/128/facedist;
		angle = atan(tang);
		intang = angle*radtoint;
		pa[halfview-1-i] = intang;
		pa[halfview+i] = -intang;
	}
	printf ("const myshort pixelangle[128] = {\n");
	for (i = 0; i < 128; i++) {
	    printf ("  %d,\n", pa[i]);
	}
	printf ("};\n");
#endif
	return 0;
}

