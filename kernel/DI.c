/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode

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

// strcasecmp()
#include <strings.h>

#include "global.h"
#include "DI.h"
#include "RealDI.h"
#include "string.h"
#include "common.h"
#include "alloc.h"
#include "dol.h"
#include "Config.h"
#include "Patch.h"
#include "Stream.h"
#include "ReadSpeed.h"
#include "ISO.h"
#include "FST.h"
#include "usbstorage.h"

#include "ff_utf8.h"
static u8 DummyBuffer[0x1000] __attribute__((aligned(32)));
extern u32 s_cnt;

#ifndef DEBUG_DI
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

struct ipcmessage DI_CallbackMsg;
u32 DI_MessageQueue = 0xFFFFFFFF;
static u8 *DI_MessageHeap = NULL;
bool DI_IRQ = false;
u32 DI_Thread = 0;
s32 DI_Handle = -1;
u64 ISOShift64 = 0;
static u32 Streaming = 0; //internal
extern u32 StreamSize, StreamStart, StreamCurrent, StreamEndOffset;

u32 DiscChangeIRQ	= 0;

static char GamePath[256] ALIGNED(32);
extern const u8 *DiskDriveInfo;
extern u32 FSTMode;
extern u32 RealDiscCMD;
extern u32 RealDiscError;
u32 WaitForRealDisc = 0;
u32 DiscRequested = 0;

u8 *const DI_READ_BUFFER = (u8*)0x12E80000;
const u32 DI_READ_BUFFER_LENGTH = 0x80000;

extern u32 GAME_ID;
extern u16 GAME_ID6;
extern u32 TITLE_ID;

static u8 *MediaBuffer;
static u8 *NetworkCMDBuffer;

// Multi-disc filenames.
static const char disc_filenames[8][16] = {
	// Disc 1
	"game.ciso", "game.cso", "game.gcm", "game.iso",
	// Disc 2
	"disc2.ciso", "disc2.cso", "disc2.gcm", "disc2.iso"
};

// Filename portions for 2-disc mode.
// Points to entries in disc_filenames.
// NOTE: If either is NULL, assume single-disc mode.
static const char *DI_2disc_filenames[2] = {NULL, NULL};

void DIRegister(void)
{
	DI_MessageHeap = malloca(0x20, 0x20);
	DI_MessageQueue = mqueue_create( DI_MessageHeap, 8 );
	device_register( "/dev/mydi", DI_MessageQueue );
}

void DIUnregister(void)
{
	mqueue_destroy(DI_MessageQueue);
	DI_MessageQueue = 0xFFFFFFFF;
	free(DI_MessageHeap);
	DI_MessageHeap = NULL;
}

bool DiscCheckAsync( void )
{
	if(RealDiscCMD)
		RealDI_Update();
	return (DI_CallbackMsg.result == 0);
}

static void DiscReadAsync(u32 Buffer, u32 Offset, u32 Length, u32 Mode)
{
	DIFinishAsync(); //if something is still running
	DI_CallbackMsg.result = -1;
	sync_after_write(&DI_CallbackMsg, 0x20);
	IOS_IoctlAsync( DI_Handle, Mode, (void*)Offset, 0, (void*)Buffer, Length, DI_MessageQueue, &DI_CallbackMsg );
}

void DiscReadSync(u32 Buffer, u32 Offset, u32 Length, u32 Mode)
{
	DiscReadAsync(Buffer, Offset, Length, Mode);
	DIFinishAsync();
}

void DIinit( bool FirstTime )
{
	//This debug statement seems to cause crashing.
	//dbgprintf("DIInit()\r\n");

	if(DI_Handle >= 0) //closes old file
		IOS_Close(DI_Handle);

	if (FirstTime)
	{
		if(RealDiscCMD == 0)
		{
			// Check if this is a 2-disc game.
			u32 i, slash_pos;
			char TempDiscName[256];
			strcpy(TempDiscName, ConfigGetGamePath());

			//search the string backwards for '/'
			for (slash_pos = strlen(TempDiscName); slash_pos > 0; --slash_pos)
			{
				if (TempDiscName[slash_pos] == '/')
					break;
			}
			slash_pos++;

			// First, check if the disc filename matches
			// the expected filenames for multi-disc games.
			int checkIdxMin = -1, checkIdxMax = -1;
			const char **DI_2disc_otherdisc = NULL;
			DI_2disc_filenames[0] = NULL;
			DI_2disc_filenames[1] = NULL;
			for (i = 0; i < 8; i++)
			{
				if (!strcasecmp(TempDiscName+slash_pos, disc_filenames[i]))
				{
					// Filename is either:
					// -  game.(ciso|cso|gcm|iso) (Disc 1)
					// - disc2.(ciso|cso|gcm|iso) (Disc 2)
					const int discIdx = i / 4;	// either 0 or 1
					DI_2disc_filenames[discIdx] = disc_filenames[i];

					// Set variables to check for the other disc.
					if (discIdx == 0) {
						checkIdxMin = 4;
						checkIdxMax = 7;
						DI_2disc_otherdisc = &DI_2disc_filenames[1];
					} else {
						checkIdxMin = 0;
						checkIdxMax = 3;
						DI_2disc_otherdisc = &DI_2disc_filenames[0];
					}
					break;
				}
			}

			if (DI_2disc_otherdisc != NULL)
			{
				// Check for the other disc.
				for (i = checkIdxMin; i <= checkIdxMax; i++)
				{
					strcpy(TempDiscName+slash_pos, disc_filenames[i]);
					FIL ExistsFile;
					s32 ret = f_open_char(&ExistsFile, TempDiscName, FA_READ);
					if (ret == FR_OK)
					{
						// Found the other disc image.
						f_close(&ExistsFile);
						*DI_2disc_otherdisc = disc_filenames[i];
						break;
					}
				}
			}
		}
		write32( DIP_STATUS, 0x54 ); //mask and clear interrupts
		write32( DIP_COVER, 4 ); //disable cover irq which DIP enabled

		sync_before_read((void*)0x13003000, 0x20);
		ISOShift64 = (u64)(read32(0x1300300C)) << 2;

		MediaBuffer = (u8*)malloc( 0x40 );
		memset32( MediaBuffer, 0, 0x40 );

		NetworkCMDBuffer = (u8*)malloc( 512 );
		memset32( NetworkCMDBuffer, 0, 512 );

		memset32( (void*)DI_BASE, 0, 0x30 );
		sync_after_write( (void*)DI_BASE, 0x40 );
	}
	DI_Handle = IOS_Open( "/dev/mydi", 0 );

	GAME_ID = read32(0);
	GAME_ID6 = R16(4);
	TITLE_ID = (GAME_ID >> 8);

	ReadSpeed_Init();
}
bool DIChangeDisc( u32 DiscNumber )
{
	// Don't do anything if multi-disc mode isn't enabled.
	if (!DI_2disc_filenames[0] ||
		!DI_2disc_filenames[1] ||
		DiscNumber > 1)
	{
		return false;
	}

	u32 slash_pos;
	char* DiscName = ConfigGetGamePath();

	//search the string backwards for '/'
	for (slash_pos = strlen(DiscName); slash_pos > 0; --slash_pos)
	{
		if (DiscName[slash_pos] == '/')
			break;
	}
	slash_pos++;

	_sprintf(DiscName+slash_pos, DI_2disc_filenames[DiscNumber]);
	dbgprintf("New Gamepath:\"%s\"\r\n", DiscName );

	DIinit(false);
	return true;
}

void DIInterrupt()
{
	if(ReadSpeed_End() == 0)
		return; //still busy
	sync_before_read( (void*)DI_BASE, 0x40 );
	/* Update DMA registers when needed */
	if(read32(DI_CONTROL) & 2)
	{
		if( TITLE_ID == 0x47544B || //Turok Evolution
			TITLE_ID == 0x47514C ) //Dora the Explorer
		{	/* Manually invalidate data */
			write32(DI_INV_ADR, read32(DI_DMA_ADR));
			write32(DI_INV_LEN, read32(DI_DMA_LEN));
		}
		write32(DI_DMA_ADR, read32(DI_DMA_ADR) + read32(DI_DMA_LEN));
	}
	write32( DI_CONTROL, read32(DI_CONTROL) & 2 ); // finished command
	u32 di_status = read32(DI_STATUS);
	if(RealDiscError == 0)
	{
		write32( DI_DMA_LEN, 0 ); // all data handled, clear length
		write32( DI_STATUS, di_status | 0x10 ); //set TC
		sync_after_write( (void*)DI_BASE, 0x40 );
		if( di_status & 0x8 ) //TC Interrupt enabled
		{
			write32( DI_INT, 0x4 ); // DI IRQ
			sync_after_write( (void*)DI_INT, 0x20 );
			write32( HW_IPC_ARMCTRL, 8 ); //throw irq
			//dbgprintf("Disc Interrupt\r\n");
		}
	}
	else
	{
		write32( DI_STATUS, di_status | 4 ); //set Error
		sync_after_write( (void*)DI_BASE, 0x40 );
		if( di_status & 2 ) //Error Interrupt enabled
		{
			write32( DI_INT, 0x4 ); // DI IRQ
			sync_after_write( (void*)DI_INT, 0x20 );
			write32( HW_IPC_ARMCTRL, 8 ); //throw irq
			//dbgprintf("Disc Interrupt\r\n");
		}
	}
	DI_IRQ = false;
}

void DIUpdateRegisters( void )
{
	if( DI_IRQ == true ) //still working
		return;

	u32 i;
	u32 DIOK = 0,DIcommand;

	sync_before_read( (void*)DI_BASE, 0x40 );
	if( read32(DI_CONTROL) & 1 )
	{
		udelay(50); //security delay
		write32( DI_STATUS, read32(DI_STATUS) & 0x2A ); //clear status
		DIcommand = read32(DI_CMD_0) >> 24;
		switch( DIcommand )
		{
			default:
			{
				dbgprintf("DI: Unknown command:%02X\r\n", DIcommand );

				for( i = 0; i < 0x30; i+=4 )
					dbgprintf("0x%08X:0x%08X\r\n", i, read32( DI_BASE + i ) );
				dbgprintf("\r\n");

				memset32( (void*)DI_BASE, 0xdeadbeef, 0x30 );
				Shutdown();

			} break;
			case 0xE1:	// play Audio Stream
			{
				//dbgprintf("DIP:DVDAudioStream(%d)\n", (read32(DI_CMD_0) >> 16 ) & 0xFF );
				switch( (read32(DI_CMD_0) >> 16) & 0xFF )
				{
					case 0x00:
						StreamStartStream(read32(DI_CMD_1) << 2, read32(DI_CMD_2));
						Streaming = 1;
						break;
					case 0x01:
						StreamEndStream();
						Streaming = 0;
						break;
					default:
						break;
				}
				DIOK = 2;
			} break;
			case 0xE2:	// request Audio Status
			{
				switch( (read32(DI_CMD_0) >> 16) & 0xFF )
				{
					case 0x00:	// Streaming?
						Streaming = !!(StreamCurrent);
						write32( DI_IMM, Streaming );
						break;
					case 0x01:	// What is the current address?
						if(Streaming)
						{
							if(StreamCurrent)
								write32( DI_IMM, ALIGN_BACKWARD(StreamCurrent, 0x8000) >> 2 );
							else
								write32( DI_IMM, StreamEndOffset >> 2 );
						}
						else
							write32( DI_IMM, 0 );
						break;
					case 0x02:	// disc offset of file
						write32( DI_IMM, StreamStart >> 2 );
						break;
					case 0x03:	// Size of file
						write32( DI_IMM, StreamSize );
						break;
					default:
						break;
				}
				//dbgprintf("DIP:DVDAudioStatus( %d, %08X )\n", (read32(DI_CMD_0) >> 16) & 0xFF, read32(DI_IMM) );
				DIOK = 2;
			} break;
			case 0x12:
			{
				if(read32(DI_CMD_2) == 32)
				{
					//dbgprintf("DIP:DVDLowInquiry( 0x%08x, 0x%08x )\r\n", read32(DI_DMA_ADR), read32(DI_DMA_LEN));
					void *Buffer = (void*)P2C(read32(DI_DMA_ADR));
					memcpy( Buffer, DiskDriveInfo, 32 );
					sync_after_write( Buffer, 32 );
				}
				DIOK = 2;
			} break;
			case 0xAB:
			{
				if(FSTMode == 0 && RealDiscCMD == 0)
				{
					u32 Offset = read32(DI_CMD_1) << 2;
					//dbgprintf("DIP:DVDLowSeek( 0x%08X )\r\n", Offset);
					ISOSeek(Offset);
				}
				DIOK = 2;
			} break;
			case 0xE0:	// Get error status
			{
				if(WaitForRealDisc == 0 && RealDiscError == 0)
					write32( DI_IMM, 0x00000000 );
				else
				{	//we just always say disc got removed as error
					write32(DI_IMM, 0x1023a00);
					write32(DI_COVER, 1);
					RealDiscError = 0;
					WaitForRealDisc = 1;
				}
				DIOK = 2;
			} break;
			case 0xE3:	// stop Motor
			{
				dbgprintf("DIP:DVDLowStopMotor()\r\n");
				if(RealDiscCMD == 0)
				{
					u32 CDiscNumber = (read32(4) << 16 ) >> 24;
					dbgprintf("DIP:Current disc number:%u\r\n", CDiscNumber + 1 );

					if (DIChangeDisc( CDiscNumber ^ 1 ))
						DiscChangeIRQ = 1;
				}
				else if (!Datel)
				{	//we just always say disc got removed as error
					write32(DI_IMM, 0x1023a00);
					write32(DI_COVER, 1);
					RealDiscError = 0;
					WaitForRealDisc = 1;
				}
				ReadSpeed_Motor();
				DIOK = 2;

			} break;
			case 0xE4:	// Disable Audio
			{
				dbgprintf("DIP:DVDDisableAudio()\r\n");
				DIOK = 2;
			} break;
			case 0xA8:
			{
				u32 Buffer	= P2C(read32(DI_DMA_ADR));
				u32 Length	= read32(DI_CMD_2);
				u32 Offset	= read32(DI_CMD_1) << 2;

				dbgprintf( "DIP:DVDReadA8( 0x%08x, 0x%08x, 0x%08x )\r\n", Offset, Length, Buffer|0x80000000 );

				// Max GC disc offset
				if( Offset >= 0x57058000 )
				{
					dbgprintf("Unhandled Read\n");
					dbgprintf("DIP:DVDRead%02X( 0x%08x, 0x%08x, 0x%08x )\n", DIcommand, Offset, Length, Buffer|0x80000000 );
					Shutdown();
				}
				if( Buffer < 0x01800000 )
				{
					DiscReadAsync(Buffer, Offset, Length, 0);
					ReadSpeed_Start();
				}
				DIOK = 2;
			} break;
			case 0xF8:
			{
				u32 Buffer	= P2C(read32(DI_DMA_ADR));
				u32 Length	= read32(DI_CMD_2);
				u32 Offset	= read32(DI_CMD_1) << 2;

				dbgprintf( "DIP:DVDReadF8( 0x%08x, 0x%08x, 0x%08x )\r\n", Offset, Length, Buffer|0x80000000 );

				if( Buffer < 0x01800000 )
				{
					DiscReadSync(Buffer, Offset, Length, 0);
				}
				DIOK = 1;
			} break;
			case 0xF9:
			{
				DIOK = 1;
			} break;
		}

		if( DIOK )
		{
			if( DIOK == 2 )
				DI_IRQ = true;
			else
				write32(DI_CONTROL, 0);
		}
		sync_after_write( (void*)DI_BASE, 0x40 );
	}
	return;
}

extern u32 Patch31A0Backup;
static const u8 *di_src = NULL;
static char *di_dest = NULL;
static u32 di_length = 0;
static u32 di_offset = 0;
u32 DIReadThread(void *arg)
{
	//dbgprintf("DI Thread Running\r\n");
	struct ipcmessage *di_msg = NULL;
	while(1)
	{
		mqueue_recv( DI_MessageQueue, &di_msg, 0 );
		switch( di_msg->command )
		{
			case IOS_OPEN:
				if( strncmp("/dev/mydi", di_msg->open.device, 8 ) != 0 )
				{	//this should never happen
					mqueue_ack( di_msg, -6 );
					break;
				}
				if(RealDiscCMD == 0)
				{
					ISOClose();
					if( ISOInit() == false )
					{
						_sprintf( GamePath, "%s", ConfigGetGamePath() );
						//Try to switch to FST mode
						if( !FSTInit(GamePath) )
						{
							//dbgprintf("Failed to open:%s Error:%u\r\n", ConfigGetGamePath(), ret );	
							Shutdown();
						}
					}
				}
				mqueue_ack( di_msg, 24 );
				break;

			case IOS_CLOSE:
				if(RealDiscCMD == 0)
				{
					if( FSTMode )
						FSTCleanup();
					else
						ISOClose();
				}
				mqueue_ack( di_msg, 0 );
				break;

			case IOS_IOCTL:
				if(di_msg->ioctl.command == 2)
				{
					USBStorage_ReadSectors(read32(HW_TIMER) % s_cnt, 1, DummyBuffer);
					mqueue_ack( di_msg, 0 );
					break;
				}
				di_src = 0;
				di_dest = (char*)di_msg->ioctl.buffer_io;
				di_length = di_msg->ioctl.length_io;
				di_offset = (u32)di_msg->ioctl.buffer_in;
				u32 Offset = 0;
				u32 Length = di_length;
				for (Offset = 0; Offset < di_length; Offset += Length)
				{
					// NOTE: ISOShift64 is applied in the RealDI and ISO readers.
					// Not supported for FST.
					Length = di_length - Offset;
					if( RealDiscCMD )
						di_src = ReadRealDisc(&Length, di_offset + Offset, true);
					else if( FSTMode )
						di_src = FSTRead(GamePath, &Length, di_offset + Offset);
					else
						di_src = ISORead(&Length, di_offset + Offset);
					// Copy data at a later point to prevent MEM1 issues
					if(((u32)di_dest + Offset) <= 0x31A0)
					{
						u32 pos31A0 = 0x31A0 - ((u32)di_dest + Offset);
						Patch31A0Backup = read32((u32)di_src + pos31A0);
					}
					memcpy( di_dest + Offset, di_src, Length > di_length ? di_length : Length );
				}
				if(di_msg->ioctl.command == 0)
				{
					DoPatches(di_dest, di_length, di_offset);
					ReadSpeed_Setup(di_offset, di_length);
				}
				sync_after_write( di_dest, di_length );
				mqueue_ack( di_msg, 0 );
				break;

			case IOS_ASYNC:
				mqueue_ack( di_msg, 0 );
				break;
		}
	}
	return 0;
}

void DIFinishAsync()
{
	while(DiscCheckAsync() == false)
	{
		udelay(200); //wait for driver
		CheckOSReport();
	}
}
/*
struct _TGCInfo
{
	u32 tgcoffset;
	u32 doloffset;
	u32 fstoffset;
	u32 fstsize;
	u32 userpos;
	u32 fstupdate;
	u32 isTGC;
};
static struct _TGCInfo *const TGCInfo = (struct _TGCInfo*)0x130031E0;

#define PATCH_STATE_PATCH 2
extern u32 PatchState, DOLSize, DOLMinOff, DOLMaxOff;

bool DICheckTGC(u32 Buffer, u32 Length)
{
	if(*(vu32*)di_dest == 0xAE0F38A2) //TGC Magic
	{	//multidol, loader always comes after reading tgc header
		dbgprintf("Found TGC Header\n");
		TGCInfo->tgcoffset = di_offset + *(vu32*)(di_dest + 0x08);
		TGCInfo->doloffset = di_offset + *(vu32*)(di_dest + 0x1C);
		TGCInfo->fstoffset = di_offset + *(vu32*)(di_dest + 0x10);
		TGCInfo->fstsize = *(vu32*)(di_dest + 0x14);
		TGCInfo->userpos = *(vu32*)(di_dest + 0x24);
		TGCInfo->fstupdate = *(vu32*)(di_dest + 0x34) - *(vu32*)(di_dest + 0x24) - di_offset;
		TGCInfo->isTGC = 1;
		sync_after_write((void*)TGCInfo, sizeof(struct _TGCInfo));
		return true;
	}
	else if(di_offset == 0x2440)
	{
		u16 company = (read32(0x4) >> 16);
		if(company == 0x3431 || company == 0x3730 || TITLE_ID == 0x474143 || TITLE_ID == 0x47434C || TITLE_ID == 0x475339)
		{	//we can patch the loader in this case, that works for some reason
			dbgprintf("Game is resetting to original loader, using original\n");
			PatchState = PATCH_STATE_PATCH;
			DOLSize = Length;
			DOLMinOff = Buffer;
			DOLMaxOff = DOLMinOff + Length;
		}
		else
		{	//multidol, just clear tgc data structure for it to load the original
			dbgprintf("Game is resetting to original loader, using multidol\n");
			memset32((void*)TGCInfo, 0, sizeof(struct _TGCInfo));
			sync_after_write((void*)TGCInfo, sizeof(struct _TGCInfo));
			return true;
		}
	}
	else
		dbgprintf("Game is loading another DOL\n");
	return false;
}
*/