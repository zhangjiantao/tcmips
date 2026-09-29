/* SDL compatibility layer: joystick stubs for TCMIPS. */

#include "tcmips_sdl_internal.h"

struct SDL_Joystick {
  int dummy;
};

int SDL_NumJoysticks(void) { return 0; }
const char *SDL_JoystickName(int device_index) {
  (void)device_index;
  return "";
}
SDL_Joystick *SDL_JoystickOpen(int device_index) {
  (void)device_index;
  return NULL;
}
void SDL_JoystickClose(SDL_Joystick *joystick) { (void)joystick; }
int SDL_JoystickOpened(int device_index) {
  (void)device_index;
  return 0;
}
int SDL_JoystickIndex(SDL_Joystick *joystick) {
  (void)joystick;
  return -1;
}
int SDL_JoystickNumAxes(SDL_Joystick *joystick) {
  (void)joystick;
  return 0;
}
int SDL_JoystickNumBalls(SDL_Joystick *joystick) {
  (void)joystick;
  return 0;
}
int SDL_JoystickNumHats(SDL_Joystick *joystick) {
  (void)joystick;
  return 0;
}
int SDL_JoystickNumButtons(SDL_Joystick *joystick) {
  (void)joystick;
  return 0;
}
void SDL_JoystickUpdate(void) {}
int SDL_JoystickEventState(int state) {
  (void)state;
  return 0;
}
Sint16 SDL_JoystickGetAxis(SDL_Joystick *joystick, int axis) {
  (void)joystick;
  (void)axis;
  return 0;
}
Uint8 SDL_JoystickGetHat(SDL_Joystick *joystick, int hat) {
  (void)joystick;
  (void)hat;
  return 0;
}
int SDL_JoystickGetBall(SDL_Joystick *joystick, int ball, int *dx, int *dy) {
  (void)joystick;
  (void)ball;
  (void)dx;
  (void)dy;
  return -1;
}
Uint8 SDL_JoystickGetButton(SDL_Joystick *joystick, int button) {
  (void)joystick;
  (void)button;
  return 0;
}
