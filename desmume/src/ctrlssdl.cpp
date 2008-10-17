/* joysdl.c - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Pascal Giard
 *
 * Author: Pascal Giard <evilynux@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "ctrlssdl.h"

u16 keyboard_cfg[NB_KEYS];
u16 joypad_cfg[NB_KEYS];
u16 nbr_joy;
mouse_status mouse;

static SDL_Joystick **open_joysticks = NULL;

/* Keypad key names */
const char *key_names[NB_KEYS] =
{
  "A", "B", "Select", "Start",
  "Right", "Left", "Up", "Down",
  "R", "L", "X", "Y",
  "Debug", "Boost"
};

/* Default joypad configuration */
const u16 default_joypad_cfg[NB_KEYS] =
  { 1,  // A
    0,  // B
    5,  // select
    8,  // start
    256, // Right -- Start cheating abit...
    256, // Left
    512, // Up
    512, // Down  -- End of cheating.
    7,  // R
    6,  // L
    4,  // X
    3,  // Y
    -1, // DEBUG
    -1  // BOOST
  };

const u16 default_keyboard_cfg[NB_KEYS] =
{
  97, // a
  98, // b
  65288, // backspace
  65293, // enter
  65363, // directional arrows
  65361,
  65362,
  65364,
  65454, // numeric .
  65456, // numeric 0
  120, // x
  121, // y
  112,
  113
};

/* Load default joystick and keyboard configurations */
void load_default_config( void)
{
  memcpy(keyboard_cfg, default_keyboard_cfg, sizeof(keyboard_cfg));
  memcpy(joypad_cfg, default_joypad_cfg, sizeof(joypad_cfg));
}

/* Initialize joysticks */
BOOL init_joy( void) {
  int i;
  BOOL joy_init_good = TRUE;

  set_joy_keys(default_joypad_cfg);

  if(SDL_InitSubSystem(SDL_INIT_JOYSTICK) == -1)
    {
      fprintf(stderr, "Error trying to initialize joystick support: %s\n",
              SDL_GetError());
      return FALSE;
    }

  nbr_joy = SDL_NumJoysticks();
  printf("Nbr of joysticks: %d\n\n", nbr_joy);

  if ( nbr_joy > 0) {
    open_joysticks =
      (SDL_Joystick**)calloc( sizeof ( SDL_Joystick *), nbr_joy);

    if ( open_joysticks != NULL) {
      for (i = 0; i < nbr_joy; i++)
        {
          SDL_Joystick * joy = SDL_JoystickOpen(i);
          printf("Joystick %d %s\n", i, SDL_JoystickName(i));
          printf("Axes: %d\n", SDL_JoystickNumAxes(joy));
          printf("Buttons: %d\n", SDL_JoystickNumButtons(joy));
          printf("Trackballs: %d\n", SDL_JoystickNumBalls(joy));
          printf("Hats: %d\n\n", SDL_JoystickNumHats(joy));
        }
    }
    else {
      joy_init_good = FALSE;
    }
  }

  return joy_init_good;
}

/* Set all buttons at once */
void set_joy_keys(const u16 joyCfg[])
{
  memcpy(joypad_cfg, joyCfg, sizeof(joypad_cfg));
}

/* Set all buttons at once */
void set_kb_keys(const u16 kbCfg[])
{
  memcpy(keyboard_cfg, kbCfg, sizeof(keyboard_cfg));
}

/* Unload joysticks */
void uninit_joy( void)
{
  int i;
  printf("Disabling joystick support.\n");

  if ( open_joysticks != NULL) {
    for (i = 0; i < SDL_NumJoysticks(); i++) {
      SDL_JoystickClose( open_joysticks[i]);
    }

    free( open_joysticks);
  }

  open_joysticks = NULL;
  SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

/* Return keypad vector with given key set to 1 */
u16 lookup_joy_key (u16 keyval) {
  int i;
  u16 Key = 0;
  for(i = 0; i < NB_KEYS; i++)
    if(keyval == joypad_cfg[i]) break;
  if(i < NB_KEYS) Key = KEYMASK_(i);
  return Key;
}

/* Return keypad vector with given key set to 1 */
u16 lookup_key (u16 keyval) {
  int i;
  u16 Key = 0;
  for(i = 0; i < NB_KEYS; i++)
    if(keyval == keyboard_cfg[i]) break;
  if(i < NB_KEYS) Key = KEYMASK_(i);
  return Key;
}

/* Empty SDL Events' queue */
static void clear_events( void)
{
  SDL_Event event;
  /* IMPORTANT: Reenable joystick events iif needed. */
  if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
    SDL_JoystickEventState(SDL_ENABLE);

  /* There's an event waiting to be processed? */
  while (SDL_PollEvent(&event))
    {
    }

  return;
}

/* Get and set a new joystick key */
u16 get_set_joy_key(int index) {
  BOOL done = FALSE;
  SDL_Event event;
  u16 key = joypad_cfg[index];

  /* Enable joystick events if needed */
  if( SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE )
    SDL_JoystickEventState(SDL_ENABLE);

  while(SDL_WaitEvent(&event) && !done)
    {
      switch(event.type)
        {
        case SDL_JOYBUTTONDOWN:
          printf( "Got joykey: %d\n", event.jbutton.button );
          key = event.jbutton.button;
          done = TRUE;
          break;
        }
    }

  if( SDL_JoystickEventState(SDL_QUERY) == SDL_ENABLE )
    SDL_JoystickEventState(SDL_IGNORE);
  joypad_cfg[index] = key;

  return key;
}

/* Reset corresponding key and its twin axis key */
static u16 get_joy_axis_twin(u16 key)
{
  switch(key)
    {
    case KEYMASK_( KEY_RIGHT-1 ):
      return KEYMASK_( KEY_LEFT-1 );
    case KEYMASK_( KEY_UP-1 ):
      return KEYMASK_( KEY_DOWN-1 );
    default:
      return 0;
    }
}

/* Get and set a new joystick axis */
void get_set_joy_axis(int index, int index_o) {
  BOOL done = FALSE;
  SDL_Event event;
  u16 key = joypad_cfg[index];

  /* Clear events */
  clear_events();
  /* Enable joystick events if needed */
  if( SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE )
    SDL_JoystickEventState(SDL_ENABLE);

  while(SDL_WaitEvent(&event) && !done)
    {
      switch(event.type)
        {
        case SDL_JOYAXISMOTION:
          /* Discriminate small movements */
          if( (event.jaxis.value >> 5) != 0 )
            {
              key = JOY_AXIS_(event.jaxis.axis);
              done = TRUE;
            }
          break;
        }
    }
  if( SDL_JoystickEventState(SDL_QUERY) == SDL_ENABLE )
    SDL_JoystickEventState(SDL_IGNORE);
  /* Update configuration */
  joypad_cfg[index]   = key;
  joypad_cfg[index_o] = joypad_cfg[index];
}

static signed long
screen_to_touch_range_x( signed long scr_x, float size_ratio) {
  signed long touch_x = (signed long)((float)scr_x * size_ratio);

  return touch_x;
}

static signed long
screen_to_touch_range_y( signed long scr_y, float size_ratio) {
  signed long touch_y = (signed long)((float)scr_y * size_ratio);

  return touch_y;
}

/* Set mouse coordinates */
void set_mouse_coord(signed long x,signed long y)
{
  if(x<0) x = 0; else if(x>255) x = 255;
  if(y<0) y = 0; else if(y>192) y = 192;
  mouse.x = x;
  mouse.y = y;
}

/* Update NDS keypad */
void update_keypad(u16 keys)
{
  ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] = ~keys & 0x3FF;
  ((u16 *)MMU.ARM7_REG)[0x130>>1] = ~keys & 0x3FF;
  /* Update X and Y buttons */
  MMU.ARM7_REG[0x136] = ( ~( keys >> 10) & 0x3 ) | (MMU.ARM7_REG[0x136] & ~0x3);
}

/* Retrieve current NDS keypad */
u16 get_keypad( void)
{
  u16 keypad;
  keypad = ~MMU.ARM7_REG[0x136];
  keypad = (keypad & 0x3) << 10;
  keypad |= ~((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x3FF;
  return keypad;
}

/*
 * The internal joystick events processing function
 */
static int
do_process_joystick_events( u16 *keypad, SDL_Event *event) {
  int processed = 1;
  u16 key;

  switch ( event->type)
    {
      /* Joystick axis motion 
         Note: button constants have a 1bit offset. */
    case SDL_JOYAXISMOTION:
      key = lookup_joy_key( JOY_AXIS_(event->jaxis.axis) );
      if( key == 0 ) break;           /* Not an axis of interest? */

      /* Axis is back to initial position */
      if( event->jaxis.value == 0 )
        RM_KEY( *keypad, key | get_joy_axis_twin(key) );
      /* Key should have been down but its currently set to up? */
      else if( (event->jaxis.value > 0) && 
               (key == KEYMASK_( KEY_UP-1 )) )
        key = KEYMASK_( KEY_DOWN-1 );
      /* Key should have been left but its currently set to right? */
      else if( (event->jaxis.value < 0) && 
               (key == KEYMASK_( KEY_RIGHT-1 )) )
        key = KEYMASK_( KEY_LEFT-1 );
              
      /* Remove some sensitivity before checking if different than zero... 
         Fixes some badly behaving joypads [like one of mine]. */
      if( (event->jaxis.value >> 5) != 0 )
        ADD_KEY( *keypad, key );
      break;

      /* Joystick button pressed */
      /* FIXME: Add support for BOOST */
    case SDL_JOYBUTTONDOWN:
      key = lookup_joy_key( event->jbutton.button );
      ADD_KEY( *keypad, key );
      break;

      /* Joystick button released */
    case SDL_JOYBUTTONUP:
      key = lookup_joy_key(event->jbutton.button);
      RM_KEY( *keypad, key );
      break;

    default:
      processed = 0;
      break;
    }

  return processed;
}

/*
 * Process only the joystick events
 */
void
process_joystick_events( u16 *keypad) {
  SDL_Event event;

  /* IMPORTANT: Reenable joystick events iif needed. */
  if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
    SDL_JoystickEventState(SDL_ENABLE);

  /* There's an event waiting to be processed? */
  while (SDL_PollEvent(&event))
    {
      do_process_joystick_events( keypad, &event);
    }
}

/* Manage input events */
int
process_ctrls_events( u16 *keypad,
                      void (*external_videoResizeFn)( u16 width, u16 height),
                      float nds_screen_size_ratio)
{
  u16 key;
  int cause_quit = 0;
  SDL_Event event;

  /* IMPORTANT: Reenable joystick events iif needed. */
  if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
    SDL_JoystickEventState(SDL_ENABLE);

  /* There's an event waiting to be processed? */
  while (SDL_PollEvent(&event))
    {
      if ( !do_process_joystick_events( keypad, &event)) {
        switch (event.type)
          {
          case SDL_VIDEORESIZE:
            if ( external_videoResizeFn != NULL) {
              external_videoResizeFn( event.resize.w, event.resize.h);
            }
            break;

          case SDL_KEYDOWN:
            key = lookup_key(event.key.keysym.sym);
            ADD_KEY( *keypad, key );
            break;

          case SDL_KEYUP:
            key = lookup_key(event.key.keysym.sym);
            RM_KEY( *keypad, key );
            break;

          case SDL_MOUSEBUTTONDOWN:
            if(event.button.button==1)
              mouse.down = TRUE;
						
          case SDL_MOUSEMOTION:
            if(!mouse.down)
              break;
            else {
              signed long scaled_x =
                screen_to_touch_range_x( event.button.x,
                                         nds_screen_size_ratio);
              signed long scaled_y =
                screen_to_touch_range_y( event.button.y,
                                         nds_screen_size_ratio);

              if( scaled_y >= 192)
                set_mouse_coord( scaled_x, scaled_y - 192);
            }
            break;

          case SDL_MOUSEBUTTONUP:
            if(mouse.down) mouse.click = TRUE;
            mouse.down = FALSE;
            break;

          case SDL_QUIT:
            cause_quit = 1;
            break;

          default:
            break;
          }
        }
    }

  return cause_quit;
}

