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

#include "ctrlssdl.h"
#include "saves.h"
#include "SPU.h"
#include "commandline.h"
#include "NDSSystem.h"
#include "GPU_osd.h"
#ifdef FAKE_MIC
#include "mic.h"
#endif

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

/* Joypad Key Codes -- 4-digit Hexadecimal number
 * 1st digit: device ID (0 is first joypad, 1 is second, etc.)
 * 2nd digit: 0 - Axis, 1 - Hat/POV/D-Pad, 2 - Button
 * 3rd & 4th digit: (depends on input type)
 *  Negative Axis - 2 * axis index
 *  Positive Axis - 2 * axis index + 1
 *  Hat Right - 4 * hat index
 *  Hat Left - 4 * hat index + 1
 *  Hat Up - 4 * hat index + 2
 *  Hat Down - 4 * hat index + 3
 *  Button - button index
 */
 
/* Default joypad configuration */
const u16 default_joypad_cfg[NB_KEYS] =
  { 0x0201,  // A
    0x0200,  // B
    0x0205,  // select
    0x0208,  // start
    0x0001, // Right
    0x0000, // Left
    0x0002, // Up
    0x0003, // Down
    0x0207,  // R
    0x0206,  // L
    0x0204,  // X
    0x0203,  // Y
    0xFFFF, // DEBUG
    0xFFFF  // BOOST
  };

/* Load default joystick and keyboard configurations */
void load_default_config(const u16 kbCfg[])
{
  memcpy(keyboard_cfg, kbCfg, sizeof(keyboard_cfg));
  memcpy(joypad_cfg, default_joypad_cfg, sizeof(joypad_cfg));
}

/* Set all buttons at once */
static void set_joy_keys(const u16 joyCfg[])
{
  memcpy(joypad_cfg, joyCfg, sizeof(joypad_cfg));
}

/* Initialize joysticks */
BOOL init_joy( void) {
  int i;
  BOOL joy_init_good = TRUE;

  //user asked for no joystick
  if(_commandline_linux_nojoy) {
	  printf("skipping joystick init\n");
	  return TRUE;
  }

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
    if(keyval == joypad_cfg[i]) {
      Key = KEYMASK_(i);
      break;
    }

  return Key;
}

/* Return keypad vector with given key set to 1 */
u16 lookup_key (u16 keyval) {
  int i;
  u16 Key = 0;

  for(i = 0; i < NB_KEYS; i++)
    if(keyval == keyboard_cfg[i]) {
      Key = KEYMASK_(i);
      break;
    }

  return Key;
}

/* Get pressed joystick key */
u16 get_joy_key(int index) {
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
          printf( "Device: %d; Button: %d\n", event.jbutton.which, event.jbutton.button );
          key = ((event.jbutton.which & 15) << 12) | JOY_BUTTON << 8 | (event.jbutton.button & 255);
          done = TRUE;
          break;
        case SDL_JOYAXISMOTION:
          /* Dead zone of 50% */
          if( (abs(event.jaxis.value) >> 14) != 0 )
            {
              key = ((event.jaxis.which & 15) << 12) | JOY_AXIS << 8 | ((event.jaxis.axis & 127) << 1);
              if (event.jaxis.value > 0) {
                printf( "Device: %d; Axis: %d (+)\n", event.jaxis.which, event.jaxis.axis );
                key |= 1;
              }
              else
                printf( "Device: %d; Axis: %d (-)\n", event.jaxis.which, event.jaxis.axis );
              done = TRUE;
            }
          break;
        case SDL_JOYHATMOTION:
          /* Diagonal positions will be treated as two separate keys being activated, rather than a single diagonal key. */
          /* JOY_HAT_* are sequential integers, rather than a bitmask */
          if (event.jhat.value != SDL_HAT_CENTERED) {
            key = ((event.jhat.which & 15) << 12) | JOY_HAT << 8 | ((event.jhat.hat & 63) << 2);
            /* Can't just use a switch here because SDL_HAT_* make up a bitmask. We only want one of these when assigning keys. */
            if ((event.jhat.value & SDL_HAT_UP) != 0) {
              key |= JOY_HAT_UP;
              printf( "Device: %d; Hat: %d (Up)\n", event.jhat.which, event.jhat.hat );
            }
            else if ((event.jhat.value & SDL_HAT_RIGHT) != 0) {
              key |= JOY_HAT_RIGHT;
              printf( "Device: %d; Hat: %d (Right)\n", event.jhat.which, event.jhat.hat );
            }
            else if ((event.jhat.value & SDL_HAT_DOWN) != 0) {
              key |= JOY_HAT_DOWN;
              printf( "Device: %d; Hat: %d (Down)\n", event.jhat.which, event.jhat.hat );
            }
            else if ((event.jhat.value & SDL_HAT_LEFT) != 0) {
              key |= JOY_HAT_LEFT;
              printf( "Device: %d; Hat: %d (Left)\n", event.jhat.which, event.jhat.hat );
            }
            done = TRUE;
          }
          break;
        }
    }

  if( SDL_JoystickEventState(SDL_QUERY) == SDL_ENABLE )
    SDL_JoystickEventState(SDL_IGNORE);

  return key;
}

/* Get and set a new joystick key */
u16 get_set_joy_key(int index) {
  joypad_cfg[index] = get_joy_key(index);

  return joypad_cfg[index];
}

static signed long
screen_to_touch_range( signed long scr, float size_ratio) {
  return (signed long)((float)scr * size_ratio);
}

/* Set mouse coordinates */
static void set_mouse_coord(signed long x,signed long y)
{
  if(x<0) x = 0; else if(x>255) x = 255;
  if(y<0) y = 0; else if(y>192) y = 192;
  mouse.x = x;
  mouse.y = y;
}

/* Update NDS keypad */
void update_keypad(u16 keys)
{
	NDS_beginProcessingInput();
	UserButtons& input = NDS_getProcessingUserInput().buttons;
	input.G = (keys>>12)&1;
	input.E = (keys>>8)&1;
	input.W = (keys>>9)&1;
	input.X = (keys>>10)&1;
	input.Y = (keys>>11)&1;
	input.A = (keys>>0)&1;
	input.B = (keys>>1)&1;
	input.S = (keys>>3)&1;
	input.T = (keys>>2)&1;
	input.U = (keys>>6)&1;
	input.D = (keys>>7)&1;
	input.L = (keys>>5)&1;
	input.R = (keys>>4)&1;
	input.F = 0;
	NDS_endProcessingInput();
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
  u16 key_code;
  u16 key;
  u16 key_o;
  u16 key_u;
  u16 key_r;
  u16 key_d;
  u16 key_l;

  switch ( event->type)
    {
      /* Joystick axis motion 
         Note: button constants have a 1bit offset. */
    case SDL_JOYAXISMOTION:
      key_code = ((event->jaxis.which & 15) << 12) | JOY_AXIS << 8 | ((event->jaxis.axis & 127) << 1);
      if( (abs(event->jaxis.value) >> 14) != 0 )
        {
          if (event->jaxis.value > 0)
            key_code |= 1;
          key = lookup_joy_key( key_code );
          key_o = lookup_joy_key( key_code ^ 1 );
          if (key != 0)
            ADD_KEY( *keypad, key );
          if (key_o != 0)
            RM_KEY( *keypad, key_o );
        }
      else
        {
          // Axis is zeroed
          key = lookup_joy_key( key_code );
          key_o = lookup_joy_key( key_code ^ 1 );
          if (key != 0)
            RM_KEY( *keypad, key );
          if (key_o != 0)
            RM_KEY( *keypad, key_o );
        }
      break;

    case SDL_JOYHATMOTION:
      /* Diagonal positions will be treated as two separate keys being activated, rather than a single diagonal key. */
      /* JOY_HAT_* are sequential integers, rather than a bitmask */
      key_code = ((event->jhat.which & 15) << 12) | JOY_HAT << 8 | ((event->jhat.hat & 63) << 2);
      key_u = lookup_joy_key( key_code | JOY_HAT_UP );
      key_r = lookup_joy_key( key_code | JOY_HAT_RIGHT );
      key_d = lookup_joy_key( key_code | JOY_HAT_DOWN );
      key_l = lookup_joy_key( key_code | JOY_HAT_LEFT );
      if ((key_u != 0) && ((event->jhat.value & SDL_HAT_UP) != 0))
        ADD_KEY( *keypad, key_u );
      else if (key_u != 0)
        RM_KEY( *keypad, key_u );
      if ((key_r != 0) && ((event->jhat.value & SDL_HAT_RIGHT) != 0))
        ADD_KEY( *keypad, key_r );
      else if (key_r != 0)
        RM_KEY( *keypad, key_r );
      if ((key_d != 0) && ((event->jhat.value & SDL_HAT_DOWN) != 0))
        ADD_KEY( *keypad, key_d );
      else if (key_d != 0)
        RM_KEY( *keypad, key_d );
      if ((key_l != 0) && ((event->jhat.value & SDL_HAT_LEFT) != 0))
        ADD_KEY( *keypad, key_l );
      else if (key_l != 0)
        RM_KEY( *keypad, key_l );
      break;

      /* Joystick button pressed */
      /* FIXME: Add support for BOOST */
    case SDL_JOYBUTTONDOWN:
      key_code = ((event->jbutton.which & 15) << 12) | JOY_BUTTON << 8 | (event->jbutton.button & 255);
      key = lookup_joy_key( key_code );
      if (key != 0)
        ADD_KEY( *keypad, key );
      break;

      /* Joystick button released */
    case SDL_JOYBUTTONUP:
      key_code = ((event->jbutton.which & 15) << 12) | JOY_BUTTON << 8 | (event->jbutton.button & 255);
      key = lookup_joy_key( key_code );
      if (key != 0)
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

  /* IMPORTANT: Reenable joystick events if needed. */
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
process_ctrls_event( SDL_Event& event,
                      struct ctrls_event_config *cfg)
{
  u16 key;
  if ( !do_process_joystick_events( &cfg->keypad, &event)) {
    switch (event.type)
    {
      case SDL_VIDEORESIZE:
        cfg->resize_cb( event.resize.w, event.resize.h, cfg->screen_texture);
        break;

      case SDL_ACTIVEEVENT:
        if (cfg->auto_pause && (event.active.state & SDL_APPINPUTFOCUS )) {
          if (event.active.gain) {
            cfg->focused = 1;
            SPU_Pause(0);
            osd->addLine("Auto pause disabled");
          } else {
            cfg->focused = 0;
            SPU_Pause(1);
          }
        }
        break;

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
                ADD_KEY( cfg->keypad, key );
                break;
        }
        break;

      case SDL_KEYUP:
        switch(event.key.keysym.sym){
            case SDLK_ESCAPE:
                cfg->sdl_quit = 1;
                break;

#ifdef FAKE_MIC
            case SDLK_m:
                cfg->fake_mic = !cfg->fake_mic;
                Mic_DoNoise(cfg->fake_mic);
                if (cfg->fake_mic)
                  osd->addLine("Fake mic enabled");
                else
                  osd->addLine("Fake mic disabled");
                break;
#endif

            case SDLK_o:
                cfg->boost = !cfg->boost;
                if (cfg->boost)
                  osd->addLine("Boost mode enabled");
                else
                  osd->addLine("Boost mode disabled");
                break;

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
                RM_KEY( cfg->keypad, key );
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
            screen_to_touch_range( event.button.x,
                                     cfg->nds_screen_size_ratio);
          signed long scaled_y =
            screen_to_touch_range( event.button.y,
                                     cfg->nds_screen_size_ratio);

          if( scaled_y >= 192)
            set_mouse_coord( scaled_x, scaled_y - 192);
        }
        break;

      case SDL_MOUSEBUTTONUP:
        if(mouse.down) mouse.click = TRUE;
        mouse.down = FALSE;
        break;

      case SDL_QUIT:
        cfg->sdl_quit = 1;
        break;

      default:
        break;
    }
  }
}

