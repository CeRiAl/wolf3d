/* wl_main.c */

#include "wl_def.h"

/*
=============================================================================

						   WOLFENSTEIN 3-D

					  An Id Software production

						   by John Carmack

=============================================================================
*/

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/


#define FOCALLENGTH     (0x5700l)               // in global coordinates
#define VIEWGLOBAL      0x10000                 // globals visable flush to wall

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

char            str[80],str2[20];
int                     dirangle[9] = {0,ANGLES/8,2*ANGLES/8,3*ANGLES/8,4*ANGLES/8,
	5*ANGLES/8,6*ANGLES/8,7*ANGLES/8,ANGLES};

//
// proejection variables
//
fixed           focallength;
int             viewwidth;
int             viewheight;
int             centerx;
int             shootdelta;                     // pixels away from centerx a target can be
fixed           scale,maxslope;
long            heightnumerator;
int                     minheightdiv;


void            Quit (char *error);

boolean         startgame,loadedgame,virtualreality;
int             mouseadjustment;

char	configname[13]="config.";

unsigned xoffset, yoffset;

int _argc;
char **_argv;

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/


/*
====================
=
= ReadConfig
=
====================
*/

void ReadConfig(void)
{
	int                     file;
	SDMode          sd;
	SMMode          sm;
	SDSMode         sds;


	if ( (file = open(configname,O_BINARY | O_RDONLY)) != -1)
	{
	//
	// valid config file
	//
		read(file,Scores,sizeof(HighScore) * MaxScores);

		read(file,&sd,sizeof(sd));
		read(file,&sm,sizeof(sm));
		read(file,&sds,sizeof(sds));

		read(file,&mouseenabled,sizeof(mouseenabled));
		read(file,&joystickenabled,sizeof(joystickenabled));
		read(file,&joypadenabled,sizeof(joypadenabled));
		read(file,&joystickport,sizeof(joystickport));

		read(file,&dirscan,sizeof(dirscan));
		read(file,&buttonscan,sizeof(buttonscan));
		read(file,&buttonmouse,sizeof(buttonmouse));
		read(file,&buttonjoy,sizeof(buttonjoy));

		read(file,&viewsize,sizeof(viewsize));
		read(file,&mouseadjustment,sizeof(mouseadjustment));

		close(file);

		if (sd == sdm_AdLib && !AdLibPresent && !SoundBlasterPresent)
		{
			sd = sdm_PC;
			sd = smm_Off;
		}

		if ((sds == sds_SoundBlaster && !SoundBlasterPresent) ||
			(sds == sds_SoundSource && !SoundSourcePresent))
			sds = sds_Off;

		if (!MousePresent)
			mouseenabled = false;
		if (!JoysPresent[joystickport])
			joystickenabled = false;

		MainMenu[6].active=1;
		MainItems.curpos=0;
	}
	else
	{
	//
	// no config file, so select by hardware
	//
		if (SoundBlasterPresent || AdLibPresent)
		{
			sd = sdm_AdLib;
			sm = smm_AdLib;
		}
		else
		{
			sd = sdm_PC;
			sm = smm_Off;
		}

		if (SoundBlasterPresent)
			sds = sds_SoundBlaster;
		else if (SoundSourcePresent)
			sds = sds_SoundSource;
		else
			sds = sds_Off;

		if (MousePresent)
			mouseenabled = true;

		joystickenabled = false;
		joypadenabled = false;
		joystickport = 0;

		viewsize = 15;
		mouseadjustment=5;
	}

	SD_SetMusicMode (sm);
	SD_SetSoundMode (sd);
	SD_SetDigiDevice (sds);

}


/*
====================
=
= WriteConfig
=
====================
*/

void WriteConfig(void)
{
	int                     file;

	file = open(configname,O_CREAT | O_BINARY | O_WRONLY,
				S_IREAD | S_IWRITE | S_IFREG);

	if (file != -1)
	{
		write(file,Scores,sizeof(HighScore) * MaxScores);

		write(file,&SoundMode,sizeof(SoundMode));
		write(file,&MusicMode,sizeof(MusicMode));
		write(file,&DigiMode,sizeof(DigiMode));

		write(file,&mouseenabled,sizeof(mouseenabled));
		write(file,&joystickenabled,sizeof(joystickenabled));
		write(file,&joypadenabled,sizeof(joypadenabled));
		write(file,&joystickport,sizeof(joystickport));

		write(file,&dirscan,sizeof(dirscan));
		write(file,&buttonscan,sizeof(buttonscan));
		write(file,&buttonmouse,sizeof(buttonmouse));
		write(file,&buttonjoy,sizeof(buttonjoy));

		write(file,&viewsize,sizeof(viewsize));
		write(file,&mouseadjustment,sizeof(mouseadjustment));

		close(file);
	}
}


//===========================================================================

/*
=====================
=
= NewGame
=
= Set up new game to start from the beginning
=
=====================
*/

void NewGame (int difficulty,int episode)
{
	memset (&gamestate,0,sizeof(gamestate));
	gamestate.difficulty = difficulty;
	gamestate.weapon = gamestate.bestweapon
		= gamestate.chosenweapon = wp_pistol;
	gamestate.health = 100;
	gamestate.ammo = STARTAMMO;
	gamestate.lives = 3;
	gamestate.nextextra = EXTRAPOINTS;
	gamestate.episode=episode;

	startgame = true;
}

//===========================================================================

void DiskFlopAnim(int x,int y)
{
 static char which=0;
 if (!x && !y)
   return;
 VWB_DrawPic(x,y,C_DISKLOADING1PIC+which);
 VW_UpdateScreen();
 which^=1;
}


long DoChecksum(byte *source,unsigned size,long checksum)
{
 unsigned i;

 for (i=0;i<size-1;i++)
   checksum += source[i]^source[i+1];

 return checksum;
}


/*
==================
=
= SaveTheGame
=
==================
*/

boolean SaveTheGame(int file,int x,int y)
{
	long avail,size,checksum;
	objtype *ob,nullobj;

	checksum = 0;

	DiskFlopAnim(x,y);
	CA_FarWrite (file,(void *)&gamestate,sizeof(gamestate));
	checksum = DoChecksum((byte *)&gamestate,sizeof(gamestate),checksum);

	DiskFlopAnim(x,y);
#ifdef SPEAR
	CA_FarWrite (file,(void *)&LevelRatios[0],sizeof(LRstruct)*20);
	checksum = DoChecksum((byte *)&LevelRatios[0],sizeof(LRstruct)*20,checksum);
#else
	CA_FarWrite (file,(void *)&LevelRatios[0],sizeof(LRstruct)*8);
	checksum = DoChecksum((byte *)&LevelRatios[0],sizeof(LRstruct)*8,checksum);
#endif

	DiskFlopAnim(x,y);
	CA_FarWrite (file,(void *)tilemap,sizeof(tilemap));
	checksum = DoChecksum((byte *)tilemap,sizeof(tilemap),checksum);
	DiskFlopAnim(x,y);
	CA_FarWrite (file,(void *)actorat,sizeof(actorat));
	checksum = DoChecksum((byte *)actorat,sizeof(actorat),checksum);

	CA_FarWrite (file,(void *)areaconnect,sizeof(areaconnect));
	CA_FarWrite (file,(void *)areabyplayer,sizeof(areabyplayer));

	for (ob = player ; ob ; ob=ob->next)
	{
	 DiskFlopAnim(x,y);
	 CA_FarWrite (file,(void *)ob,sizeof(*ob));
	}
	nullobj.active = ac_badobject;          // end of file marker
	DiskFlopAnim(x,y);
	CA_FarWrite (file,(void *)&nullobj,sizeof(nullobj));



	DiskFlopAnim(x,y);
	CA_FarWrite (file,(void *)&laststatobj,sizeof(laststatobj));
	checksum = DoChecksum((byte *)&laststatobj,sizeof(laststatobj),checksum);
	DiskFlopAnim(x,y);
	CA_FarWrite (file,(void *)statobjlist,sizeof(statobjlist));
	checksum = DoChecksum((byte *)statobjlist,sizeof(statobjlist),checksum);

	DiskFlopAnim(x,y);
	CA_FarWrite (file,(void *)doorposition,sizeof(doorposition));
	checksum = DoChecksum((byte *)doorposition,sizeof(doorposition),checksum);
	DiskFlopAnim(x,y);
	CA_FarWrite (file,(void *)doorobjlist,sizeof(doorobjlist));
	checksum = DoChecksum((byte *)doorobjlist,sizeof(doorobjlist),checksum);

	DiskFlopAnim(x,y);
	CA_FarWrite (file,(void *)&pwallstate,sizeof(pwallstate));
	checksum = DoChecksum((byte *)&pwallstate,sizeof(pwallstate),checksum);
	CA_FarWrite (file,(void *)&pwallx,sizeof(pwallx));
	checksum = DoChecksum((byte *)&pwallx,sizeof(pwallx),checksum);
	CA_FarWrite (file,(void *)&pwally,sizeof(pwally));
	checksum = DoChecksum((byte *)&pwally,sizeof(pwally),checksum);
	CA_FarWrite (file,(void *)&pwalldir,sizeof(pwalldir));
	checksum = DoChecksum((byte *)&pwalldir,sizeof(pwalldir),checksum);
	CA_FarWrite (file,(void *)&pwallpos,sizeof(pwallpos));
	checksum = DoChecksum((byte *)&pwallpos,sizeof(pwallpos),checksum);

	//
	// WRITE OUT CHECKSUM
	//
	CA_FarWrite (file,(void *)&checksum,sizeof(checksum));

	return(true);
}

//===========================================================================

/*
==================
=
= LoadTheGame
=
==================
*/

boolean LoadTheGame(int file,int x,int y)
{
	long checksum,oldchecksum;
	objtype *ob,nullobj;


	checksum = 0;

	DiskFlopAnim(x,y);
	CA_FarRead (file,(void *)&gamestate,sizeof(gamestate));
	checksum = DoChecksum((byte *)&gamestate,sizeof(gamestate),checksum);

	DiskFlopAnim(x,y);
#ifdef SPEAR
	CA_FarRead (file,(void *)&LevelRatios[0],sizeof(LRstruct)*20);
	checksum = DoChecksum((byte *)&LevelRatios[0],sizeof(LRstruct)*20,checksum);
#else
	CA_FarRead (file,(void *)&LevelRatios[0],sizeof(LRstruct)*8);
	checksum = DoChecksum((byte *)&LevelRatios[0],sizeof(LRstruct)*8,checksum);
#endif

	DiskFlopAnim(x,y);
	SetupGameLevel ();

	DiskFlopAnim(x,y);
	CA_FarRead (file,(void *)tilemap,sizeof(tilemap));
	checksum = DoChecksum((byte *)tilemap,sizeof(tilemap),checksum);
	DiskFlopAnim(x,y);
	CA_FarRead (file,(void *)actorat,sizeof(actorat));
	checksum = DoChecksum((byte *)actorat,sizeof(actorat),checksum);

	CA_FarRead (file,(void *)areaconnect,sizeof(areaconnect));
	CA_FarRead (file,(void *)areabyplayer,sizeof(areabyplayer));



	InitActorList ();
	DiskFlopAnim(x,y);
	CA_FarRead (file,(void *)player,sizeof(*player));

	while (1)
	{
	 DiskFlopAnim(x,y);
		CA_FarRead (file,(void *)&nullobj,sizeof(nullobj));
		if (nullobj.active == ac_badobject)
			break;
		GetNewActor ();
	 // don't copy over the links
		memcpy (new,&nullobj,sizeof(nullobj)-4);
	}



	DiskFlopAnim(x,y);
	CA_FarRead (file,(void *)&laststatobj,sizeof(laststatobj));
	checksum = DoChecksum((byte *)&laststatobj,sizeof(laststatobj),checksum);
	DiskFlopAnim(x,y);
	CA_FarRead (file,(void *)statobjlist,sizeof(statobjlist));
	checksum = DoChecksum((byte *)statobjlist,sizeof(statobjlist),checksum);

	DiskFlopAnim(x,y);
	CA_FarRead (file,(void *)doorposition,sizeof(doorposition));
	checksum = DoChecksum((byte *)doorposition,sizeof(doorposition),checksum);
	DiskFlopAnim(x,y);
	CA_FarRead (file,(void *)doorobjlist,sizeof(doorobjlist));
	checksum = DoChecksum((byte *)doorobjlist,sizeof(doorobjlist),checksum);

	DiskFlopAnim(x,y);
	CA_FarRead (file,(void *)&pwallstate,sizeof(pwallstate));
	checksum = DoChecksum((byte *)&pwallstate,sizeof(pwallstate),checksum);
	CA_FarRead (file,(void *)&pwallx,sizeof(pwallx));
	checksum = DoChecksum((byte *)&pwallx,sizeof(pwallx),checksum);
	CA_FarRead (file,(void *)&pwally,sizeof(pwally));
	checksum = DoChecksum((byte *)&pwally,sizeof(pwally),checksum);
	CA_FarRead (file,(void *)&pwalldir,sizeof(pwalldir));
	checksum = DoChecksum((byte *)&pwalldir,sizeof(pwalldir),checksum);
	CA_FarRead (file,(void *)&pwallpos,sizeof(pwallpos));
	checksum = DoChecksum((byte *)&pwallpos,sizeof(pwallpos),checksum);

	CA_FarRead (file,(void *)&oldchecksum,sizeof(oldchecksum));

	if (oldchecksum != checksum)
	{
	 Message(STR_SAVECHT1"\n"
			 STR_SAVECHT2"\n"
			 STR_SAVECHT3"\n"
			 STR_SAVECHT4);

	 IN_ClearKeysDown();
	 IN_Ack();

	 gamestate.score = 0;
	 gamestate.lives = 1;
	 gamestate.weapon =
	   gamestate.chosenweapon =
	   gamestate.bestweapon = wp_pistol;
	 gamestate.ammo = 8;
	}

	return true;
}

//===========================================================================

/*
==========================
=
= ShutdownId
=
= Shuts down all ID_?? managers
=
==========================
*/

void ShutdownId (void)
{
	US_Shutdown ();
	SD_Shutdown ();
	PM_Shutdown ();
	IN_Shutdown ();
	VW_Shutdown ();
	CA_Shutdown ();
	MM_Shutdown ();
}


//===========================================================================

/*
==================
=
= BuildTables
=
= Calculates:
=
= scale                 projection constant
= sintable/costable     overlapping fractional tables
=
==================
*/

const float radtoint = (float)FINEANGLES/2.0f/PI;

void BuildTables (void)
{
  int           i;
  float         angle,anglestep;
  double        tang;
  fixed         value;


//
// calculate fine tangents
//

	for (i=0;i<FINEANGLES/8;i++)
	{
		tang = tan( (i+0.5)/radtoint);
		finetangent[i] = tang*TILEGLOBAL;
		finetangent[FINEANGLES/4-1-i] = 1/tang*TILEGLOBAL;
	}

//
// costable overlays sintable with a quarter phase shift
// ANGLES is assumed to be divisable by four
//
// The low word of the value is the fraction, the high bit is the sign bit,
// bits 16-30 should be 0
//

  angle = 0;
  anglestep = PI/2/ANGLEQUAD;
  for (i=0;i<=ANGLEQUAD;i++)
  {
	value=GLOBAL1*sin(angle);
	sintable[i]=
	  sintable[i+ANGLES]=
	  sintable[ANGLES/2-i] = value;
	sintable[ANGLES-i]=
	  sintable[ANGLES/2+i] = value | 0x80000000l;
	angle += anglestep;
  }

}

//===========================================================================


/*
====================
=
= CalcProjection
=
= Uses focallength
=
====================
*/

void CalcProjection (long focal)
{
	int             i;
	long            intang;
	float   angle;
	double  tang;
	double  planedist;
	double  globinhalf;
	int             halfview;
	double  halfangle,facedist;


	focallength = focal;
	facedist = focal+MINDIST;
	halfview = viewwidth/2;                                 // half view in pixels

//
// calculate scale value for vertical height calculations
// and sprite x calculations
//
	scale = halfview*facedist/(VIEWGLOBAL/2);

//
// divide heightnumerator by a posts distance to get the posts height for
// the heightbuffer.  The pixel height is height>>2
//
	heightnumerator = (TILEGLOBAL*scale)>>6;
	minheightdiv = heightnumerator/0x7fff +1;

//
// calculate the angle offset from view angle of each pixel's ray
//

	for (i=0;i<halfview;i++)
	{
	// start 1/2 pixel over, so viewangle bisects two middle pixels
		tang = (long)i*VIEWGLOBAL/viewwidth/facedist;
		angle = atan(tang);
		intang = angle*radtoint;
		pixelangle[halfview-1-i] = intang;
		pixelangle[halfview+i] = -intang;
	}

//
// if a point's abs(y/x) is greater than maxslope, the point is outside
// the view area
//
	maxslope = finetangent[pixelangle[0]];
	maxslope >>= 8;
}



//===========================================================================

/*
===================
=
= SetupWalls
=
= Map tile values to scaled pics
=
===================
*/

void SetupWalls (void)
{
	int     i;

	for (i=1;i<MAXWALLTILES;i++)
	{
		horizwall[i]=(i-1)*2;
		vertwall[i]=(i-1)*2+1;
	}
}

//===========================================================================

/*
==========================
=
= SignonScreen
=
==========================
*/

void SignonScreen (void)                        // VGA version
{
	unsigned        segstart,seglength;

	VL_SetPalette (&gamepal);

	if (!virtualreality)
	{
		VL_MemToScreen (&introscn,320,200,0,0);
	}
}


/*
==========================
=
= FinishSignon
=
==========================
*/

void FinishSignon (void)
{

#ifndef SPEAR
	VW_Bar (0,189,300,11, *gfxbuf);
	WindowX = 0;
	WindowW = 320;
	PrintY = 190;

	#ifndef JAPAN
	SETFONTCOLOR(14,4);

	#ifdef SPANISH
	US_CPrint ("Oprima una tecla");
	#else
	US_CPrint ("Press a key");
	#endif

	#endif

	if (!NoWait)
		IN_Ack ();

	#ifndef JAPAN
	VW_Bar (0,189,300,11,*gfxbuf);

	PrintY = 190;
	SETFONTCOLOR(10,4);

	#ifdef SPANISH
	US_CPrint ("pensando...");
	#else
	US_CPrint ("Working...");
	#endif

	#endif

	SETFONTCOLOR(0,15);
#else
	if (!NoWait)
		VW_WaitVBL(3*70);
#endif
}

//===========================================================================

/*
=================
=
= MS_CheckParm
=
=================
*/

boolean MS_CheckParm(char *check)
{
	int             i;
	char    *parm;

	for (i = 1;i<_argc;i++)
	{
		parm = _argv[i];

		while ( !isalpha(*parm) )       // skip - / \ etc.. in front of parm
			if (!*parm++)
				break;                          // hit end of string without an alphanum

		if ( !strcasecmp(check,parm) )
			return true;
	}

	return false;
}

//===========================================================================

/*
=====================
=
= InitDigiMap
=
=====================
*/

static  int     wolfdigimap[] =
		{
			// These first sounds are in the upload version
#ifndef SPEAR
			HALTSND,                0,
			DOGBARKSND,             1,
			CLOSEDOORSND,           2,
			OPENDOORSND,            3,
			ATKMACHINEGUNSND,       4,
			ATKPISTOLSND,           5,
			ATKGATLINGSND,          6,
			SCHUTZADSND,            7,
			GUTENTAGSND,            8,
			MUTTISND,               9,
			BOSSFIRESND,            10,
			SSFIRESND,              11,
			DEATHSCREAM1SND,        12,
			DEATHSCREAM2SND,        13,
			DEATHSCREAM3SND,        13,
			TAKEDAMAGESND,          14,
			PUSHWALLSND,            15,

			LEBENSND,               20,
			NAZIFIRESND,            21,
			SLURPIESND,             22,

			YEAHSND,				32,

#ifndef UPLOAD
			// These are in all other episodes
			DOGDEATHSND,            16,
			AHHHGSND,               17,
			DIESND,                 18,
			EVASND,                 19,

			TOT_HUNDSND,            23,
			MEINGOTTSND,            24,
			SCHABBSHASND,           25,
			HITLERHASND,            26,
			SPIONSND,               27,
			NEINSOVASSND,           28,
			DOGATTACKSND,           29,
			LEVELDONESND,           30,
			MECHSTEPSND,			31,

			SCHEISTSND,				33,
			DEATHSCREAM4SND,		34,		// AIIEEE
			DEATHSCREAM5SND,		35,		// DEE-DEE
			DONNERSND,				36,		// EPISODE 4 BOSS DIE
			EINESND,				37,		// EPISODE 4 BOSS SIGHTING
			ERLAUBENSND,			38,		// EPISODE 6 BOSS SIGHTING
			DEATHSCREAM6SND,		39,		// FART
			DEATHSCREAM7SND,		40,		// GASP
			DEATHSCREAM8SND,		41,		// GUH-BOY!
			DEATHSCREAM9SND,		42,		// AH GEEZ!
			KEINSND,				43,		// EPISODE 5 BOSS SIGHTING
			MEINSND,				44,		// EPISODE 6 BOSS DIE
			ROSESND,				45,		// EPISODE 5 BOSS DIE

#endif
#else
//
// SPEAR OF DESTINY DIGISOUNDS
//
			HALTSND,                0,
			CLOSEDOORSND,           2,
			OPENDOORSND,            3,
			ATKMACHINEGUNSND,       4,
			ATKPISTOLSND,           5,
			ATKGATLINGSND,          6,
			SCHUTZADSND,            7,
			BOSSFIRESND,            8,
			SSFIRESND,              9,
			DEATHSCREAM1SND,        10,
			DEATHSCREAM2SND,        11,
			TAKEDAMAGESND,          12,
			PUSHWALLSND,            13,
			AHHHGSND,               15,
			LEBENSND,               16,
			NAZIFIRESND,            17,
			SLURPIESND,             18,
			LEVELDONESND,           22,
			DEATHSCREAM4SND,		23,		// AIIEEE
			DEATHSCREAM3SND,        23,		// DOUBLY-MAPPED!!!
			DEATHSCREAM5SND,		24,		// DEE-DEE
			DEATHSCREAM6SND,		25,		// FART
			DEATHSCREAM7SND,		26,		// GASP
			DEATHSCREAM8SND,		27,		// GUH-BOY!
			DEATHSCREAM9SND,		28,		// AH GEEZ!
			GETGATLINGSND,			38,		// Got Gat replacement

#ifndef SPEARDEMO
			DOGBARKSND,             1,
			DOGDEATHSND,            14,
			SPIONSND,               19,
			NEINSOVASSND,           20,
			DOGATTACKSND,           21,
			TRANSSIGHTSND,			29,		// Trans Sight
			TRANSDEATHSND,			30,		// Trans Death
			WILHELMSIGHTSND,		31,		// Wilhelm Sight
			WILHELMDEATHSND,		32,		// Wilhelm Death
			UBERDEATHSND,			33,		// Uber Death
			KNIGHTSIGHTSND,			34,		// Death Knight Sight
			KNIGHTDEATHSND,			35,		// Death Knight Death
			ANGELSIGHTSND,			36,		// Angel Sight
			ANGELDEATHSND,			37,		// Angel Death
			GETSPEARSND,			39,		// Got Spear replacement
#endif
#endif
			LASTSOUND
		};


void InitDigiMap (void)
{
	int                     *map;

	for (map = wolfdigimap;*map != LASTSOUND;map += 2)
		DigiMap[map[0]] = map[1];


}

#ifndef SPEAR
CP_iteminfo	MusicItems={CTL_X,CTL_Y,6,0,32};
CP_itemtype MusicMenu[]=
	{
		{1,"Get Them!",0},
		{1,"Searching",0},
		{1,"P.O.W.",0},
		{1,"Suspense",0},
		{1,"War March",0},
		{1,"Around The Corner!",0},

		{1,"Nazi Anthem",0},
		{1,"Lurking...",0},
		{1,"Going After Hitler",0},
		{1,"Pounding Headache",0},
		{1,"Into the Dungeons",0},
		{1,"Ultimate Conquest",0},

		{1,"Kill the S.O.B.",0},
		{1,"The Nazi Rap",0},
		{1,"Twelfth Hour",0},
		{1,"Zero Hour",0},
		{1,"Ultimate Conquest",0},
		{1,"Wolfpack",0}
	};
#else
CP_iteminfo MusicItems={CTL_X,CTL_Y-20,9,0,32};
CP_itemtype MusicMenu[]=
   {
		{1,"Funky Colonel Bill",0},
		{1,"Death To The Nazis",0},
		{1,"Tiptoeing Around",0},
		{1,"Is This THE END?",0},
		{1,"Evil Incarnate",0},
		{1,"Jazzin' Them Nazis",0},
		{1,"Puttin' It To The Enemy",0},
		{1,"The SS Gonna Get You",0},
		{1,"Towering Above",0}
	};
#endif

#ifndef SPEARDEMO
void DoJukebox(void)
{
	int which,lastsong=-1;
	unsigned start,songs[]=
		{
#ifndef SPEAR
			GETTHEM_MUS,
			SEARCHN_MUS,
			POW_MUS,
			SUSPENSE_MUS,
			WARMARCH_MUS,
			CORNER_MUS,

			NAZI_OMI_MUS,
			PREGNANT_MUS,
			GOINGAFT_MUS,
			HEADACHE_MUS,
			DUNGEON_MUS,
			ULTIMATE_MUS,

			INTROCW3_MUS,
			NAZI_RAP_MUS,
			TWELFTH_MUS,
			ZEROHOUR_MUS,
			ULTIMATE_MUS,
			PACMAN_MUS
#else
			XFUNKIE_MUS,             // 0
			XDEATH_MUS,              // 2
			XTIPTOE_MUS,             // 4
			XTHEEND_MUS,             // 7
			XEVIL_MUS,               // 17
			XJAZNAZI_MUS,            // 18
			XPUTIT_MUS,              // 21
			XGETYOU_MUS,             // 22
			XTOWER2_MUS              // 23
#endif
		};

	IN_ClearKeysDown();
	if (!AdLibPresent && !SoundBlasterPresent)
		return;


	MenuFadeOut();

#ifndef SPEAR
#ifndef UPLOAD
	start = (rand()%3)*6;
#else
	start = 0;
#endif
#else
	start = 0;
#endif


	CA_CacheGrChunk (STARTFONT+1);
#ifdef SPEAR
	CacheLump (BACKDROP_LUMP_START,BACKDROP_LUMP_END);
#else
	CacheLump (CONTROLS_LUMP_START,CONTROLS_LUMP_END);
#endif
	CA_LoadAllSounds ();

	fontnumber=1;
	ClearMScreen ();
	VWB_DrawPic(112,184,C_MOUSELBACKPIC);
	DrawStripes (10);
	SETFONTCOLOR (TEXTCOLOR,BKGDCOLOR);

#ifndef SPEAR
	DrawWindow (CTL_X-2,CTL_Y-6,280,13*7,BKGDCOLOR);
#else
	DrawWindow (CTL_X-2,CTL_Y-26,280,13*10,BKGDCOLOR);
#endif

	DrawMenu (&MusicItems,&MusicMenu[start]);

	SETFONTCOLOR (READHCOLOR,BKGDCOLOR);
	PrintY=15;
	WindowX = 0;
	WindowY = 320;
	US_CPrint ("Robert's Jukebox");

	SETFONTCOLOR (TEXTCOLOR,BKGDCOLOR);
	VW_UpdateScreen();
	MenuFadeIn();

	do
	{
		which = HandleMenu(&MusicItems,&MusicMenu[start],NULL);
		if (which>=0)
		{
			if (lastsong >= 0)
				MusicMenu[start+lastsong].active = 1;

			StartCPMusic(songs[start + which]);
			MusicMenu[start+which].active = 2;
			DrawMenu (&MusicItems,&MusicMenu[start]);
			VW_UpdateScreen();
			lastsong = which;
		}
	} while(which>=0);

	MenuFadeOut();
	IN_ClearKeysDown();
#ifdef SPEAR
	UnCacheLump (BACKDROP_LUMP_START,BACKDROP_LUMP_END);
#else
	UnCacheLump (CONTROLS_LUMP_START,CONTROLS_LUMP_END);
#endif
}
#endif


/*
==========================
=
= InitGame
=
= Load a few things right away
=
==========================
*/

void InitGame (void)
{
	int                     i,x,y;

	if (MS_CheckParm ("virtual"))
		virtualreality = true;
	else
		virtualreality = false;

	MM_Startup (); 
	VW_Startup ();
	IN_Startup ();
	PM_Startup ();
	SD_Startup ();
	CA_Startup ();
	US_Startup ();
	
	SignonScreen ();
	
//
// build some tables
//
	InitDigiMap ();

	for (i=0;i<MAPSIZE;i++)
	{
		farmapylookup[i] = i*64;
	}

	ReadConfig ();


//
// HOLDING DOWN 'M' KEY?
//
#ifndef SPEARDEMO
	if (Keyboard[sc_M])
	  DoJukebox();
	else
#endif
//
// draw intro screen stuff
//
	if (!virtualreality)
		IntroScreen ();

//
// load in and lock down some basic chunks
//

	CA_CacheGrChunk(STARTFONT);
	MM_SetLock (&grsegs[STARTFONT],true);

	LoadLatchMem ();
	BuildTables ();          // trig tables
	SetupWalls ();

	NewViewSize(viewsize);


//
// initialize variables
//
	InitRedShifts ();
	if (!virtualreality)
		FinishSignon();

	if (virtualreality)
	{
		NoWait = true;
	}
}

//===========================================================================

/*
==========================
=
= SetViewSize
=
==========================
*/

boolean SetViewSize (unsigned width, unsigned height)
{
	viewwidth = width&~15;                  // must be divisable by 16
	viewheight = height&~1;                 // must be even
	centerx = viewwidth/2-1;
	shootdelta = viewwidth/10;
	
	yoffset =  (200-STATUSLINES-viewheight)/2;
	xoffset = (320-viewwidth)/2;
	
//
// calculate trace angles and projection constants
//
	CalcProjection (FOCALLENGTH);

//
// build all needed compiled scalers
//
//	MM_BombOnError (false);
	SetupScaling (viewwidth*1.5);
#if 0
	MM_BombOnError (true);
	if (mmerror)
	{
		Quit ("Can't build scalers!");
		mmerror = false;
		return false;
	}
#endif
	return true;
}


void ShowViewSize (int width)
{
	int     oldwidth,oldheight;

	oldwidth = viewwidth;
	oldheight = viewheight;

	viewwidth = width*16;
	viewheight = width*16*HEIGHTRATIO;
	DrawPlayBorder ();

	viewheight = oldheight;
	viewwidth = oldwidth;
}


void NewViewSize(int width)
{
	CA_UpLevel ();
	MM_SortMem ();
	viewsize = width;
	SetViewSize (width*16,width*16*HEIGHTRATIO);
	CA_DownLevel ();
}



//===========================================================================

/*
==========================
=
= Quit
=
==========================
*/

void Quit (char *error)
{
	memptr screen;

	if (!*error)
	{
	 #ifndef JAPAN
	 CA_CacheGrChunk(ORDERSCREEN);
	 screen = grsegs[ORDERSCREEN];
	 #endif
	 WriteConfig ();
	}
	else
	{
	 CA_CacheGrChunk(ERRORSCREEN);
	 screen = grsegs[ERRORSCREEN];
	}
	
	ShutdownId ();
	
	if (screen) {
		/* blah blah */
	}
	
	if (error && *error)
		printf("Quit: %s\n", error);
	else
		printf("Quit: Nothing\n");
		
#if 0
	if (error && *error)
	{
	  movedata ((unsigned)screen,7,0xb800,0,7*160);
	  gotoxy (10,4);
	  puts(error);
	  gotoxy (1,8);
	  exit(1);
	}
	else
	if (!error || !(*error))
	{
		clrscr();
		#ifndef JAPAN
		movedata ((unsigned)screen,7,0xb800,0,4000);
		gotoxy(1,24);
		#endif
	}
#endif

	exit(0);
}

//===========================================================================



/*
=====================
=
= DemoLoop
=
=====================
*/

void DemoLoop (void)
{
	static int LastDemo;
//
// main game cycle
//


	#ifndef UPLOAD

		#ifndef GOODTIMES
		#ifndef SPEAR
		#ifndef JAPAN
		if (!NoWait)
			NonShareware();
		#endif
		#else

			#ifndef GOODTIMES
			#ifndef SPEARDEMO
			CopyProtection();
			#endif
			#endif

		#endif
		#endif
	#endif

	StartCPMusic(INTROSONG);

#ifndef JAPAN
	if (!NoWait)
		PG13 ();
#endif

	while (1)
	{
		while (!NoWait)
		{
//
// title page
//
			MM_SortMem ();
#ifdef SPEAR
			CA_CacheGrChunk (TITLEPALETTE);

			CA_CacheGrChunk (TITLE1PIC);
			VWB_DrawPic (0,0,TITLE1PIC);
			UNCACHEGRCHUNK (TITLE1PIC);

			CA_CacheGrChunk (TITLE2PIC);
			VWB_DrawPic (0,80,TITLE2PIC);
			UNCACHEGRCHUNK (TITLE2PIC);
			VW_UpdateScreen ();
			VL_FadeIn(0,255,grsegs[TITLEPALETTE],30);

			UNCACHEGRCHUNK (TITLEPALETTE);
#else
			CA_CacheScreen (TITLEPIC);
			VW_UpdateScreen ();
			VW_FadeIn();
#endif
			if (IN_UserInput(TickBase*15))
				break;
			VW_FadeOut();
//
// credits page
//
			CA_CacheScreen (CREDITSPIC);
			VW_UpdateScreen();
			VW_FadeIn ();
			if (IN_UserInput(TickBase*10))
				break;
			VW_FadeOut ();
//
// high scores
//
			DrawHighScores ();
			VW_UpdateScreen ();
			VW_FadeIn ();

			if (IN_UserInput(TickBase*10))
				break;
//
// demo
//

			#ifndef SPEARDEMO
			PlayDemo (LastDemo++%4);
			#else
			PlayDemo (0);
			#endif

			if (playstate == ex_abort)
				break;
			StartCPMusic(INTROSONG);
		}

		VW_FadeOut ();

#ifndef SPEAR
		if (Keyboard[sc_Tab] && MS_CheckParm("goobers"))
#else
		if (Keyboard[sc_Tab] && MS_CheckParm("debugmode"))
#endif
			RecordDemo ();
		else
			US_ControlPanel (0);

		if (startgame || loadedgame)
		{
			GameLoop ();
			VW_FadeOut();
			StartCPMusic(INTROSONG);
		}
	}
}


//===========================================================================


/*
==========================
=
= main
=
==========================
*/

int main(int argc, char *argv[])
{
	int i;

	_argc = argc;
	_argv = argv;
	
	CheckForEpisodes();

	InitGame();

	DemoLoop();

	Quit("Demo loop exited???");
	
	return 0;
}

