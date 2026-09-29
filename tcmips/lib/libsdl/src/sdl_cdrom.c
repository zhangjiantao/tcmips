/* SDL compatibility layer: CD-ROM stubs for TCMIPS. */

#include "tcmips_sdl_internal.h"

int SDL_CDNumDrives(void) { return 0; }
const char *SDL_CDName(int drive) {
  (void)drive;
  return "";
}
void *SDL_CDOpen(int drive) {
  (void)drive;
  return NULL;
}
void SDL_CDClose(void *cdrom) { (void)cdrom; }
int SDL_CDStatus(void *cdrom) {
  (void)cdrom;
  return -1;
}
int SDL_CDPlay(void *cdrom, int start, int length) {
  (void)cdrom;
  (void)start;
  (void)length;
  return -1;
}
int SDL_CDPlayTracks(void *cdrom, int strack, int sframe, int ntracks,
                     int nframes) {
  (void)cdrom;
  (void)strack;
  (void)sframe;
  (void)ntracks;
  (void)nframes;
  return -1;
}
int SDL_CDPause(void *cdrom) {
  (void)cdrom;
  return -1;
}
int SDL_CDResume(void *cdrom) {
  (void)cdrom;
  return -1;
}
int SDL_CDStop(void *cdrom) {
  (void)cdrom;
  return -1;
}
int SDL_CDEject(void *cdrom) {
  (void)cdrom;
  return -1;
}
