/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_bloom.c: 2D lighting post process effect


//http://www.quakesrc.org/forums/viewtopic.php?t=4340&start=0

#include "quakedef.h"

#ifdef RGLQUAKE
#include "glquake.h"

extern vrect_t         scr_vrect;

/* 
============================================================================== 
 
						LIGHT BLOOMS
 
============================================================================== 
*/ 

static float Diamond8x[8][8] = { 
	{0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f}, 
	{0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f}, 
	{0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f}, 
	{0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f}, 
	{0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f}, 
	{0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f}, 
	{0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f}, 
	{0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f} };

static float Diamond6x[6][6] = { 
	{0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f}, 
	{0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f},  
	{0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f}, 
	{0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f}, 
	{0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f}, 
	{0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f} };

static float Diamond4x[4][4] = {  
	{0.3f, 0.4f, 0.4f, 0.3f},  
	{0.4f, 0.9f, 0.9f, 0.4f}, 
	{0.4f, 0.9f, 0.9f, 0.4f}, 
	{0.3f, 0.4f, 0.4f, 0.3f} };


static int		BLOOM_SIZE;

cvar_t		r_bloom = FCVAR("r_bloom", "gl_bloom", "0", CVAR_ARCHIVE);
cvar_t		r_bloom_alpha = SCVAR("r_bloom_alpha", "0.5");
cvar_t		r_bloom_diamond_size = SCVAR("r_bloom_diamond_size", "8");
cvar_t		r_bloom_intensity = SCVAR("r_bloom_intensity", "1");
cvar_t		r_bloom_darken = SCVAR("r_bloom_darken", "3");
cvar_t		r_bloom_sample_size = SCVARF("r_bloom_sample_size", "256", CVAR_RENDERERLATCH);
cvar_t		r_bloom_fast_sample = SCVARF("r_bloom_fast_sample", "0", CVAR_RENDERERLATCH);

int	r_bloomscreentexture;
int r_bloomeffecttexture;
int r_bloombackuptexture;
int r_bloomdownsamplingtexture;

static int		r_screendownsamplingtexture_size;
static int		screen_texture_width, screen_texture_height;
static int		r_screenbackuptexture_size;

//current refdef size:
static int	curView_x;
static int	curView_y;
static int	curView_width;
static int	curView_height;

//texture coordinates of screen data inside screentexture
static float screenText_tcw;
static float screenText_tch;

static int	sample_width;
static int	sample_height;

//texture coordinates of adjusted textures
static float sampleText_tcw;
static float sampleText_tch;

//this macro is in sample size workspace coordinates
#define R_Bloom_SamplePass( xpos, ypos )							\
	qglBegin(GL_QUADS);												\
	qglTexCoord2f(	0,						sampleText_tch);		\
	qglVertex2f(	xpos,					ypos);					\
	qglTexCoord2f(	0,						0);						\
	qglVertex2f(	xpos,					ypos+sample_height);	\
	qglTexCoord2f(	sampleText_tcw,			0);						\
	qglVertex2f(	xpos+sample_width,		ypos+sample_height);	\
	qglTexCoord2f(	sampleText_tcw,			sampleText_tch);		\
	qglVertex2f(	xpos+sample_width,		ypos);					\
	qglEnd();

#define R_Bloom_Quad( x, y, width, height, textwidth, textheight )	\
	qglBegin(GL_QUADS);												\
	qglTexCoord2f(	0,			textheight);						\
	qglVertex2f(	x,			y);									\
	qglTexCoord2f(	0,			0);									\
	qglVertex2f(	x,			y+height);							\
	qglTexCoord2f(	textwidth,	0);									\
	qglVertex2f(	x+width,	y+height);							\
	qglTexCoord2f(	textwidth,	textheight);						\
	qglVertex2f(	x+width,	y);									\
	qglEnd();



/*
=================
R_Bloom_InitBackUpTexture
=================
*/
void R_Bloom_InitBackUpTexture( int width, int height )
{
	qbyte	*data;
	
	data = Z_Malloc( width * height * 4 );

	r_screenbackuptexture_size = width;

	r_bloombackuptexture = GL_LoadTexture32("***r_bloombackuptexture***", width, height, (unsigned int*)data, false, false );
	
	Z_Free ( data );
}

/*
=================
R_Bloom_InitEffectTexture
=================
*/
void R_Bloom_InitEffectTexture( void )
{
	qbyte	*data;
	float	bloomsizecheck;
	
	if( r_bloom_sample_size.value < 32 )
		Cvar_SetValue (&r_bloom_sample_size, 32);

	//make sure bloom size is a power of 2
	BLOOM_SIZE = r_bloom_sample_size.value;
	bloomsizecheck = (float)BLOOM_SIZE;
	while(bloomsizecheck > 1.0f) bloomsizecheck /= 2.0f;
	if( bloomsizecheck != 1.0f )
	{
		BLOOM_SIZE = 32;
		while( BLOOM_SIZE < r_bloom_sample_size.value )
			BLOOM_SIZE *= 2;
	}

	//make sure bloom size doesn't have stupid values
	if( BLOOM_SIZE > screen_texture_width ||
		BLOOM_SIZE > screen_texture_height )
		BLOOM_SIZE = min( screen_texture_width, screen_texture_height );

	if( BLOOM_SIZE != r_bloom_sample_size.value )
		Cvar_SetValue (&r_bloom_sample_size, BLOOM_SIZE);

	data = Z_Malloc( BLOOM_SIZE * BLOOM_SIZE * 4 );

	r_bloomeffecttexture = GL_LoadTexture32("***r_bloomeffecttexture***", BLOOM_SIZE, BLOOM_SIZE, (unsigned int*)data, false, false );
	
	Z_Free ( data );
}

/*
=================
R_Bloom_InitTextures
=================
*/
void R_Bloom_InitTextures( void )
{
	qbyte	*data;
	int		size;
	int maxtexsize;

	//find closer power of 2 to screen size 
	for (screen_texture_width = 1;screen_texture_width < glwidth;screen_texture_width *= 2);
	for (screen_texture_height = 1;screen_texture_height < glheight;screen_texture_height *= 2);

	//disable blooms if we can't handle a texture of that size
	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsize);
	if( screen_texture_width > maxtexsize ||
		screen_texture_height > maxtexsize ) {
		screen_texture_width = screen_texture_height = 0;
		Cvar_SetValue (&r_bloom, 0);
		Con_Printf( "WARNING: 'R_InitBloomScreenTexture' too high resolution for Light Bloom. Effect disabled\n" );
		return;
	}

	//init the screen texture
	size = screen_texture_width * screen_texture_height * 4;
	data = Z_Malloc( size );
	memset( data, 255, size );
	if (!r_bloomscreentexture)
		r_bloomscreentexture = texture_extension_number++;
	GL_Bind(r_bloomscreentexture);
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, screen_texture_width, screen_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	Z_Free ( data );


	//validate bloom size and init the bloom effect texture
	R_Bloom_InitEffectTexture ();

	//if screensize is more than 2x the bloom effect texture, set up for stepped downsampling
	r_bloomdownsamplingtexture = 0;
	r_screendownsamplingtexture_size = 0;
	if( glwidth > (BLOOM_SIZE * 2) && !r_bloom_fast_sample.value )
	{
		r_screendownsamplingtexture_size = (int)(BLOOM_SIZE * 2);
		data = Z_Malloc( r_screendownsamplingtexture_size * r_screendownsamplingtexture_size * 4 );
		r_bloomdownsamplingtexture = GL_LoadTexture32("***r_bloomdownsamplingtexture***", r_screendownsamplingtexture_size, r_screendownsamplingtexture_size, (unsigned int*)data, false, false );
		Z_Free ( data );
	}

	//Init the screen backup texture
	if( r_screendownsamplingtexture_size )
		R_Bloom_InitBackUpTexture( r_screendownsamplingtexture_size, r_screendownsamplingtexture_size );
	else
		R_Bloom_InitBackUpTexture( BLOOM_SIZE, BLOOM_SIZE );
}

void R_BloomRegister(void)
{
	Cvar_Register (&r_bloom, "bloom");
	Cvar_Register (&r_bloom_alpha, "bloom");
	Cvar_Register (&r_bloom_diamond_size, "bloom");
	Cvar_Register (&r_bloom_intensity, "bloom");
	Cvar_Register (&r_bloom_darken, "bloom");
	Cvar_Register (&r_bloom_sample_size, "bloom");
	Cvar_Register (&r_bloom_fast_sample, "bloom");
}

/*
=================
R_InitBloomTextures
=================
*/
void R_InitBloomTextures( void )
{
	BLOOM_SIZE = 0;
	if( !r_bloom.value )
		return;

	r_bloomscreentexture = 0;	//this came from a vid_restart, where none of the textures are valid any more.
	R_Bloom_InitTextures ();
}


/*
=================
R_Bloom_DrawEffect
=================
*/
void R_Bloom_DrawEffect( void )
{
	GL_Bind(r_bloomeffecttexture);
	qglEnable(GL_BLEND);
	qglBlendFunc(GL_ONE, GL_ONE);
	qglColor4f(r_bloom_alpha.value, r_bloom_alpha.value, r_bloom_alpha.value, 1.0f);
	GL_TexEnv(GL_MODULATE);
	qglBegin(GL_QUADS);							
	qglTexCoord2f(	0,							sampleText_tch	);	
	qglVertex2f(	curView_x,					curView_y	);				
	qglTexCoord2f(	0,							0	);				
	qglVertex2f(	curView_x,					curView_y + curView_height	);	
	qglTexCoord2f(	sampleText_tcw,				0	);				
	qglVertex2f(	curView_x + curView_width,	curView_y + curView_height	);	
	qglTexCoord2f(	sampleText_tcw,				sampleText_tch	);	
	qglVertex2f(	curView_x + curView_width,	curView_y	);				
	qglEnd();
	
	qglDisable(GL_BLEND);
}


#if 0
/*
=================
R_Bloom_GeneratexCross - alternative bluring method
=================
*/
void R_Bloom_GeneratexCross( void )
{
	int			i;
	static int		BLOOM_BLUR_RADIUS = 8;
	//static float	BLOOM_BLUR_INTENSITY = 2.5f;
	float	BLOOM_BLUR_INTENSITY;
	static float intensity;
	static float range;

	//set up sample size workspace
	qglViewport( 0, 0, sample_width, sample_height );
	qglMatrixMode( GL_PROJECTION );
    qglLoadIdentity ();
	qglOrtho(0, sample_width, sample_height, 0, -10, 100);
	qglMatrixMode( GL_MODELVIEW );
    qglLoadIdentity ();

	//copy small scene into r_bloomeffecttexture
	GL_Bind(0, r_bloomeffecttexture);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	//start modifying the small scene corner
	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	qglEnable(GL_BLEND);

	//darkening passes
	if( r_bloom_darken.value )
	{
		qglBlendFunc(GL_DST_COLOR, GL_ZERO);
		GL_TexEnv(GL_MODULATE);
		
		for(i=0; i<r_bloom_darken->integer ;i++) {
			R_Bloom_SamplePass( 0, 0 );
		}
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);
	}

	//bluring passes
	if( BLOOM_BLUR_RADIUS ) {
		
		qglBlendFunc(GL_ONE, GL_ONE);

		range = (float)BLOOM_BLUR_RADIUS;

		BLOOM_BLUR_INTENSITY = r_bloom_intensity.value;
		//diagonal-cross draw 4 passes to add initial smooth
		qglColor4f( 0.5f, 0.5f, 0.5f, 1.0);
		R_Bloom_SamplePass( 1, 1 );
		R_Bloom_SamplePass( -1, 1 );
		R_Bloom_SamplePass( -1, -1 );
		R_Bloom_SamplePass( 1, -1 );
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

		for(i=-(BLOOM_BLUR_RADIUS+1);i<BLOOM_BLUR_RADIUS;i++) {
			intensity = BLOOM_BLUR_INTENSITY/(range*2+1)*(1 - fabs(i*i)/(float)(range*range));
			if( intensity < 0.05f ) continue;
			qglColor4f( intensity, intensity, intensity, 1.0f);
			R_Bloom_SamplePass( i, 0 );
			//R_Bloom_SamplePass( -i, 0 );
		}

		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

		//for(i=0;i<BLOOM_BLUR_RADIUS;i++) {
		for(i=-(BLOOM_BLUR_RADIUS+1);i<BLOOM_BLUR_RADIUS;i++) {
			intensity = BLOOM_BLUR_INTENSITY/(range*2+1)*(1 - fabs(i*i)/(float)(range*range));
			if( intensity < 0.05f ) continue;
			qglColor4f( intensity, intensity, intensity, 1.0f);
			R_Bloom_SamplePass( 0, i );
			//R_Bloom_SamplePass( 0, -i );
		}

		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);
	}
	
	//restore full screen workspace
	qglViewport( 0, 0, glState.width, glState.height );
	qglMatrixMode( GL_PROJECTION );
    qglLoadIdentity ();
	qglOrtho(0, glState.width, glState.height, 0, -10, 100);
	qglMatrixMode( GL_MODELVIEW );
    qglLoadIdentity ();
}
#endif


/*
=================
R_Bloom_GeneratexDiamonds
=================
*/
void R_Bloom_GeneratexDiamonds( void )
{
	int			i, j;
	static float intensity;

	//set up sample size workspace
	qglViewport( 0, 0, sample_width, sample_height );
	qglMatrixMode( GL_PROJECTION );
    qglLoadIdentity ();
	qglOrtho(0, sample_width, sample_height, 0, -10, 100);
	qglMatrixMode( GL_MODELVIEW );
    qglLoadIdentity ();

	//copy small scene into r_bloomeffecttexture
	GL_Bind(r_bloomeffecttexture);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	//start modifying the small scene corner
	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	qglEnable(GL_BLEND);

	//darkening passes
	if( r_bloom_darken.value )
	{
		qglBlendFunc(GL_DST_COLOR, GL_ZERO);
		GL_TexEnv(GL_MODULATE);
		
		for(i=0; i<r_bloom_darken.value ;i++) {
			R_Bloom_SamplePass( 0, 0 );
		}
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);
	}

	//bluring passes
	//qglBlendFunc(GL_ONE, GL_ONE);
	qglBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
	
	if( r_bloom_diamond_size.value > 7 || r_bloom_diamond_size.value <= 3)
	{
		if( r_bloom_diamond_size.value != 8 ) Cvar_SetValue( &r_bloom_diamond_size, 8 );

		for(i=0; i<r_bloom_diamond_size.value; i++) {
			for(j=0; j<r_bloom_diamond_size.value; j++) {
				intensity = r_bloom_intensity.value * 0.3 * Diamond8x[i][j];
				if( intensity < 0.01f ) continue;
				qglColor4f( intensity, intensity, intensity, 1.0);
				R_Bloom_SamplePass( i-4, j-4 );
			}
		}
	} else if( r_bloom_diamond_size.value > 5 ) {
		
		if( r_bloom_diamond_size.value != 6 ) Cvar_SetValue(&r_bloom_diamond_size, 6 );

		for(i=0; i<r_bloom_diamond_size.value; i++) {
			for(j=0; j<r_bloom_diamond_size.value; j++) {
				intensity = r_bloom_intensity.value * 0.5 * Diamond6x[i][j];
				if( intensity < 0.01f ) continue;
				qglColor4f( intensity, intensity, intensity, 1.0);
				R_Bloom_SamplePass( i-3, j-3 );
			}
		}
	} else if( r_bloom_diamond_size.value > 3 ) {

		if( r_bloom_diamond_size.value != 4 ) Cvar_SetValue(&r_bloom_diamond_size, 4 );

		for(i=0; i<r_bloom_diamond_size.value; i++) {
			for(j=0; j<r_bloom_diamond_size.value; j++) {
				intensity = r_bloom_intensity.value * 0.8f * Diamond4x[i][j];
				if( intensity < 0.01f ) continue;
				qglColor4f( intensity, intensity, intensity, 1.0);
				R_Bloom_SamplePass( i-2, j-2 );
			}
		}
	}
	
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	//restore full screen workspace
	qglViewport( 0, 0, glwidth, glheight );
	qglMatrixMode( GL_PROJECTION );
    qglLoadIdentity ();
	qglOrtho(0, glwidth, glheight, 0, -10, 100);
	qglMatrixMode( GL_MODELVIEW );
    qglLoadIdentity ();
}											

/*
=================
R_Bloom_DownsampleView
=================
*/
void R_Bloom_DownsampleView( void )
{
	qglDisable( GL_BLEND );
	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	//stepped downsample
	if( r_screendownsamplingtexture_size )
	{
		int		midsample_width = r_screendownsamplingtexture_size * sampleText_tcw;
		int		midsample_height = r_screendownsamplingtexture_size * sampleText_tch;
		
		if (qglGetError())
			Con_Printf("GL Error whilst rendering bloom\n");
		//copy the screen and draw resized
		GL_Bind(r_bloomscreentexture);
		if (qglGetError())
			Con_Printf("GL Error whilst rendering bloom\n");
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, glheight - (curView_y + curView_height), curView_width, curView_height);
		if (qglGetError())
			Con_Printf("GL Error whilst rendering bloom\n");
		R_Bloom_Quad( 0,  glheight-midsample_height, midsample_width, midsample_height, screenText_tcw, screenText_tch  );

		if (qglGetError())
			Con_Printf("GL Error whilst rendering bloom\n");
		
		//now copy into Downsampling (mid-sized) texture
		GL_Bind(r_bloomdownsamplingtexture);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, midsample_width, midsample_height);

		if (qglGetError())
			Con_Printf("GL Error whilst rendering bloom\n");

		//now draw again in bloom size
		qglColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		R_Bloom_Quad( 0,  glheight-sample_height, sample_width, sample_height, sampleText_tcw, sampleText_tch );

		if (qglGetError())
			Con_Printf("GL Error whilst rendering bloom\n");
		
		//now blend the big screen texture into the bloom generation space (hoping it adds some blur)
		qglEnable( GL_BLEND );
		qglBlendFunc(GL_ONE, GL_ONE);
		qglColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		GL_Bind(r_bloomscreentexture);
		R_Bloom_Quad( 0,  glheight-sample_height, sample_width, sample_height, screenText_tcw, screenText_tch );
		qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		qglDisable( GL_BLEND );

		if (qglGetError())
			Con_Printf("GL Error whilst rendering bloom\n");

	} else {	//downsample simple

		GL_Bind(r_bloomscreentexture);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, glheight - (curView_y + curView_height), curView_width, curView_height);
		R_Bloom_Quad( 0, glheight-sample_height, sample_width, sample_height, screenText_tcw, screenText_tch );
	}
}

/*
=================
R_BloomBlend
=================
*/
void R_BloomBlend (void)//refdef_t *fd, meshlist_t *meshlist )
{
	if(!r_bloom.value)
		return;

	if( !BLOOM_SIZE || screen_texture_width < glwidth || screen_texture_height < glheight)
		R_Bloom_InitTextures();

	if( screen_texture_width < BLOOM_SIZE ||
		screen_texture_height < BLOOM_SIZE )
		return;

	//set up full screen workspace
	qglViewport( 0, 0, glwidth, glheight );
	qglDisable( GL_DEPTH_TEST );
	qglMatrixMode( GL_PROJECTION );
    qglLoadIdentity ();
	qglOrtho(0, glwidth, glheight, 0, -10, 100);
	qglMatrixMode( GL_MODELVIEW );
    qglLoadIdentity ();
	qglDisable(GL_CULL_FACE);

	qglDisable( GL_BLEND );
	qglEnable( GL_TEXTURE_2D );

	qglColor4f( 1, 1, 1, 1 );

	if (qglGetError())
		Con_Printf("GL Error whilst rendering bloom\n");

	//set up current sizes
	curView_x = scr_vrect.x*((float)glwidth/vid.width);
	curView_y = scr_vrect.y*((float)glheight/vid.height);
	curView_width = scr_vrect.width*((float)glwidth/vid.width);
	curView_height = scr_vrect.height*((float)glheight/vid.height);
	screenText_tcw = ((float)curView_width / (float)screen_texture_width);
	screenText_tch = ((float)curView_height / (float)screen_texture_height);
	if( scr_vrect.height > scr_vrect.width ) {
		sampleText_tcw = ((float)scr_vrect.width / (float)scr_vrect.height);
		sampleText_tch = 1.0f;
	} else {
		sampleText_tcw = 1.0f;
		sampleText_tch = ((float)scr_vrect.height / (float)scr_vrect.width);
	}
	sample_width = BLOOM_SIZE * sampleText_tcw;
	sample_height = BLOOM_SIZE * sampleText_tch;

	if (qglGetError())
		Con_Printf("GL Error whilst rendering bloom\n");
	
	//copy the screen space we'll use to work into the backup texture
	GL_Bind(r_bloombackuptexture);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, r_screenbackuptexture_size * sampleText_tcw, r_screenbackuptexture_size * sampleText_tch);
	if (qglGetError())
		Con_Printf("GL Error whilst rendering bloom\n");
	//create the bloom image
	R_Bloom_DownsampleView();
	if (qglGetError())
		Con_Printf("GL Error whilst rendering bloom\n");
	R_Bloom_GeneratexDiamonds();
	//R_Bloom_GeneratexCross();

	if (qglGetError())
		Con_Printf("GL Error whilst rendering bloom\n");

	//restore the screen-backup to the screen
	qglDisable(GL_BLEND);
	GL_Bind(r_bloombackuptexture);
	qglColor4f( 1, 1, 1, 1 );
	R_Bloom_Quad( 0, 
		glheight - (r_screenbackuptexture_size * sampleText_tch),
		r_screenbackuptexture_size * sampleText_tcw,
		r_screenbackuptexture_size * sampleText_tch,
		sampleText_tcw,
		sampleText_tch );

	R_Bloom_DrawEffect();


	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (qglGetError())
		Con_Printf("GL Error whilst rendering bloom\n");
}

#endif
