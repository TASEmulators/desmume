/*
	Copyright (C) 2007 Pascal Giard
	Copyright (C) 2007-2011 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CTRLSSDL_H
#define CTRLSSDL_H

#ifdef HAVE_GL_GL_H
#include <GL/gl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL.h>
#include "MMU.h"

#include "types.h"

#define ADD_KEY(keypad,key) ( (keypad) |= (key) )
#define RM_KEY(keypad,key) ( (keypad) &= ~(key) )
#define KEYMASK_(k)	(1 << (k))

#define JOY_AXIS  0
#define JOY_HAT  1
#define JOY_BUTTON  2

#define JOY_HAT_RIGHT 0
#define JOY_HAT_LEFT 1
#define JOY_HAT_UP 2
#define JOY_HAT_DOWN 3

#define NB_KEYS		14
#define KEY_NONE		0
#define KEY_A			1
#define KEY_B			2
#define KEY_SELECT		3
#define KEY_START		4
#define KEY_RIGHT		5
#define KEY_LEFT		6
#define KEY_UP			7
#define KEY_DOWN		8
#define KEY_R			9
#define KEY_L			10
#define KEY_X			11
#define KEY_Y			12
#define KEY_DEBUG		13
#define KEY_BOOST		14

/* Keypad key names */
extern const char *key_names[NB_KEYS];
/* Current keyboard configuration */
extern u16 keyboard_cfg[NB_KEYS];
/* Current joypad configuration */
extern u16 joypad_cfg[NB_KEYS];
/* Number of detected joypads */
extern u16 nbr_joy;

#ifndef GTK_UI
struct mouse_status
{
  signed long x;
  signed long y;
  BOOL click;
  BOOL down;
};

extern mouse_status mouse;
#endif // !GTK_UI

struct ctrls_event_config {
  unsigned short keypad;
  float nds_screen_size_ratio;
  int auto_pause;
  int focused;
  int sdl_quit;
  int boost;
  int fake_mic;
#ifdef HAVE_GL_GL_H
  GLuint *screen_texture;
  void (*resize_cb)(u16 width, u16 height, GLuint *screen_texture);
#else
  void *screen_texture;
  void (*resize_cb)(u16 width, u16 height, void *screen_texture);
#endif
};

void load_default_config(const u16 kbCfg[]);
BOOL init_joy( void);
void uninit_joy( void);
u16 get_joy_key(int index);
u16 get_set_joy_key(int index);
void update_keypad(u16 keys);
u16 get_keypad( void);
u16 lookup_key (u16 keyval);
u16 lookup_joy_key (u16 keyval);
void
process_ctrls_event( SDL_Event& event,
                      struct ctrls_event_config *cfg);

void
process_joystick_events( u16 *keypad);

#endif /* CTRLSSDL_H */
