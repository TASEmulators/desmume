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
#include "saves.h"
#include "SPU.h"

u16 keyboard_cfg[NB_KEYS];
u16 joypad_cfg[NB_KEYS];
u16 nbr_joy;
mouse_status mouse;

extern volatile BOOL execute;

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

const u16 plain_keyboard_cfg[NB_KEYS] =
{
    'x',         // A
    'z',         // B
    'y',         // select
    'u',         // start
    'l',         // Right
    'j',         // Left
    'i',         // Up
    'k',         // Down
    'w',         // R
    'q',         // L
    's',         // X
    'a',         // Y
    'p',         // DEBUG
    'o'          // BOOST
};

/* Load default joystick and keyboard configurations */
void load_default_config(const u16 kbCfg[])
{
  memcpy(keyboard_cfg, kbCfg, sizeof(keyboard_cfg));
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

  if ( nbr_joy > 0) {
    printf("Found %d joysticks\n", nbr_joy);
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
#ifdef WORDS_BIGENDIAN
  MMU.ARM9_REG[0x130] = ~keys & 0xFF;
  MMU.ARM9_REG[0x131] = (~keys >> 8) & 0x3;
  MMU.ARM7_REG[0x130] = ~keys & 0xFF;
  MMU.ARM7_REG[0x131] = (~keys >> 8) & 0x3;
#else
  ((u16 *)MMU.ARM9_REG)[0x130>>1] = ~keys & 0x3FF;
  ((u16 *)MMU.ARM7_REG)[0x130>>1] = ~keys & 0x3FF;
#endif
  /* Update X and Y buttons */
  MMU.ARM7_REG[0x136] = ( ~( keys >> 10) & 0x3 ) | (MMU.ARM7_REG[0x136] & ~0x3);
}

/* Retrieve current NDS keypad */
u16 get_keypad( void)
{
  u16 keypad;
  keypad = ~MMU.ARM7_REG[0x136];
  keypad = (keypad & 0x3) << 10;
#ifdef WORDS_BIGENDIAN
  keypad |= ~(MMU.ARM9_REG[0x130] | (MMU.ARM9_REG[0x131] << 8)) & 0x3FF;
#else
  keypad |= ~((u16 *)MMU.ARM9_REG)[0x130>>1] & 0x3FF;
#endif
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

u16 shift_pressed;

void
process_ctrls_event( SDL_Event& event, u16 *keypad,
                      float nds_screen_size_ratio)
{
  u16 key;
  if ( !do_process_joystick_events( keypad, &event)) {
    switch (event.type)
    {
      case SDL_KEYDOWN:
        switch(event.key.keysym.sym){
            case SDLK_LSHIFT:
                shift_pressed |= 1;
                break;
            case SDLK_RSHIFT:
                shift_pressed |= 2;
                break;
            default:
                key = lookup_key(event.key.keysym.sym);
                ADD_KEY( *keypad, key );
                break;
        }
        break;

      case SDL_KEYUP:
        switch(event.key.keysym.sym){
            case SDLK_LSHIFT:
                shift_pressed &= ~1;
                break;
            case SDLK_RSHIFT:
                shift_pressed &= ~2;
                break;

            case SDLK_F1:
            case SDLK_F2:
            case SDLK_F3:
            case SDLK_F4:
            case SDLK_F5:
            case SDLK_F6:
            case SDLK_F7:
            case SDLK_F8:
            case SDLK_F9:
            case SDLK_F10:
                int prevexec;
                prevexec = execute;
                execute = FALSE;
                SPU_Pause(1);
                if(!shift_pressed){
                    loadstate_slot(event.key.keysym.sym - SDLK_F1 + 1);
                }else{
                    savestate_slot(event.key.keysym.sym - SDLK_F1 + 1);
                }
                execute = prevexec;
                SPU_Pause(!execute);
                break;
            default:
                key = lookup_key(event.key.keysym.sym);
                RM_KEY( *keypad, key );
                break;
        }
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

      default:
        break;
    }
  }
}

