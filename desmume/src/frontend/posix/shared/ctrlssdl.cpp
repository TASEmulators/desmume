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
#include "frontend/modules/osd/agg/agg_osd.h"
#include "driver.h"

#ifdef FAKE_MIC
#include "mic.h"
#endif



u32 keyboard_cfg[NB_KEYS];
u32 joypad_cfg[NB_KEYS];

static_assert(sizeof(keyboard_cfg) == sizeof(joypad_cfg), "");

mouse_status mouse;
touchpad_status touchpad = {0.0, 0.0, 0, 0};
static int fullscreen;

struct JoystickInfo
{
  bool             isController;
  union {
    SDL_Joystick*            joy;
    SDL_GameController*      gcont;
  }                handle;
  SDL_JoystickID   id;
};

static JoystickInfo open_joysticks[MAX_JOYSTICKS];

/* Keypad key names */
const char *key_names[NB_KEYS] =
{
  "A", "B", "Select", "Start",
  "Right", "Left", "Up", "Down",
  "R", "L", "X", "Y",
  "Debug", "Boost",
  "Lid",
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
const u32 default_joypad_cfg[NB_KEYS] =
  { 0xFFFF,  // A
    0xFFFF,  // B
    0xFFFF,  // select
    0xFFFF,  // start
    0xFFFF, // Right
    0xFFFF, // Left
    0xFFFF, // Up
    0xFFFF, // Down
    0xFFFF,  // R
    0xFFFF,  // L
    0xFFFF,  // X
    0xFFFF,  // Y
    0xFFFF, // DEBUG
    0xFFFF, // BOOST
    0xFFFF, // Lid
  };

/* Load default joystick and keyboard configurations */
void load_default_config(const u32 kbCfg[])
{
  memcpy(keyboard_cfg, kbCfg, sizeof(keyboard_cfg));
  memcpy(joypad_cfg, default_joypad_cfg, sizeof(joypad_cfg));
}

/* Set all buttons at once */
static void set_joy_keys(const u32 joyCfg[])
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
  
  for(i=0; i<MAX_JOYSTICKS; ++i)
    open_joysticks[i]={FALSE, {NULL}, -1};

  int nbr_joy = std::min(SDL_NumJoysticks(), MAX_JOYSTICKS);
  
  if ( nbr_joy > 0) {
    printf("Found %d joysticks\n", nbr_joy);
    
    for (i = 0; i < nbr_joy; i++) {
       if(SDL_IsGameController(i)) {
         SDL_GameController * gcont = SDL_GameControllerOpen(i);
         if(gcont) {
           printf("Game controller %d %s\n", i, SDL_GameControllerNameForIndex(i));
           printf("Axes: %d\n", SDL_JoystickNumAxes(SDL_GameControllerGetJoystick(gcont)));
           printf("Buttons: %d\n", SDL_JoystickNumButtons(SDL_GameControllerGetJoystick(gcont)));
           printf("Touchpads: %d\n", SDL_GameControllerGetNumTouchpads(gcont));
           printf("%s\n", SDL_GameControllerHasSensor(gcont, SDL_SENSOR_ACCEL) ? "Accelerometer present" : "Accelerometer not present");
           printf("%s\n", SDL_GameControllerHasSensor(gcont, SDL_SENSOR_GYRO) ? "Gyroscope present" : "Gyroscope not present");
           open_joysticks[i].isController=TRUE;
           open_joysticks[i].handle.gcont=gcont;
           open_joysticks[i].id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gcont));
         }
         else {
           fprintf(stderr, "Failed to open game controller %d: %s\n", i, SDL_GetError());
           joy_init_good = FALSE;
         }
       }
       else {
         SDL_Joystick * joy = SDL_JoystickOpen(i);
         if(joy) {
           printf("Joystick %d %s\n", i, SDL_JoystickNameForIndex(i));
           printf("Axes: %d\n", SDL_JoystickNumAxes(joy));
           printf("Buttons: %d\n", SDL_JoystickNumButtons(joy));
           printf("Trackballs: %d\n", SDL_JoystickNumBalls(joy));
           printf("Hats: %d\n\n", SDL_JoystickNumHats(joy));
           open_joysticks[i].isController=FALSE;
           open_joysticks[i].handle.joy=joy;
           open_joysticks[i].id=SDL_JoystickInstanceID(joy);
         }
         else {
           fprintf(stderr, "Failed to open joystick %d: %s\n", i, SDL_GetError());
           joy_init_good = FALSE;
         }
       }
    }
  }

  return joy_init_good;
}

/* Unload joysticks */
void uninit_joy( void)
{
  int i;
  for (i=0; i<MAX_JOYSTICKS; ++i) {
    if(open_joysticks[i].isController) {
      if(open_joysticks[i].handle.gcont)
        SDL_GameControllerClose(open_joysticks[i].handle.gcont);
    }
    else {
      if(open_joysticks[i].handle.joy)
        SDL_JoystickClose(open_joysticks[i].handle.joy);
    }
    open_joysticks[i]={FALSE, {NULL}, -1};
  }
  
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
u32 lookup_key (u32 keyval) {
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
u16 get_joy_key(int index, bool *modified) {
    SDL_Event event;
    u16 key=joypad_cfg[index];
    bool done = FALSE;
    int jn;
    joystick_input_type etype=Joy_InvalidInput;
    Uint8 bnum, bval;
    Sint16 aval;
    
    if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
        SDL_JoystickEventState(SDL_ENABLE);
    if(SDL_GameControllerEventState(SDL_QUERY) == SDL_IGNORE)
        SDL_GameControllerEventState(SDL_ENABLE);
    
    while(!done && SDL_PollEvent(&event))
    {
        etype=Joy_InvalidInput;
        switch(event.type)
        {
        case SDL_JOYBUTTONDOWN:
            jn=get_joystick_number_by_id(event.jbutton.which);
            if(!open_joysticks[jn].isController)
            {
                etype=Joy_ButtonDown;
                bnum=event.jbutton.button;
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            jn=get_joystick_number_by_id(event.cbutton.which);
            if(open_joysticks[jn].isController)
            {
                etype=Joy_ButtonDown;
                bnum=event.cbutton.button;
            }
            break;
        case SDL_JOYAXISMOTION:
            jn=get_joystick_number_by_id(event.jaxis.which);
            if(!open_joysticks[jn].isController)
            {
                etype=Joy_AxisMotion;
                bnum=event.jaxis.axis;
                aval=event.jaxis.value;
            }
            break;
        case SDL_CONTROLLERAXISMOTION:
            jn=get_joystick_number_by_id(event.caxis.which);
            if(open_joysticks[jn].isController)
            {
                etype=Joy_AxisMotion;
                bnum=event.caxis.axis;
                aval=event.caxis.value;
            }
            break;
        case SDL_JOYHATMOTION:
            jn=get_joystick_number_by_id(event.jhat.which);
            if(!open_joysticks[jn].isController)
            {
                etype=Joy_HatMotion;
                bnum=event.jhat.hat;
                bval=event.jhat.value;
            }
            break;
        default:
            do_process_joystick_device_events(&event);
            break;
        }
        
        switch(etype)
        {
            case Joy_ButtonDown:
                key = ((jn & 15) << 12) | JOY_BUTTON << 8 | (bnum & 255);
                done = TRUE;
                break;
            case Joy_AxisMotion:
                if( ((u32)abs(aval) >> 14) != 0 )
                {
                    key = ((jn & 15) << 12) | JOY_AXIS << 8 | ((bnum & 127) << 1);
                    if (aval > 0)
                        key |= 1;
                    done = TRUE;
                }
                break;
            case Joy_HatMotion:
                if (bval != SDL_HAT_CENTERED) {
                    key = ((jn & 15) << 12) | JOY_HAT << 8 | ((bnum & 63) << 2);
                    if ((bval & SDL_HAT_UP) != 0)
                        key |= JOY_HAT_UP;
                    else if ((bval & SDL_HAT_RIGHT) != 0)
                        key |= JOY_HAT_RIGHT;
                    else if ((bval & SDL_HAT_DOWN) != 0)
                        key |= JOY_HAT_DOWN;
                    else if ((bval & SDL_HAT_LEFT) != 0)
                        key |= JOY_HAT_LEFT;
                    done = TRUE;
                }
                break;
            default:
                break;
        }
    }
    if(modified)
      (*modified)=done;
    return key;
}

/* Get and set a new joystick key */
u16 get_set_joy_key(int index) {
  joypad_cfg[index] = get_joy_key(index);

  return joypad_cfg[index];
}

static signed long
screen_to_touch_range( signed long scr, float size_ratio) {
  return (signed long)((float)scr / size_ratio);
}

/* Set mouse coordinates */
static void set_mouse_coord(signed long x,signed long y)
{
  if(x<0) x = 0; else if(x>255) x = 255;
  if(y<0) y = 0; else if(y>192) y = 192;
  mouse.x = x;
  mouse.y = y;
}

// Adapted from Windows port
static bool allowUpAndDown = false;
static buttonstruct<int> cardinalHeldTime = {};

static void RunAntipodalRestriction(const buttonstruct<bool>& pad)
{
	if (allowUpAndDown)
		return;

	pad.U ? (cardinalHeldTime.U++) : (cardinalHeldTime.U=0);
	pad.D ? (cardinalHeldTime.D++) : (cardinalHeldTime.D=0);
	pad.L ? (cardinalHeldTime.L++) : (cardinalHeldTime.L=0);
	pad.R ? (cardinalHeldTime.R++) : (cardinalHeldTime.R=0);
}
static void ApplyAntipodalRestriction(buttonstruct<bool>& pad)
{
	if (allowUpAndDown)
		return;

	// give preference to whichever direction was most recently pressed
	if (pad.U && pad.D) {
		if (cardinalHeldTime.U < cardinalHeldTime.D)
			pad.D = false;
		else
			pad.U = false;
	}
	if (pad.L && pad.R) {
		if (cardinalHeldTime.L < cardinalHeldTime.R)
			pad.R = false;
		else
			pad.L = false;
	}
}

/* Update NDS keypad */
void update_keypad(u16 keys)
{
	// Set raw inputs
	{
		buttonstruct<bool> input = {};
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
		input.F = (keys>>14)&1;
		RunAntipodalRestriction(input);
		NDS_setPad(
			input.R, input.L, input.D, input.U,
			input.T, input.S, input.B, input.A,
			input.Y, input.X, input.W, input.E,
			input.G, input.F);
	}
	
	// Set real input
	NDS_beginProcessingInput();
	{
		UserButtons& input = NDS_getProcessingUserInput().buttons;
		ApplyAntipodalRestriction(input);
	}
	NDS_endProcessingInput();
}

/* Retrieve current NDS keypad */
u16 get_keypad( void)
{
  u16 keypad;
  keypad = ~MMU.ARM7_REG[0x136];
  keypad = (keypad & 0x3) << 10;
#ifdef MSB_FIRST
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
  
  int jn;
  joystick_input_type etype=Joy_InvalidInput;
  Sint16 aval;
  Uint8 bnum, bval;

  switch ( event->type)
    {
    case SDL_JOYAXISMOTION:
      jn=get_joystick_number_by_id(event->jaxis.which);
      if(!open_joysticks[jn].isController)
      {
        etype=Joy_AxisMotion;
        bnum=event->jaxis.axis;
        aval=event->jaxis.value;
      }
      break;

    case SDL_JOYHATMOTION:
      jn=get_joystick_number_by_id(event->jhat.which);
      if(!open_joysticks[jn].isController)
      {
        etype=Joy_HatMotion;
        bnum=event->jhat.hat;
        bval=event->jhat.value;
      }
      break;

    case SDL_JOYBUTTONDOWN:
      jn=get_joystick_number_by_id(event->jbutton.which);
      if(!open_joysticks[jn].isController)
      {
        etype=Joy_ButtonDown;
        bnum=event->jbutton.button;
      }
      break;

    case SDL_JOYBUTTONUP:
      jn=get_joystick_number_by_id(event->jbutton.which);
      if(!open_joysticks[jn].isController)
      {
        etype=Joy_ButtonUp;
        bnum=event->jbutton.button;
      }
      break;

    case SDL_CONTROLLERAXISMOTION:
      jn=get_joystick_number_by_id(event->caxis.which);
      if(open_joysticks[jn].isController)
      {
        etype=Joy_AxisMotion;
        bnum=event->caxis.axis;
        aval=event->caxis.value;
      }
      break;

    case SDL_CONTROLLERBUTTONDOWN:
      jn=get_joystick_number_by_id(event->cbutton.which);
      if(open_joysticks[jn].isController)
      {
        etype=Joy_ButtonDown;
        bnum=event->cbutton.button;
      }
      break;

    case SDL_CONTROLLERBUTTONUP:
      jn=get_joystick_number_by_id(event->cbutton.which);
      if(open_joysticks[jn].isController)
      {
        etype=Joy_ButtonUp;
        bnum=event->cbutton.button;
      }
      break;

    case SDL_CONTROLLERTOUCHPADDOWN:
      jn=get_joystick_number_by_id(event->ctouchpad.which);
      if(open_joysticks[jn].isController)
      {
        if(event->ctouchpad.finger==0)
          if(!touchpad.down)
          {
            touchpad.down=1;
            touchpad.x=event->ctouchpad.x;
            touchpad.y=event->ctouchpad.y;
          }
      }
      break;

    case SDL_CONTROLLERTOUCHPADMOTION:
      jn=get_joystick_number_by_id(event->ctouchpad.which);
      if(open_joysticks[jn].isController)
        if(event->ctouchpad.finger==0)
        {
          touchpad.x=event->ctouchpad.x;
          touchpad.y=event->ctouchpad.y;
        }
      break;

    case SDL_CONTROLLERTOUCHPADUP:
      jn=get_joystick_number_by_id(event->ctouchpad.which);
      if(open_joysticks[jn].isController)
      {
        if(event->ctouchpad.finger==0)
          if(touchpad.down)
          {
             touchpad.click = 1;
             if(touchpad.down > 1) touchpad.down = 0;
             touchpad.x=event->ctouchpad.x;
             touchpad.y=event->ctouchpad.y;
          }
      }
      break;

    default:
      processed = 0;
      break;
    }
  
  switch(etype)
  {
    /* Joystick axis motion 
       Note: button constants have a 1bit offset. */
    case Joy_AxisMotion:
      key_code = ((jn & 15) << 12) | JOY_AXIS << 8 | ((bnum & 127) << 1);
      if( ((u32)abs(aval) >> 14) != 0 )
        {
          if (aval > 0)
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
    
    /* Diagonal positions will be treated as two separate keys being activated, rather than a single diagonal key. */
    /* JOY_HAT_* are sequential integers, rather than a bitmask */
    case Joy_HatMotion:
      key_code = ((jn & 15) << 12) | JOY_HAT << 8 | ((bnum & 63) << 2);
      key_u = lookup_joy_key( key_code | JOY_HAT_UP );
      key_r = lookup_joy_key( key_code | JOY_HAT_RIGHT );
      key_d = lookup_joy_key( key_code | JOY_HAT_DOWN );
      key_l = lookup_joy_key( key_code | JOY_HAT_LEFT );
      if ((key_u != 0) && ((bval & SDL_HAT_UP) != 0))
        ADD_KEY( *keypad, key_u );
      else if (key_u != 0)
        RM_KEY( *keypad, key_u );
      if ((key_r != 0) && ((bval & SDL_HAT_RIGHT) != 0))
        ADD_KEY( *keypad, key_r );
      else if (key_r != 0)
        RM_KEY( *keypad, key_r );
      if ((key_d != 0) && ((bval & SDL_HAT_DOWN) != 0))
        ADD_KEY( *keypad, key_d );
      else if (key_d != 0)
        RM_KEY( *keypad, key_d );
      if ((key_l != 0) && ((bval & SDL_HAT_LEFT) != 0))
        ADD_KEY( *keypad, key_l );
      else if (key_l != 0)
        RM_KEY( *keypad, key_l );
      break;
    
    /* Joystick button pressed */
    case Joy_ButtonDown:
      key_code = ((jn & 15) << 12) | JOY_BUTTON << 8 | (bnum & 255);
      key = lookup_joy_key( key_code );
      if (key != 0)
        ADD_KEY( *keypad, key );
      break;
    
    /* Joystick button released */
    case Joy_ButtonUp:
      key_code = ((jn & 15) << 12) | JOY_BUTTON << 8 | (bnum & 255);
      key = lookup_joy_key( key_code );
      if (key != 0)
        RM_KEY( *keypad, key );
      break;
    
    default:
      break;
  }

  return processed;
}

/*
 * The real joystick connect/disconnect processing function
 * Not static because we need it in some frontends during gamepad button configuration
 */
int
do_process_joystick_device_events(SDL_Event* event) {
  int processed = 1, n;
  switch(event->type) {
    /* Joystick disconnected */
    case SDL_JOYDEVICEREMOVED:
      n=get_joystick_number_by_id(event->jdevice.which);
      if(n != -1) {
        if(!open_joysticks[n].isController) {
          printf("Joystick %d disconnected\n", n);
          SDL_JoystickClose(open_joysticks[n].handle.joy);
          open_joysticks[n]={FALSE, {NULL}, -1};
        }
      }
      break;
    
    /* Joystick connected */
    case SDL_JOYDEVICEADDED:
      if(event->jdevice.which<MAX_JOYSTICKS) {
        if(!SDL_IsGameController(event->jdevice.which)) {
          //Filter connect events for joysticks already initialized in init_joy
          if(!open_joysticks[event->jdevice.which].handle.joy) {
            SDL_Joystick* joy = SDL_JoystickOpen(event->jdevice.which);
            if(joy) {
              printf("Joystick %d %s\n", event->jdevice.which, SDL_JoystickNameForIndex(event->jdevice.which));
              printf("Axes: %d\n", SDL_JoystickNumAxes(joy));
              printf("Buttons: %d\n", SDL_JoystickNumButtons(joy));
              printf("Trackballs: %d\n", SDL_JoystickNumBalls(joy));
              printf("Hats: %d\n\n", SDL_JoystickNumHats(joy));
              open_joysticks[event->cdevice.which].isController=FALSE;
              open_joysticks[event->cdevice.which].handle.joy=joy;
              open_joysticks[event->cdevice.which].id=SDL_JoystickInstanceID(joy);
            }
            else
              fprintf(stderr, "Failed to open joystick %d: %s\n", event->jdevice.which, SDL_GetError());
          }
        }
      }
      else
        printf("Joystick %d connected to the system, but maximum supported joystick index is %d, ignoring\n",
          event->jdevice.which, MAX_JOYSTICKS-1);
      break;
    
    /* Game controller disconnected */
    case SDL_CONTROLLERDEVICEREMOVED:
      n=get_joystick_number_by_id(event->cdevice.which);
      if(n != -1) {
        if(open_joysticks[n].isController) {
          printf("Game controller %d disconnected\n", n);
          SDL_GameControllerClose(open_joysticks[n].handle.gcont);
          open_joysticks[n]={FALSE, {NULL}, -1};
        }
      }
      break;
    
    /* Game controller connected */
    case SDL_CONTROLLERDEVICEADDED:
      if(event->cdevice.which<MAX_JOYSTICKS) {
        if(SDL_IsGameController(event->cdevice.which)) {
          //Filter connect events for joysticks already initialized in init_joy
          if(!open_joysticks[event->cdevice.which].handle.gcont) {
            SDL_GameController* gcont = SDL_GameControllerOpen(event->cdevice.which);
            if(gcont) {
              printf("Game controller %d %s\n", event->cdevice.which, SDL_GameControllerNameForIndex(event->cdevice.which));
              printf("Axes: %d\n", SDL_JoystickNumAxes(SDL_GameControllerGetJoystick(gcont)));
              printf("Buttons: %d\n", SDL_JoystickNumButtons(SDL_GameControllerGetJoystick(gcont)));
              printf("Touchpads: %d\n", SDL_GameControllerGetNumTouchpads(gcont));
              printf("%s\n", SDL_GameControllerHasSensor(gcont, SDL_SENSOR_ACCEL) ? "Accelerometer present" : "Accelerometer not present");
              printf("%s\n", SDL_GameControllerHasSensor(gcont, SDL_SENSOR_GYRO) ? "Gyroscope present" : "Gyroscope not present");
              open_joysticks[event->cdevice.which].isController=TRUE;
              open_joysticks[event->cdevice.which].handle.gcont=gcont;
              open_joysticks[event->cdevice.which].id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gcont));
            }
            else
              fprintf(stderr, "Failed to open game controller %d: %s\n", event->cdevice.which, SDL_GetError());
          }
        }
      }
      else
        printf("Game controller %d connected to the system, but maximum supported joystick index is %d, ignoring\n",
          event->cdevice.which, MAX_JOYSTICKS-1);
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
  if(SDL_GameControllerEventState(SDL_QUERY) == SDL_IGNORE)
    SDL_GameControllerEventState(SDL_ENABLE);
  
  /* There's an event waiting to be processed? */
  while (SDL_PollEvent(&event))
    {
      if(!do_process_joystick_events( keypad, &event))
        do_process_joystick_device_events(&event);
    }
}

/*
 * Process only joystick connect/disconnect events
 */
void process_joystick_device_events() {
  SDL_Event event;

  /* IMPORTANT: Reenable joystick events if needed. */
  if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
    SDL_JoystickEventState(SDL_ENABLE);
  if(SDL_GameControllerEventState(SDL_QUERY) == SDL_IGNORE)
    SDL_GameControllerEventState(SDL_ENABLE);

  /* There's an event waiting to be processed? */
  while (SDL_PollEvent(&event))
    do_process_joystick_device_events(&event);
}

int get_joystick_number_by_id(SDL_JoystickID id)
{
  int i;
  for(i=0; i<MAX_JOYSTICKS; ++i)
      if(open_joysticks[i].id==id)
        return i;
  return -1;
}

int get_number_of_joysticks()
{
  int i, n=0;
  for(i=0; i<MAX_JOYSTICKS; ++i)
    if(open_joysticks[i].id>=0)
      ++n;
  return n;
}

static u16 shift_pressed;

void
process_ctrls_event( SDL_Event& event,
                      struct ctrls_event_config *cfg)
{
  u16 key;
  if ( !do_process_joystick_events( &cfg->keypad, &event)) {
    switch (event.type)
    {
      case SDL_WINDOWEVENT:
        switch (event.window.event) {
          case SDL_WINDOWEVENT_RESIZED:
            cfg->resize_cb(event.window.data1, event.window.data2, cfg->screen_texture);
            break;
          case SDL_WINDOWEVENT_FOCUS_GAINED:
            if (cfg->auto_pause) {
              cfg->focused = 1;
              SPU_Pause(0);
              driver->AddLine("Auto pause disabled");
            }
            break;
          case SDL_WINDOWEVENT_FOCUS_LOST:
            if (cfg->auto_pause) {
              cfg->focused = 0;
              SPU_Pause(1);
            }
            break;
        }
        break;

      case SDL_KEYDOWN:
        if ((event.key.keysym.sym == SDLK_RETURN) && (event.key.keysym.mod & KMOD_ALT)) {
            SDL_SetWindowFullscreen(cfg->window, fullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
            fullscreen = !fullscreen;
            break;
        }

        if (event.key.keysym.sym == SDLK_LSHIFT) {
            shift_pressed |= 1;
        } else if (event.key.keysym.sym == SDLK_RSHIFT) {
            shift_pressed |= 2;
        }

        key = lookup_key(event.key.keysym.sym);
        ADD_KEY( cfg->keypad, key );
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
                  driver->AddLine("Fake mic enabled");
                else
                  driver->AddLine("Fake mic disabled");
                break;
#endif
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
		emu_halt(EMUHALT_REASON_USER_REQUESTED_HALT, NDSErrorTag_None);
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
        if(event.button.button==1 && !mouse.down)
          mouse.down = 1;
        break;

      case SDL_MOUSEMOTION:
	{
          signed long scaled_x =
            screen_to_touch_range( event.button.x,
                                     cfg->nds_screen_size_ratio);
          signed long scaled_y =
            screen_to_touch_range( event.button.y,
                                     cfg->nds_screen_size_ratio);

          if(!cfg->horizontal && scaled_y >= 192)
            set_mouse_coord( scaled_x, scaled_y - 192);
          else if(cfg->horizontal && scaled_x >= 256)
            set_mouse_coord( scaled_x - 256, scaled_y);
        }
        break;

      case SDL_MOUSEBUTTONUP:
        if(mouse.down) {
		mouse.click = 1;
		if(mouse.down > 1) mouse.down = 0;
	}
        break;

      case SDL_QUIT:
        cfg->sdl_quit = 1;
        break;

      default:
        break;
    }
  }
}

