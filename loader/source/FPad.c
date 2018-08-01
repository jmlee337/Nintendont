/*

Nintendont (Loader) - Playing Gamecubes in Wii mode

Copyright (C) 2013  crediar

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include "FPad.h"
#include "global.h"
#include "exi.h"

#include <unistd.h>
#include "menu.h"

static u32 PAD_Pressed;
static s8  PAD_Stick_Y;
static s8  PAD_Stick_X;
static bool SLock;
static u32 SpeedX;
static u32 Repeat;
#define DELAY_START	900

void FPAD_Init( void )
{
	PAD_Init();

	PAD_Pressed = 0;
	PAD_Stick_Y = 0;
	PAD_Stick_X = 0;
	Repeat = 0;
}
void FPAD_Update( void )
{
	u8 i;

	PAD_Pressed = 0;
	PAD_Stick_Y = 0;
	PAD_Stick_X = 0;
	PAD_ScanPads();
	for(i = 0; i < PAD_CHANMAX; ++i)
	{
		PAD_Pressed |= PAD_ButtonsDown(i) | PAD_ButtonsHeld(i);
		PAD_Stick_Y |= PAD_StickY(i);
		PAD_Stick_X |= PAD_StickX(i);
	}

	if( PAD_Pressed == 0 && ( PAD_Stick_Y < 25 && PAD_Stick_Y > -25 )  && ( PAD_Stick_X < 25 && PAD_Stick_X > -25 ) )
	{
		Repeat = 0;
		SLock = false;
		SpeedX= DELAY_START;
	}
	if(SLock == true)
	{
		if(Repeat > 0)
		{
			Repeat--;
			if(Repeat == 0)
				SLock = false;
		}
	}
	//Power Button
	if((*(vu32*)0xCD8000C8) & 1)
		SetShutdown();
}
bool FPAD_Up( bool ILock )
{
	if( !ILock && SLock ) return false;

	if((PAD_Pressed & PAD_BUTTON_UP) || (PAD_Stick_Y > 30))
	{
		Repeat = 2;
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_Down( bool ILock )
{
	if( !ILock && SLock ) return false;

	if((PAD_Pressed & PAD_BUTTON_DOWN) || (PAD_Stick_Y < -30))
	{
		Repeat = 2;
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_Left( bool ILock )
{
	if( !ILock && SLock ) return false;

	if((PAD_Pressed & PAD_BUTTON_LEFT) || (PAD_Stick_X < -30))
	{
		Repeat = 2;
		SLock = true;
		return true;
	}
	return false;
}
bool FPAD_Right( bool ILock )
{
	if( !ILock && SLock ) return false;

	if((PAD_Pressed & PAD_BUTTON_RIGHT) || ( PAD_Stick_X > 30 ))
	{
		Repeat = 2;
		SLock = true;
		return true;
	}
	return false;
}
bool FPAD_OK( bool ILock )
{
	if( !ILock && SLock ) return false;

	if(PAD_Pressed & PAD_BUTTON_A)
	{
		Repeat = 0;
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_X( bool ILock )
{
	if( !ILock && SLock ) return false;

	if(PAD_Pressed & PAD_BUTTON_X)
	{
		Repeat = 0;
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_Y( bool ILock )
{
	if( !ILock && SLock ) return false;

	if(PAD_Pressed & PAD_BUTTON_Y)
	{
		Repeat = 0;
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_Cancel( bool ILock )
{
	if( !ILock && SLock ) return false;

	if(PAD_Pressed & PAD_BUTTON_B)
	{
		Repeat = 0;
		SLock = true;
		return true;
	}
	return false;
}

bool FPAD_Start( bool ILock )
{
	if( !ILock && SLock ) return false;

	if(PAD_Pressed & PAD_BUTTON_START)
	{
		Repeat = 0;
		SLock = true;
		return true;
	}
	return false;
}

inline void Screenshot(void) {
}
