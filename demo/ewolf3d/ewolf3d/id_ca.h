#ifndef __ID_CA_H__
#define __ID_CA_H__

/* ======================================================================== */

#ifdef UPLOAD
#define NUMMAPS		10
#else
#define NUMMAPS		60
#endif
#define MAPPLANES	2

typedef	struct
{
#ifdef ENABLE_PRECOMPILE
	myint planestart[2];
	umyshort planelength[2];
#else
	myint planestart[3];
	myint planelength[3];
	myint width, height;
#endif
} maptype;

/* ======================================================================== */

extern	myint	mapon;

typedef byte ms0;
extern	ms0	mapseg0[64 * 64];
#ifdef ENABLE_PRECOMPILE
extern const byte *const MapPlane[NUMMAPS * 2];
extern	const maptype mapheaderseg[NUMMAPS];
#else
extern	maptype	*mapheaderseg[NUMMAPS];
#endif
extern	byte	*audiosegs[NUMSNDCHUNKS];

/* ======================================================================== */

boolean CA_LoadFile(const char *filename, memptr *ptr);
boolean CA_WriteFile(const char *filename, const void *ptr, long length);

void CA_Startup();
void CA_Shutdown();

#ifdef ENABLE_AUDIO
void CA_CacheAudioChunk(myint chunk);
void CA_UnCacheAudioChunk(myint chunk);
#endif
void CA_LoadAllSounds();

void CA_CacheMap(myint mapnum, myint plane);
void CA_CacheGrChunk(myint chunk);
void CA_UnCacheGrChunk(myint chunk);

memptr CA_GetChunk(myint chunk);

/* ======================================================================= */

typedef word pool_id;

void MM_Startup();
void MM_Shutdown();

void MM_GetPtr(memptr *baseptr, unsigned long size);
memptr MM_AllocPool(pool_id *id, unsigned long size);
void MM_FreePtr(memptr *baseptr);

void MM_SetPurge(memptr *baseptr, myint purge);
void MM_SetLock(memptr *baseptr, boolean locked);

#define PMPageSize	4096

typedef	struct {
	umyshort offset;	/* Offset of chunk into file, >> 8 */
	myshort length;	/* Length of the chunk */
} PageListStruct;

#ifdef ENABLE_PRECOMPILE
extern const myint PMSpriteStart, PMSoundStart;
extern const PageListStruct PMPages[];
extern pool_id PageAddr[];
extern const uint32_t grstarts[NUMCHUNKS + 1];
extern const byte *const RomChunks[];
extern const byte WallData[];
extern const byte WallChunks[];
#else
extern myint ChunksInFile, PMSpriteStart, PMSoundStart;
extern PageListStruct *PMPages;
extern pool_id *PageAddr;
#endif

#define	PM_GetSoundPage(v)	PM_GetPage(PMSoundStart + (v))
#ifdef ENABLE_COLOR
#define	PM_GetSpritePage(v)	PM_GetPage(PMSpriteStart + (v))
#else
extern const byte *const SpriteChunks[];
#define PM_GetSpritePage(v) SpriteChunks[v]
#endif
memptr PM_GetPage(myint pagenum);

void PM_Startup();
void PM_Shutdown();

#endif
