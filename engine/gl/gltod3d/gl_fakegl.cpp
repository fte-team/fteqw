/*
Copyright (C) 2000 Jack Palevich.

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
// gl_fakegl.cpp -- Uses Direct3D 7.0 to implement a subset of OpenGL.

/*
This would probably be faster if it wasn't written in cpp.
the fact that it uses wrapper functions to call methods in a class could be a reasonable hit in speed.
*/

#include "bothdefs.h"	//our always-present config file

#ifdef USE_D3D


#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if 0
#undef WINGDIAPI
#define WINGDIAPI
#undef APIENTRY
#define APIENTRY
#endif

#include <gl/gl.h>

#if 0
#undef APIENTRY
#define APIENTRY    WINAPI
#undef WINGDIAPI
#define WINGDIAPI DECLSPEC_IMPORT
#endif

#pragma warning( disable : 4244 )
#pragma warning( disable : 4820 )

#if (_MSC_VER < 1400)
#define     D3D_OVERLOADS
#endif

#define     RELEASENULL(object) if (object) {object->Release();}

#include    "ddraw.h"
#include    "d3d.h"
#include    "d3dx.h"

typedef HRESULT (WINAPI *qD3DXInitialize_t)();
qD3DXInitialize_t qD3DXInitialize;
typedef HRESULT (WINAPI *qD3DXUninitialize_t)();
qD3DXUninitialize_t qD3DXUninitialize;
typedef D3DXMATRIX* (WINAPI *qD3DXMatrixScaling_t)    ( D3DXMATRIX *pOut, float sx, float sy, float sz );
qD3DXMatrixScaling_t qD3DXMatrixScaling;
typedef void (WINAPI *qD3DXGetErrorString_t)(HRESULT hr, DWORD   strLength, LPSTR   pStr);
qD3DXGetErrorString_t qD3DXGetErrorString;
typedef D3DXMATRIX* (WINAPI *qD3DXMatrixPerspectiveOffCenter_t) ( D3DXMATRIX *pOut, float l, float r, float b, float t, float zn, float zf );
qD3DXMatrixPerspectiveOffCenter_t qD3DXMatrixPerspectiveOffCenter;
typedef D3DXMATRIX* (WINAPI *qD3DXMatrixOrthoOffCenter_t) ( D3DXMATRIX *pOut, float l, float r, float b, float t, float zn, float zf );
qD3DXMatrixOrthoOffCenter_t qD3DXMatrixOrthoOffCenter;
typedef HRESULT (WINAPI *qD3DXCreateContextEx_t)(DWORD          deviceIndex,  DWORD          flags,HWND           hwnd,HWND           hwndFocus, DWORD          numColorBits,DWORD          numAlphaBits,DWORD          numDepthbits,DWORD          numStencilBits,DWORD          numBackBuffers,DWORD          width, DWORD          height,DWORD          refreshRate,LPD3DXCONTEXT* ppCtx);
qD3DXCreateContextEx_t qD3DXCreateContextEx;
typedef HRESULT (WINAPI *qD3DXCreateMatrixStack_t)( DWORD flags, LPD3DXMATRIXSTACK *ppStack );
qD3DXCreateMatrixStack_t qD3DXCreateMatrixStack;
typedef HRESULT (WINAPI *qD3DXCheckTextureRequirements_t)( LPDIRECT3DDEVICE7     pd3dDevice, LPDWORD               pFlags, LPDWORD               pWidth, LPDWORD               pHeight, D3DX_SURFACEFORMAT*   pPixelFormat);
qD3DXCheckTextureRequirements_t qD3DXCheckTextureRequirements;
typedef HRESULT (WINAPI *qD3DXMakeDDPixelFormat_t) (D3DX_SURFACEFORMAT d3dxFormat, DDPIXELFORMAT*     pddpf);
qD3DXMakeDDPixelFormat_t qD3DXMakeDDPixelFormat;
typedef D3DXMATRIX* (WINAPI *qD3DXMatrixTranslation_t) ( D3DXMATRIX *pOut, float x, float y, float z );
qD3DXMatrixTranslation_t qD3DXMatrixTranslation;

extern "C" {
#include "quakedef.h"
#include "glquake.h"
}
#ifdef USE_D3D

// Choose one of the following. D3DXContext is new in DX7, and
// provides a standard way of managing DX. D3DFrame is from
// the D3DIM sample code.
// Advantages of D3DXContext:
//   + less code.
//   + official standard.
//   + Does standard things correctly. (For example I can get Gamma
//     correction to work. I can't get it to work with D3DFrame,
//     probably because I've left out some stupid step.)
//
// Advantages of D3DFrame
//   + Some hacked DX7 drivers that are really DX6 drivers will crash
//     with D3DXContext, but work with D3DFrame. Pre-release Windows 2000
//     Voodoo drivers are an example.
//   + Source is available, so you can do things any way you want.

#define USE_D3DXCONTEXT
// #define USE_D3DFRAME

#ifdef USE_D3DFRAME
#include "d3denum.h"
#include "d3dframe.h"
#include "d3dutil.h"
#endif

#if 0
#undef APIENTRY
#define APIENTRY
#endif

#define    TEXTURE0_SGIS    0x835E
#define    TEXTURE1_SGIS				0x835F


#ifdef _DEBUG
void LocalDebugBreak(){
	DebugBreak();
}
#else
void LocalDebugBreak(){
}
#endif

// Globals
bool g_force16bitTextures;
bool gFullScreen = true;
DWORD gWidth = 640;
DWORD gHeight = 480;
DWORD gBpp = 16;
DWORD gZbpp = 16;
class FakeGL;
static FakeGL* gFakeGL;

class TextureEntry {
public:
	TextureEntry(){
		m_id = 0;
		m_texture = 0;
		m_format = D3DX_SF_UNKNOWN;
		m_internalFormat = 0;

		m_glTexParameter2DMinFilter = GL_NEAREST_MIPMAP_LINEAR;
		m_glTexParameter2DMagFilter = GL_LINEAR;
		m_glTexParameter2DWrapS = GL_REPEAT;
		m_glTexParameter2DWrapT = GL_REPEAT;
		m_maxAnisotropy = 1.0;
	}
	~TextureEntry(){
	}
	GLuint m_id;
	LPDIRECTDRAWSURFACE7 m_texture;
	D3DX_SURFACEFORMAT m_format;
	GLint m_internalFormat;

	GLint m_glTexParameter2DMinFilter;
	GLint m_glTexParameter2DMagFilter;
	GLint m_glTexParameter2DWrapS;
	GLint m_glTexParameter2DWrapT;
	float m_maxAnisotropy;
};


#define TASIZE 2000

class TextureTable {
public:
	TextureTable(){
		m_count = 0;
		m_size = 0;
		m_textures = 0;
		m_currentTexture = 0;
		m_currentID = 0;
		BindTexture(0);
	}
	~TextureTable(){
		DWORD i;
		for(i = 0; i < m_count; i++) {
			RELEASENULL(m_textures[i].m_texture);
		}
		for(i = 0; i < TASIZE; i++) {
			RELEASENULL(m_textureArray[i].m_texture);
		}

		delete [] m_textures;
	}

	void BindTexture(GLuint id){
		TextureEntry* oldEntry = m_currentTexture;
		m_currentID = id;

		if ( id < TASIZE ) {
			m_currentTexture = m_textureArray + id;
			if ( m_currentTexture->m_id ) {
				return;
			}
		}
		else {
			// Check overflow table.
			// Really ought to be a hash table.
			for(DWORD i = 0; i < m_count; i++){
				if ( id == m_textures[i].m_id ) {
					m_currentTexture =  m_textures + i;
					return;
				}
			}
			// It's a new ID.
			// Ensure space in the table
			if ( m_count >= m_size ) {
				int newSize = m_size * 2 + 10;
				TextureEntry* newTextures = new TextureEntry[newSize];
				for(DWORD i = 0; i < m_count; i++ ) {
					newTextures[i] = m_textures[i];
				}
				delete[] m_textures;
				m_textures = newTextures;
				m_size = newSize;
			}
			// Put new entry in table
			oldEntry = m_currentTexture;
			m_currentTexture = m_textures + m_count;
			m_count++;
		}
		if ( oldEntry ) {
			*m_currentTexture = *oldEntry;
		}
		m_currentTexture->m_id = id;
		m_currentTexture->m_texture = NULL;		
	}

	int GetCurrentID() {
		return m_currentID;
	}

	TextureEntry* GetCurrentEntry() {
		return m_currentTexture;
	}

	TextureEntry* GetEntry(GLuint id){
		if ( m_currentID == id && m_currentTexture ) {
			return m_currentTexture;
		}
		if ( id < TASIZE ) {
			return &m_textureArray[id];
		}
		else {
			// Check overflow table.
			// Really ought to be a hash table.
			for(DWORD i = 0; i < m_count; i++){
				if ( id == m_textures[i].m_id ) {
					return  &m_textures[i];
				}
			}
		}
		return 0;
	}

	LPDIRECTDRAWSURFACE7 GetTexture(){
		if ( m_currentTexture ) {
			return m_currentTexture->m_texture;
		}
		return 0;
	}

	LPDIRECTDRAWSURFACE7 GetTexture(int id){
		TextureEntry* entry = GetEntry(id);
		if ( entry ) {
			return entry->m_texture;
		}
		return 0;
	}

	D3DX_SURFACEFORMAT GetSurfaceFormat() {
		if ( m_currentTexture ) {
			return m_currentTexture->m_format;
		}
		return D3DX_SF_UNKNOWN;
	}

	GLint GetInternalFormat() {
		if ( m_currentTexture ) {
			return m_currentTexture->m_internalFormat;
		}
		return 0;
	}
	void SetTexture(LPDIRECTDRAWSURFACE7 texture, D3DX_SURFACEFORMAT d3dFormat, GLint internalFormat){
		if ( !m_currentTexture ) {
			BindTexture(0);
		}
		RELEASENULL ( m_currentTexture->m_texture );
		m_currentTexture->m_texture = texture;
		m_currentTexture->m_format = d3dFormat;
		m_currentTexture->m_internalFormat = internalFormat;
	}
private:
	GLuint m_currentID;
	DWORD m_count;
	DWORD m_size;
	TextureEntry m_textureArray[TASIZE]; // IDs 0..TASIZE-1
	TextureEntry* m_textures;			  // Overflow

	TextureEntry* m_currentTexture;
};


#if 1
#define Clamp(x) (x) // No clamping -- we've made sure the inputs are in the range 0..1
#else
float Clamp(float x) {
	if ( x < 0 ) {
		x = 0;
		LocalDebugBreak();
	}
	else if ( x > 1 ) {
		x = 1;
		LocalDebugBreak();
	}
	return x;
}
#endif

static DWORD GLToDXSBlend(GLenum glBlend){
	DWORD result = D3DBLEND_ONE;
	switch ( glBlend ) {
	case GL_ZERO: result = D3DBLEND_ZERO; break;
	case GL_ONE: result = D3DBLEND_ONE; break;
	case GL_DST_COLOR: result = D3DBLEND_DESTCOLOR; break;
	case GL_ONE_MINUS_DST_COLOR: result = D3DBLEND_INVDESTCOLOR; break;
	case GL_SRC_ALPHA: result = D3DBLEND_SRCALPHA; break;
	case GL_ONE_MINUS_SRC_ALPHA: result = D3DBLEND_INVSRCALPHA; break;
	case GL_DST_ALPHA: result = D3DBLEND_DESTALPHA; break;
	case GL_ONE_MINUS_DST_ALPHA: result = D3DBLEND_INVDESTALPHA; break;
	case GL_SRC_ALPHA_SATURATE: result = D3DBLEND_SRCALPHASAT; break;
	default: LocalDebugBreak(); break;
	}
	return result;
}

static DWORD GLToDXDBlend(GLenum glBlend){
	DWORD result = D3DBLEND_ONE;
	switch ( glBlend ) {
	case GL_ZERO: result = D3DBLEND_ZERO; break;
	case GL_ONE: result = D3DBLEND_ONE; break;
	case GL_SRC_COLOR: result = D3DBLEND_SRCCOLOR; break;
	case GL_ONE_MINUS_SRC_COLOR: result = D3DBLEND_INVSRCCOLOR; break;
	case GL_SRC_ALPHA: result = D3DBLEND_SRCALPHA; break;
	case GL_ONE_MINUS_SRC_ALPHA: result = D3DBLEND_INVSRCALPHA; break;
	case GL_DST_ALPHA: result = D3DBLEND_DESTALPHA; break;
	case GL_ONE_MINUS_DST_ALPHA: result = D3DBLEND_INVDESTALPHA; break;
	default: LocalDebugBreak(); break;
	}
	return result;
}

static DWORD GLToDXCompare(GLenum func){
	DWORD result = D3DCMP_ALWAYS;
	switch ( func ) {
	case GL_NEVER: result = D3DCMP_NEVER; break;
	case GL_LESS: result = D3DCMP_LESS; break;
	case GL_EQUAL: result = D3DCMP_EQUAL; break;
	case GL_LEQUAL: result = D3DCMP_LESSEQUAL; break;
	case GL_GREATER: result = D3DCMP_GREATER; break;
	case GL_NOTEQUAL: result = D3DCMP_NOTEQUAL; break;
	case GL_GEQUAL: result = D3DCMP_GREATEREQUAL; break;
	case GL_ALWAYS: result = D3DCMP_ALWAYS; break;
	default: break;
	}
	return result;
}

/*
   OpenGL                      MinFilter           MipFilter       Comments
   GL_NEAREST                  D3DTFN_POINT        D3DTFP_NONE
   GL_LINEAR                   D3DTFN_LINEAR       D3DTFP_NONE
   GL_NEAREST_MIPMAP_NEAREST   D3DTFN_POINT        D3DTFP_POINT        
   GL_LINEAR_MIPMAP_NEAREST    D3DTFN_LINEAR       D3DTFP_POINT    bilinear
   GL_NEAREST_MIPMAP_LINEAR    D3DTFN_POINT        D3DTFP_LINEAR 
   GL_LINEAR_MIPMAP_LINEAR     D3DTFN_LINEAR       D3DTFP_LINEAR   trilinear
*/
static DWORD GLToDXMinFilter(GLint filter){
	DWORD result = D3DTFN_LINEAR;
	switch ( filter ) {
	case GL_NEAREST: result = D3DTFN_POINT; break;
	case GL_LINEAR: result = D3DTFN_LINEAR; break;
	case GL_NEAREST_MIPMAP_NEAREST: result = D3DTFN_POINT; break;
	case GL_LINEAR_MIPMAP_NEAREST: result = D3DTFN_LINEAR; break;
	case GL_NEAREST_MIPMAP_LINEAR: result = D3DTFN_POINT; break;
	case GL_LINEAR_MIPMAP_LINEAR: result = D3DTFN_LINEAR; break;
	default:
		LocalDebugBreak();
		break;
	}
	return result;
}

static DWORD GLToDXMipFilter(GLint filter){
	DWORD result = D3DTFP_POINT;
	switch ( filter ) {
	case GL_NEAREST: result = D3DTFP_NONE; break;
	case GL_LINEAR: result = D3DTFP_NONE; break;
	case GL_NEAREST_MIPMAP_NEAREST: result = D3DTFP_POINT; break;
	case GL_LINEAR_MIPMAP_NEAREST: result = D3DTFP_POINT; break;
	case GL_NEAREST_MIPMAP_LINEAR: result = D3DTFP_LINEAR; break;
	case GL_LINEAR_MIPMAP_LINEAR: result = D3DTFP_LINEAR; break;
	default:
		LocalDebugBreak();
		break;
	}
	return result;
}

static DWORD GLToDXMagFilter(GLint filter){
	DWORD result = D3DTFG_POINT;
	switch ( filter ) {
	case GL_NEAREST: result = D3DTFG_POINT; break;
	case GL_LINEAR: result = D3DTFG_LINEAR; break;
	default:
		LocalDebugBreak();
		break;
	}
	return result;
}

static DWORD GLToDXTextEnvMode(GLint mode){
	DWORD result = D3DTOP_MODULATE;
	switch ( mode ) {
	case GL_MODULATE: result = D3DTOP_MODULATE; break;
	case GL_DECAL: result = D3DTOP_SELECTARG1; break; // Fix this
	case GL_BLEND: result = D3DTOP_BLENDTEXTUREALPHA; break;
	case GL_REPLACE: result = D3DTOP_SELECTARG1; break;
	default: break;
	}
	return result;
}

#define MAXSTATES 8

class TextureStageState {
public:
	TextureStageState() {
		m_currentTexture = 0;
		m_glTextEnvMode = GL_MODULATE;
		m_glTexture2D = false;
		m_dirty = true;
	}

	bool GetDirty() { return m_dirty; }
	void SetDirty(bool dirty) { m_dirty = dirty; }

	void DirtyTexture(GLuint textureID) {
		if ( textureID == m_currentTexture ) {
			m_dirty = true;
		}
	}

	GLuint GetCurrentTexture() { return m_currentTexture; }
	void SetCurrentTexture(GLuint texture) { m_dirty = true; m_currentTexture = texture; }

	GLfloat GetTextEnvMode() { return m_glTextEnvMode; }
	void SetTextEnvMode(GLfloat mode) { m_dirty = true; m_glTextEnvMode = mode; }

	bool GetTexture2D() { return m_glTexture2D; }
	void SetTexture2D(bool texture2D) { m_dirty = true; m_glTexture2D = texture2D; }

private:
	
	GLuint m_currentTexture;
	GLfloat m_glTextEnvMode;
	bool m_glTexture2D;
	bool m_dirty;
};

class TextureState {
public:
	TextureState(){
		m_currentStage = 0;
		memset(&m_stage, 0, sizeof(m_stage));
		m_dirty = false;
		m_mainBlend = false;
	}

	void SetMaxStages(int maxStages){
		m_maxStages = maxStages;
		for(int i = 0; i < m_maxStages;i++){
			m_stage[i].SetDirty(true);
		}
		m_dirty = true;
	}

	// Keep track of changes to texture stage state
	void SetCurrentStage(int index){
		m_currentStage = index;
	}

	int GetMaxStages() { return m_maxStages; }
	bool GetDirty() { return m_dirty; }
	void DirtyTexture(int textureID){
		for(int i = 0; i < m_maxStages;i++){
			m_stage[i].DirtyTexture(textureID);
		}
		m_dirty = true;
	}

	void SetMainBlend(bool mainBlend){
		m_mainBlend = mainBlend;
		m_stage[0].SetDirty(true);
		m_dirty = true;
	}

	// These methods apply to the current stage

	GLuint GetCurrentTexture() { return Get()->GetCurrentTexture(); }
	void SetCurrentTexture(GLuint texture) { m_dirty = true; Get()->SetCurrentTexture(texture); }

	GLfloat GetTextEnvMode() { return Get()->GetTextEnvMode(); }
	void SetTextEnvMode(GLfloat mode) { m_dirty = true; Get()->SetTextEnvMode(mode); }

	bool GetTexture2D() { return Get()->GetTexture2D(); }
	void SetTexture2D(bool texture2D) { m_dirty = true; Get()->SetTexture2D(texture2D); }

	void SetTextureStageState(LPDIRECT3DDEVICE7 pD3DDev, TextureTable* textures){
		if ( ! m_dirty ) {
			return;
		}

		m_dirty = false;

		for(int i = 0; i < m_maxStages; i++ ) {
			if ( ! m_stage[i].GetDirty() ) {
				continue;
			}
			m_stage[i].SetDirty(false);
			if ( m_stage[i].GetTexture2D() ) {
				DWORD color1 = D3DTA_TEXTURE;
				int textEnvMode =  m_stage[i].GetTextEnvMode();
				DWORD colorOp = GLToDXTextEnvMode(textEnvMode);
				if ( i > 0 && textEnvMode == GL_BLEND ) {
					// Assume we're doing multi-texture light mapping.
					// I don't think this is the right way to do this
					// but it works for D3DQuake.
					colorOp = D3DTOP_MODULATE;
					color1 |= D3DTA_COMPLEMENT;
				}
				pD3DDev->SetTextureStageState( i, D3DTSS_COLORARG1, color1);
				pD3DDev->SetTextureStageState( i, D3DTSS_COLORARG2, i == 0 ? D3DTA_DIFFUSE :  D3DTA_CURRENT);
				pD3DDev->SetTextureStageState( i, D3DTSS_COLOROP, colorOp);
				DWORD alpha1 = D3DTA_TEXTURE;
				DWORD alpha2 = D3DTA_DIFFUSE;
				DWORD alphaOp;
				alphaOp = GLToDXTextEnvMode(textEnvMode);
				if (i == 0 && m_mainBlend ) {
					alphaOp = D3DTOP_MODULATE;	// Otherwise the console is never transparent
				}
				pD3DDev->SetTextureStageState( i, D3DTSS_ALPHAARG1, alpha1);
				pD3DDev->SetTextureStageState( i, D3DTSS_ALPHAARG2, alpha2);
				pD3DDev->SetTextureStageState( i, D3DTSS_ALPHAOP,   alphaOp);

				TextureEntry* entry = textures->GetEntry(m_stage[i].GetCurrentTexture());
				if ( entry ) {
					int minFilter = entry->m_glTexParameter2DMinFilter;
					DWORD dxMinFilter = GLToDXMinFilter(minFilter);
					DWORD dxMipFilter = GLToDXMipFilter(minFilter);
					DWORD dxMagFilter = GLToDXMagFilter(entry->m_glTexParameter2DMagFilter);

					// Avoid setting anisotropic if the user doesn't request it.
					static bool bSetMaxAnisotropy = false;
					if ( entry->m_maxAnisotropy != 1.0f ) {
						bSetMaxAnisotropy = true;
						if ( dxMagFilter == D3DTFG_LINEAR) {
							dxMagFilter = D3DTFG_ANISOTROPIC;
						}
						if ( dxMinFilter == D3DTFN_LINEAR) {
							dxMinFilter = D3DTFN_ANISOTROPIC;
						}
					}
					if ( bSetMaxAnisotropy ) {
						pD3DDev->SetTextureStageState( i, D3DTSS_MAXANISOTROPY, entry->m_maxAnisotropy);
					}
					pD3DDev->SetTextureStageState( i, D3DTSS_MINFILTER, dxMinFilter );
					pD3DDev->SetTextureStageState( i, D3DTSS_MIPFILTER, dxMipFilter );
					pD3DDev->SetTextureStageState( i, D3DTSS_MAGFILTER,  dxMagFilter);
					LPDIRECTDRAWSURFACE7 pTexture = entry->m_texture;
					if ( pTexture ) {
						pD3DDev->SetTexture( i, pTexture);
					}
				}
			}
			else {
				pD3DDev->SetTexture( i, NULL);
				pD3DDev->SetTextureStageState( i, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				pD3DDev->SetTextureStageState( i, D3DTSS_COLORARG2, i == 0 ? D3DTA_DIFFUSE :  D3DTA_CURRENT);
				pD3DDev->SetTextureStageState( i, D3DTSS_COLOROP, D3DTOP_DISABLE);
			}
		}
	}

private:
	TextureStageState* Get() {
		return m_stage + m_currentStage;
	}

	bool m_dirty;
	bool m_mainBlend;
	int m_maxStages;
	int m_currentStage;
	TextureStageState m_stage[MAXSTATES];
};

// This class buffers up all the glVertex calls between
// glBegin and glEnd.
//
// Choose one of these three
// USE_DRAWINDEXEDPRIMITIVE seems slightly faster (54 fps vs 53 fps) than USE_DRAWPRIMITIVE.
// USE_DRAWINDEXEDPRIMITIVEVB is much slower (30fps vs 54fps), at least on GeForce Win9x 3.75.

// #define USE_DRAWPRIMITIVE
#define USE_DRAWINDEXEDPRIMITIVE
//#define USE_DRAWINDEXEDPRIMITIVEVB

#if defined(USE_DRAWINDEXEDPRIMITIVE) || defined(USE_DRAWINDEXEDPRIMITIVEVB)
#define USE_INDECIES
#endif

#ifdef USE_DRAWINDEXEDPRIMITIVEVB
// The DX 7 docs suggest that you can get away with just one
// vertex buffer. But drivers (NVIDIA 3.75 on Win2K) don't seem to like that.

#endif

#ifdef USE_INDECIES
#define VERTSUSED 1024
#define VERTSSLOP 100
#endif

#ifdef USE_INDECIES

class OGLPrimitiveVertexBuffer {
public:
	OGLPrimitiveVertexBuffer(){
		m_drawMode = (GLuint) -1;
		m_size = 0;
		m_count = 0;
		m_OGLPrimitiveVertexBuffer = 0;
		m_vertexCount = 0;
		m_vertexTypeDesc = 0;
		memset(m_textureCoords, 0, sizeof(m_textureCoords));

		m_pD3DDev = 0;
#ifdef USE_DRAWINDEXEDPRIMITIVEVB
		m_buffer = 0;
#else
		m_buffer = 0;
#endif
		m_color = (DWORD) D3DRGBA(0.0,0.0,0.0,1.0); // Don't know if this is correct
		m_indecies = 0;
		m_indexCount = 0;
	}

	~OGLPrimitiveVertexBuffer(){
		delete [] m_indecies;
#ifdef USE_DRAWINDEXEDPRIMITIVEVB
			RELEASENULL(m_buffer);
#else
		delete[] m_buffer;
#endif
	}

	HRESULT Initialize(LPDIRECT3DDEVICE7 pD3DDev, IDirect3D7* pD3D7, bool hardwareTandL, DWORD typeDesc){
		m_pD3DDev = pD3DDev;

		int numVerts = VERTSUSED + VERTSSLOP;

		m_vertexTypeDesc = typeDesc;
		m_vertexSize = 0;
		if ( m_vertexTypeDesc & D3DFVF_XYZ ) {
			m_vertexSize += 3 * sizeof(float);
		}
		if ( m_vertexTypeDesc & D3DFVF_DIFFUSE ) {
			m_vertexSize += 4;
		}
		int textureStages = (m_vertexTypeDesc & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
		m_vertexSize += 2 * sizeof(float) * textureStages;

		m_indexSize = numVerts * 3;
		delete [] m_indecies;
		m_indecies = new WORD[m_indexSize];

#ifdef USE_DRAWINDEXEDPRIMITIVEVB
		D3DVERTEXBUFFERDESC vbdesc = {sizeof(D3DVERTEXBUFFERDESC)};
		vbdesc.dwCaps = D3DVBCAPS_WRITEONLY;
		if ( ! hardwareTandL ) {
			vbdesc.dwCaps |= D3DVBCAPS_SYSTEMMEMORY;
		}
		vbdesc.dwFVF = typeDesc;
		vbdesc.dwNumVertices = numVerts;
		RELEASENULL(m_buffer);
		HRESULT hr = pD3D7->CreateVertexBuffer(&vbdesc, &m_buffer, 0);
		if ( FAILED(hr) ) {
			return hr;
		}
#else
		m_size = (VERTSUSED + VERTSSLOP) * m_vertexSize;
		delete[] m_buffer;
		m_buffer = new char[m_size];
#endif
		
		return S_OK;
	}

	DWORD GetVertexTypeDesc(){
		return m_vertexTypeDesc;
	}

	LPVOID GetOGLPrimitiveVertexBuffer(){
		return m_OGLPrimitiveVertexBuffer;
	}

	DWORD GetVertexCount(){
		return m_vertexCount;
	}

	inline void SetColor(D3DCOLOR color){
		m_color = color;
	}
	
	inline void SetTextureCoord0(float u, float v){
		DWORD* pCoords = (DWORD*) m_textureCoords;
		pCoords[0] = *(DWORD*)& u;
		pCoords[1] = *(DWORD*)& v;
	}

	inline void SetTextureCoord(int textStage, float u, float v){
		DWORD* pCoords = (DWORD*) m_textureCoords + (textStage << 1);
		pCoords[0] = *(DWORD*)& u;
		pCoords[1] = *(DWORD*)& v;
	}

	void CheckFlush() {
		if ( m_size && m_indexCount &&
			((m_count + m_vertexSize * VERTSSLOP > m_size ) 
			|| (m_indexCount + VERTSSLOP*6 > m_indexSize) ) ) {
			Flush();
		}
	}

	void Flush() {
		if ( m_indexCount > 0 ) {
#ifdef USE_DRAWINDEXEDPRIMITIVEVB
			m_OGLPrimitiveVertexBuffer = 0;
			m_buffer->Unlock();
     		HRESULT hr = m_pD3DDev->DrawIndexedPrimitiveVB(
				D3DPT_TRIANGLELIST, m_buffer, 
				0, m_vertexCount, m_indecies, m_indexCount, 0);
			if ( FAILED(hr) ) {
				// LocalDebugBreak(); // ? NVidia driver sometimes says it's out of memory
			}
#else
			m_OGLPrimitiveVertexBuffer = 0;
     		HRESULT hr = m_pD3DDev->DrawIndexedPrimitive(
				D3DPT_TRIANGLELIST, m_vertexTypeDesc, m_buffer, 
				m_vertexCount, m_indecies, m_indexCount, 0);
			if ( FAILED(hr) ) {
				 LocalDebugBreak(); // ? NVidia driver sometimes says it's out of memory
			}
#endif
		}
		else {
			LocalDebugBreak();
		}
		m_indexCount = 0;
		m_vertexState = 0;
	}

	void SetVertex(float x, float y, float z){
		bool bCheckFlush = false;
		if (m_count + m_vertexSize > m_size) {
			Ensure(m_vertexSize);
		}
		if ( ! m_OGLPrimitiveVertexBuffer ) {
			LockBuffer();
		}
		DWORD* pFloat = (DWORD*) (m_OGLPrimitiveVertexBuffer + m_count);
		pFloat[0] = *(DWORD*)& x;
		pFloat[1] = *(DWORD*)& y;
		pFloat[2] = *(DWORD*)& z;
		const DWORD* pCoords = (DWORD*) m_textureCoords;
		switch(m_vertexTypeDesc){
		case (D3DFVF_XYZ | D3DFVF_DIFFUSE | (1 << D3DFVF_TEXCOUNT_SHIFT)):
			pFloat[3] = m_color;
			pFloat[4] = pCoords[0];
			pFloat[5] = pCoords[1];
			break;
		case (D3DFVF_XYZ | D3DFVF_DIFFUSE | (2 << D3DFVF_TEXCOUNT_SHIFT)):
			pFloat[3] = m_color;
			pFloat[4] = pCoords[0];
			pFloat[5] = pCoords[1];
			pFloat[6] = pCoords[2];
			pFloat[7] = pCoords[3];
			break;
		default:
			{
				if ( m_vertexTypeDesc & D3DFVF_DIFFUSE ) {
					*pFloat++ = m_color;
				}
				int textureStages = (m_vertexTypeDesc & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
				for ( int i = 0; i < textureStages; i++ ) {
					*pFloat++ = *pCoords++;
					*pFloat++ = *pCoords++;
				}
			}
			break;
		}

		if( m_indexCount < m_indexSize - 5){
			// Convert quads to double triangles
			switch ( m_drawMode ) {
			default:
				LocalDebugBreak();
				break;
			case GL_LINES:
				{
					m_indecies[m_indexCount++] = m_vertexCount;
					if ( m_vertexState++==1)
					{
						SetVertex(x+1, y+1, z+1);
//						m_indecies[m_indexCount++] = m_vertexCount;
						m_vertexState = 0;
						bCheckFlush = true; // Flush for long sequences of quads.
					}
				}
				break;
			case GL_TRIANGLES:
				m_indecies[m_indexCount++] = m_vertexCount;
				if ( m_vertexState++ == 2 ) {
					m_vertexState = 0;
					bCheckFlush = true; // Flush for long sequences of triangles.
				}
				break;
			case GL_QUADS:
				{
					if ( m_vertexState++ < 3) {
						m_indecies[m_indexCount++] = m_vertexCount;
					}
					else {
						// We've already done triangle (0 , 1, 2), now draw (2, 3, 0)
						m_indecies[m_indexCount++] = m_vertexCount-1;
						m_indecies[m_indexCount++] = m_vertexCount;
						m_indecies[m_indexCount++] = m_vertexCount-3;
						m_vertexState = 0;
						bCheckFlush = true; // Flush for long sequences of quads.
					}
				}
				break;
			case GL_TRIANGLE_STRIP:
				{
					if ( m_vertexState > VERTSSLOP ) {
						// This is a strip that's too big for us to buffer.
						// (We can't just flush the buffer because we have to keep
						// track of the last two vertices.
						LocalDebugBreak();
					}
					if ( m_vertexState++ < 3) {
						m_indecies[m_indexCount++] = m_vertexCount;
					}
					else {
						// Flip triangles between clockwise and counter clockwise
						if (m_vertexState & 1) {
							// draw triangle [n-2 n-1 n]
							m_indecies[m_indexCount++] = m_vertexCount-2;
							m_indecies[m_indexCount++] = m_vertexCount-1;
							m_indecies[m_indexCount++] = m_vertexCount;
						}
						else {
							// draw triangle [n-1 n-2 n]
							m_indecies[m_indexCount++] = m_vertexCount-1;
							m_indecies[m_indexCount++] = m_vertexCount-2;
							m_indecies[m_indexCount++] = m_vertexCount;
						}
					}
				}
				break;
			case GL_TRIANGLE_FAN:
			case GL_POLYGON:
				{
					if ( m_vertexState > VERTSSLOP ) {
						// This is a polygon or fan that's too big for us to buffer.
						// (We can't just flush the buffer because we have to keep
						// track of the starting vertex.
						LocalDebugBreak();
					}
					if ( m_vertexState++ < 3) {
						m_indecies[m_indexCount++] = m_vertexCount;
					}
					else {
						// Draw triangle [0 n-1 n]
						m_indecies[m_indexCount++] = m_vertexCount-(m_vertexState-1);
						m_indecies[m_indexCount++] = m_vertexCount-1;
						m_indecies[m_indexCount++] = m_vertexCount;
					}
				}
				break;
			}
		}
		else {
			LocalDebugBreak();
		}

		m_count += m_vertexSize;
		m_vertexCount++;
		if ( bCheckFlush ) {
			CheckFlush();
		}
	}

	inline bool IsMergableMode(GLenum /* mode */){
		CheckFlush();
		return true;
	}

	void Begin(GLuint drawMode){
		m_drawMode = drawMode;
		CheckFlush();
		if ( ! m_OGLPrimitiveVertexBuffer ) {
			LockBuffer();
		}
		m_vertexState = 0;
	}

	void Append(GLuint drawMode){
		m_drawMode = drawMode;
		CheckFlush();
		m_vertexState = 0;
	}

	void LockBuffer(){
		if ( ! m_OGLPrimitiveVertexBuffer ) {
#ifdef USE_DRAWINDEXEDPRIMITIVEVB
			void* memory = 0;
			// If there's room in the buffer, we try to append to what's already there.
			DWORD dwFlags = DDLOCK_WAIT | DDLOCK_WRITEONLY;
			if ( m_vertexCount > 0 && m_vertexCount < VERTSUSED ){
				dwFlags |= DDLOCK_NOOVERWRITE;
			}
			else {
				m_vertexCount = 0;
				m_count = 0;
				dwFlags |= DDLOCK_DISCARDCONTENTS;
			}
			HRESULT hr = m_buffer->Lock(dwFlags, & memory, &m_size);
			if ( FAILED(hr) || ! memory) {
//				LocalDebugBreak();

				while (!memory)
					hr = m_buffer->Lock(dwFlags, & memory, &m_size);
			}
			m_OGLPrimitiveVertexBuffer = (char*) memory;
#else
			m_OGLPrimitiveVertexBuffer = (char*) m_buffer;
			m_vertexCount = 0;
			m_count = 0;
#endif
			m_indexCount = 0;
		}
	}

	void End(){
		if ( m_indexCount == 0 ) { // Startup
			return;
		}
		Flush();
	}
private:
	void Ensure(int size){
		if (( m_count + size ) > m_size ) {
			LocalDebugBreak();
		}
	}

	GLuint m_drawMode;
	DWORD  m_vertexTypeDesc;
	int m_vertexSize; // in bytes

	LPDIRECT3DDEVICE7 m_pD3DDev;
#ifdef USE_DRAWINDEXEDPRIMITIVEVB
	IDirect3DVertexBuffer7* m_buffer;
#else
	char* m_buffer;
#endif
	char* m_OGLPrimitiveVertexBuffer;
	DWORD m_size; // total vertex buffer size in bytes
	DWORD m_count; // used ammount of vertex buffer, in bytes
	DWORD m_vertexCount;
	DWORD m_indexCount;
	int m_vertexState; // Cycles from 0..n-1 where n is the number of verticies in a primitive.
	DWORD m_indexSize;
	WORD* m_indecies;
	D3DCOLOR m_color;
	float m_textureCoords[MAXSTATES*2];
};
#endif

#ifdef USE_DRAWPRIMITIVE
class OGLPrimitiveVertexBuffer {
public:
	OGLPrimitiveVertexBuffer(){
		m_drawMode = -1;
		m_size = 0;
		m_count = 0;
		m_OGLPrimitiveVertexBuffer = 0;
		m_vertexCount = 0;
		m_vertexTypeDesc = 0;
		memset(m_textureCoords, 0, sizeof(m_textureCoords));

		m_pD3DDev = 0;
		m_color = D3DRGBA(0.0,0.0,0.0,1.0); // Don't know if this is correct
	}

	~OGLPrimitiveVertexBuffer(){
		delete [] m_OGLPrimitiveVertexBuffer;
	}

	HRESULT Initialize(LPDIRECT3DDEVICE7 pD3DDev, IDirect3D7* pD3D7, bool hardwareTandL, DWORD typeDesc){
		m_pD3DDev = pD3DDev;
		if (m_vertexTypeDesc != typeDesc) {
			m_vertexTypeDesc = typeDesc;
			m_vertexSize = 0;
			if ( m_vertexTypeDesc & D3DFVF_XYZ ) {
				m_vertexSize += 3 * sizeof(float);
			}
			if ( m_vertexTypeDesc & D3DFVF_DIFFUSE ) {
				m_vertexSize += 4;
			}
			int textureStages = (m_vertexTypeDesc & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
			m_vertexSize += 2 * sizeof(float) * textureStages;
		}
		return S_OK;
	}

	DWORD GetVertexTypeDesc(){
		return m_vertexTypeDesc;
	}

	LPVOID GetOGLPrimitiveVertexBuffer(){
		return m_OGLPrimitiveVertexBuffer;
	}

	DWORD GetVertexCount(){
		return m_vertexCount;
	}

	inline void SetColor(D3DCOLOR color){
		m_color = color;
	}
	
	inline void SetTextureCoord0(float u, float v){
		DWORD* pCoords = (DWORD*) m_textureCoords;
		pCoords[0] = *(DWORD*)& u;
		pCoords[1] = *(DWORD*)& v;
	}

	inline void SetTextureCoord(int textStage, float u, float v){
		DWORD* pCoords = (DWORD*) m_textureCoords + (textStage << 1);
		pCoords[0] = *(DWORD*)& u;
		pCoords[1] = *(DWORD*)& v;
	}

	void SetVertex(float x, float y, float z){
		int newCount = m_count + m_vertexSize;
		if (newCount > m_size) {
			Ensure(m_vertexSize);
		}
		DWORD* pFloat = (DWORD*) (m_OGLPrimitiveVertexBuffer + m_count);
		pFloat[0] = *(DWORD*)& x;
		pFloat[1] = *(DWORD*)& y;
		pFloat[2] = *(DWORD*)& z;
		const DWORD* pCoords = (DWORD*) m_textureCoords;
		switch(m_vertexTypeDesc){
		case (D3DFVF_XYZ | D3DFVF_DIFFUSE | (1 << D3DFVF_TEXCOUNT_SHIFT)):
			pFloat[3] = m_color;
			pFloat[4] = pCoords[0];
			pFloat[5] = pCoords[1];
			break;
		case (D3DFVF_XYZ | D3DFVF_DIFFUSE | (2 << D3DFVF_TEXCOUNT_SHIFT)):
			pFloat[3] = m_color;
			pFloat[4] = pCoords[0];
			pFloat[5] = pCoords[1];
			pFloat[6] = pCoords[2];
			pFloat[7] = pCoords[3];
			break;
		default:
			{
				if ( m_vertexTypeDesc & D3DFVF_DIFFUSE ) {
					*pFloat++ = m_color;
				}
				int textureStages = (m_vertexTypeDesc & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
				for ( int i = 0; i < textureStages; i++ ) {
					*pFloat++ = *pCoords++;
					*pFloat++ = *pCoords++;
				}
			}
			break;
		}
		m_count = newCount;
		m_vertexCount++;

		// TO DO: Flush vertex buffer if larger than 1000 vertexes.
		// Have to do this modulo vertexes-per-primitive
	}

	inline IsMergableMode(GLenum mode){
		return ( mode == m_drawMode ) && ( mode == GL_QUADS || mode == GL_TRIANGLES );
	}

	void Begin(GLuint drawMode){
		m_drawMode = drawMode;
	}

	void Append(GLuint drawMode){
	}

	void End(){
		if ( m_vertexCount == 0 ) { // Startup
			return;
		}
		D3DPRIMITIVETYPE dptPrimitiveType;
		switch ( m_drawMode ) {
		case GL_POINTS: dptPrimitiveType = D3DPT_POINTLIST; break;
		case GL_LINES: dptPrimitiveType = D3DPT_LINELIST; break;
		case GL_LINE_STRIP: dptPrimitiveType = D3DPT_LINESTRIP; break;
		case GL_LINE_LOOP:
			dptPrimitiveType = D3DPT_LINESTRIP;
			LocalDebugBreak();  // Need to add one more point
			break;		
		case GL_TRIANGLES: dptPrimitiveType = D3DPT_TRIANGLELIST; break;
		case GL_TRIANGLE_STRIP: dptPrimitiveType = D3DPT_TRIANGLESTRIP; break;
		case GL_TRIANGLE_FAN: dptPrimitiveType = D3DPT_TRIANGLEFAN; break;
		case GL_QUADS:
			if ( m_vertexCount <= 4 ) {
				dptPrimitiveType = D3DPT_TRIANGLEFAN;
			}
			else {
				dptPrimitiveType = D3DPT_TRIANGLELIST;
				ConvertQuadsToTriangles();
			}
			break;
		case GL_QUAD_STRIP:
			if ( m_vertexCount <= 4 ) {
				dptPrimitiveType = D3DPT_TRIANGLEFAN;
			}
			else {
				dptPrimitiveType = D3DPT_TRIANGLESTRIP;
				ConvertQuadStripToTriangleStrip();
			}
			break;

		case GL_POLYGON:
			dptPrimitiveType = D3DPT_TRIANGLEFAN;
			if ( m_vertexCount < 3) {
				goto exit;
			}
			// How is this different from GL_TRIANGLE_FAN, other than
			// that polygons are planar?
			break;
		default:
			LocalDebugBreak();
			goto exit;
		}
		{
     		HRESULT hr = m_pD3DDev->DrawPrimitive(
				dptPrimitiveType, m_vertexTypeDesc,
				m_OGLPrimitiveVertexBuffer, 
				m_vertexCount, 0);
			if ( FAILED(hr) ) {
				// LocalDebugBreak();
			}
		}
exit:
		m_vertexCount = 0;
		m_count = 0;
	}

private:
	void ConvertQuadsToTriangles(){
		int quadCount = m_vertexCount / 4;
		int addedVerticies = 2 * quadCount;
		int addedDataSize = addedVerticies * m_vertexSize;
		Ensure( addedDataSize );

		// A quad is v0, v1, v2, v3
		// The corresponding triangle pair is v0 v1 v2 , v0 v2 v3
		for(int i = quadCount-1; i >= 0; i--) {
			int startOfQuad = i * m_vertexSize * 4;
			int startOfTrianglePair = i * m_vertexSize * 6;
			// Copy the last two verticies of the second triangle
			memcpy(m_OGLPrimitiveVertexBuffer + startOfTrianglePair + 4 * m_vertexSize,
				m_OGLPrimitiveVertexBuffer + startOfQuad + m_vertexSize * 2, m_vertexSize * 2);
			// Copy the first vertex of the second triangle
			memcpy(m_OGLPrimitiveVertexBuffer + startOfTrianglePair + 3 * m_vertexSize,
				m_OGLPrimitiveVertexBuffer + startOfQuad, m_vertexSize);
			// Copy the first triangle
			if ( i > 0 ) {
				memcpy(m_OGLPrimitiveVertexBuffer + startOfTrianglePair, m_OGLPrimitiveVertexBuffer + startOfQuad, 3 * m_vertexSize);
			}
		}
		m_count += addedDataSize;
		m_vertexCount += addedVerticies;
	}

	void ConvertQuadStripToTriangleStrip(){
		int vertexPairCount = m_vertexCount / 2;

		// Doesn't add any points, but does reorder the verticies.
		// Swap each pair of verticies.

		for(int i = 0; i < vertexPairCount; i++) {
			int startOfPair = i * m_vertexSize * 2;
			int middleOfPair = startOfPair + m_vertexSize;
			for(int j = 0; j < m_vertexSize; j++) {
				int c = m_OGLPrimitiveVertexBuffer[startOfPair + j];
				m_OGLPrimitiveVertexBuffer[startOfPair + j] = m_OGLPrimitiveVertexBuffer[middleOfPair + j];
				m_OGLPrimitiveVertexBuffer[middleOfPair + j] = c;
			}
		}
	}

	void Ensure(int size){
		if (( m_count + size ) > m_size ) {
			int newSize = m_size * 2;
			if ( newSize < m_count + size ) newSize = m_count + size;
			char* newVB = new char[newSize];
			if ( m_OGLPrimitiveVertexBuffer ) {
				memcpy(newVB, m_OGLPrimitiveVertexBuffer, m_count);
			}
			delete[] m_OGLPrimitiveVertexBuffer;
			m_OGLPrimitiveVertexBuffer = newVB;
			m_size = newSize;
		}
	}

	GLuint m_drawMode;
	DWORD  m_vertexTypeDesc;
	int m_vertexSize; // in bytes

	LPDIRECT3DDEVICE7 m_pD3DDev;
	char* m_OGLPrimitiveVertexBuffer;
	int m_size;
	int m_count;
	DWORD m_vertexCount;
	D3DCOLOR m_color;
	float m_textureCoords[MAXSTATES*2];
};

#endif // USE_DRAWPRIMITIVE

class FakeGL {
private:
	LPDIRECT3DDEVICE7       m_pD3DDev;
    LPDIRECTDRAW7           m_pDD;
	LPDIRECT3D7				m_pD3D;
	LPDIRECTDRAWSURFACE7    m_pPrimary;
#ifdef USE_D3DXCONTEXT
    ID3DXContext*           m_pD3DX;
#endif
	bool m_hardwareTandL;

    BOOL                    m_bD3DXReady;
    HWND                    m_hwndMain;

	bool m_glRenderStateDirty;

	bool m_glAlphaStateDirty;
	GLenum m_glAlphaFunc;
	GLclampf m_glAlphaFuncRef;
	bool m_glAlphaTest;

	bool m_glBlendStateDirty;
	bool m_glBlend;
	GLenum m_glBlendFuncSFactor;
	GLenum m_glBlendFuncDFactor;

	bool m_glCullStateDirty;
	bool m_glCullFace;
	GLenum m_glCullFaceMode;

	bool m_glDepthStateDirty;
	bool m_glDepthTest;
	GLenum m_glDepthFunc;
	bool m_glDepthMask;

	GLclampd m_glDepthRangeNear;
	GLclampd m_glDepthRangeFar;

	GLenum m_glMatrixMode;

	GLenum m_glPolygonModeFront;
	GLenum m_glPolygonModeBack;

	bool m_glShadeModelStateDirty;
	GLenum m_glShadeModel;

	bool m_bViewPortDirty;
	GLint m_glViewPortX;
	GLint m_glViewPortY;
	GLsizei m_glViewPortWidth;
	GLsizei m_glViewPortHeight;

	TextureState m_textureState;
	TextureTable m_textures;

	bool m_modelViewMatrixStateDirty;
	bool m_projectionMatrixStateDirty;
	bool m_textureMatrixStateDirty;
	bool* m_currentMatrixStateDirty; // an alias to one of the preceeding stacks

	ID3DXMatrixStack* m_modelViewMatrixStack;
	ID3DXMatrixStack* m_projectionMatrixStack;
	ID3DXMatrixStack* m_textureMatrixStack;
	ID3DXMatrixStack* m_currentMatrixStack; // an alias to one of the preceeding stacks

	bool m_viewMatrixStateDirty;
	D3DXMATRIX m_d3dViewMatrix;

	OGLPrimitiveVertexBuffer m_OGLPrimitiveVertexBuffer;

	bool m_needBeginScene;

	const char* m_vendor;
	const char* m_renderer;
	char m_version[64];
	const char* m_extensions;
	DDDEVICEIDENTIFIER2 m_dddi;
	DWORD m_windowHeight;

	char* m_stickyAlloc;
	DWORD m_stickyAllocSize;

	bool m_hintGenerateMipMaps;

#ifdef USE_D3DFRAME
	D3DCOLOR m_clearColor;
#endif

	HRESULT ReleaseD3DX()
	{
#ifdef USE_D3DFRAME
		Cleanup3DEnvironment();
#endif
#ifdef USE_D3DXCONTEXT
		RELEASENULL(m_pDD);
		RELEASENULL(m_pD3D);
		RELEASENULL(m_pD3DDev);
		RELEASENULL(m_pPrimary);
		RELEASENULL(m_pD3DX);
#endif
		m_bD3DXReady = FALSE;
		qD3DXUninitialize();
		return S_OK;
	}

#ifdef USE_D3DFRAME
	static HRESULT AppConfirmFn(DDCAPS* caps, D3DDEVICEDESC7* desc){
		return S_OK;
	}
#endif

	HRESULT InitD3DX()
	{
		HRESULT hr;
		if( FAILED(hr = qD3DXInitialize()) )
			return hr;

#ifdef USE_D3DFRAME
		// Choose device

		hr = D3DEnum_EnumerateDevices(&AppConfirmFn);
		if( FAILED(hr) )
			return hr;

		hr = D3DEnum_SelectDefaultDevice(&m_pDeviceInfo, 0);
		if( FAILED(hr) )
			return hr;

		m_pDeviceInfo->bWindowed = gFullScreen ? 0 : 1;

		m_pFramework = new CD3DFramework7();
		
		if( FAILED( hr = Initialize3DEnvironment() ) )
			return hr;
#endif

#ifdef USE_D3DXCONTEXT
    
		DWORD width = gWidth;
		DWORD height = gHeight;
		DWORD bpp = gBpp;
		DWORD zbpp = gZbpp;

		// Try as specified.
		hr = qD3DXCreateContextEx(D3DX_DEFAULT, gFullScreen ? D3DX_CONTEXT_FULLSCREEN : 0,
			m_hwndMain, NULL, bpp, 0,
			zbpp, 0, 1, width, height, D3DX_DEFAULT, &m_pD3DX);
		if( FAILED(hr) ) {
			// default z-buffer
			hr = qD3DXCreateContextEx(D3DX_DEFAULT, gFullScreen ? D3DX_CONTEXT_FULLSCREEN : 0,
				m_hwndMain, NULL, bpp, 0,
				D3DX_DEFAULT, 0, 1, width, height, D3DX_DEFAULT, &m_pD3DX);
			if( FAILED(hr) ) {
				// default depth and z-buffer
				hr = qD3DXCreateContextEx(D3DX_DEFAULT, gFullScreen ? D3DX_CONTEXT_FULLSCREEN : 0,
					m_hwndMain, NULL, D3DX_DEFAULT, 0,
					D3DX_DEFAULT, 0, 1, width, height, D3DX_DEFAULT, &m_pD3DX);
				if( FAILED(hr) ) {
					// default everything
					hr = qD3DXCreateContextEx(D3DX_DEFAULT, gFullScreen ? D3DX_CONTEXT_FULLSCREEN : 0,
						m_hwndMain, NULL, D3DX_DEFAULT, 0,
						D3DX_DEFAULT, 0, 1, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, &m_pD3DX);
					if( FAILED(hr) ) {
						return hr;
					}
				}
			}
		}
		m_pDD = m_pD3DX->GetDD();
		m_pD3D = m_pD3DX->GetD3D();
		m_pD3DDev = m_pD3DX->GetD3DDevice();
		m_pPrimary = m_pD3DX->GetPrimary();
#endif
		m_bD3DXReady = TRUE;

		return hr;
	}

	void InterpretError(HRESULT hr)
	{
		char errStr[100];
		qD3DXGetErrorString(hr, 100, errStr );
		Con_Printf("%s\n", errStr);
//		MessageBox(NULL,errStr,"D3DX Error",MB_OK);
//		LocalDebugBreak();
	}

#ifdef USE_D3DFRAME
    D3DEnum_DeviceInfo*  m_pDeviceInfo;
    LPDIRECTDRAWSURFACE7 m_pddsRenderTargetLeft;
    DDSURFACEDESC2       m_ddsdRenderTarget;

    CD3DFramework7* m_pFramework;

	//-----------------------------------------------------------------------------
	// Name: Initialize3DEnvironment()
	// Desc: Initializes the sample framework, then calls the app-specific function
	//       to initialize device specific objects. This code is structured to
	//       handled any errors that may occur duing initialization
	//-----------------------------------------------------------------------------
	HRESULT Initialize3DEnvironment()
	{
		HRESULT hr;
		DWORD   dwFrameworkFlags = 0L;
		dwFrameworkFlags |= ( !m_pDeviceInfo->bWindowed ? D3DFW_FULLSCREEN : 0L );
		dwFrameworkFlags |= (  m_pDeviceInfo->bStereo   ? D3DFW_STEREO     : 0L );
		dwFrameworkFlags |= (  D3DFW_ZBUFFER );

		// Initialize the D3D framework
		if( SUCCEEDED( hr = m_pFramework->Initialize( m_hwndMain,
						 m_pDeviceInfo->pDriverGUID, m_pDeviceInfo->pDeviceGUID,
						 &m_pDeviceInfo->ddsdFullscreenMode, dwFrameworkFlags ) ) )
		{
			m_pDD        = m_pFramework->GetDirectDraw();
			m_pD3D       = m_pFramework->GetDirect3D();
			m_pD3DDev    = m_pFramework->GetD3DDevice();

			m_pPrimary     = m_pFramework->GetRenderSurface();
			m_pddsRenderTargetLeft = m_pFramework->GetRenderSurfaceLeft();

			m_ddsdRenderTarget.dwSize = sizeof(m_ddsdRenderTarget);
			m_pPrimary->GetSurfaceDesc( &m_ddsdRenderTarget );

		   // Let the app run its startup code which creates the 3d scene.
			if( SUCCEEDED( hr = InitDeviceObjects() ) )
				return S_OK;
			else
			{
				DeleteDeviceObjects();
				m_pFramework->DestroyObjects();
			}
		}

		// If we get here, the first initialization passed failed. If that was with a
		// hardware device, try again using a software rasterizer instead.
		if( m_pDeviceInfo->bHardware )
		{
			// Try again with a software rasterizer
			// DisplayFrameworkError( hr, MSGWARN_SWITCHEDTOSOFTWARE );
			D3DEnum_SelectDefaultDevice( &m_pDeviceInfo, D3DENUM_SOFTWAREONLY );
			return Initialize3DEnvironment();
		}

		return hr;
	}

	//-----------------------------------------------------------------------------
	// Name: Change3DEnvironment()
	// Desc: Handles driver, device, and/or mode changes for the app.
	//-----------------------------------------------------------------------------
	HRESULT Change3DEnvironment()
	{
		HRESULT hr;
		static BOOL  bOldWindowedState = TRUE;
		static DWORD dwSavedStyle;
		static RECT  rcSaved;

		// Release all scene objects that will be re-created for the new device
		DeleteDeviceObjects();

		// Release framework objects, so a new device can be created
		if( FAILED( hr = m_pFramework->DestroyObjects() ) )
		{
			// DisplayFrameworkError( hr, MSGERR_APPMUSTEXIT );
			SendMessage( m_hwndMain, WM_CLOSE, 0, 0 );
			return hr;
		}

		// Check if going from fullscreen to windowed mode, or vice versa.
		if( bOldWindowedState != m_pDeviceInfo->bWindowed )
		{
			if( m_pDeviceInfo->bWindowed )
			{
				// Coming from fullscreen mode, so restore window properties
				SetWindowLong( m_hwndMain, GWL_STYLE, dwSavedStyle );
				SetWindowPos( m_hwndMain, HWND_NOTOPMOST, rcSaved.left, rcSaved.top,
							  ( rcSaved.right - rcSaved.left ), 
							  ( rcSaved.bottom - rcSaved.top ), SWP_SHOWWINDOW );
			}
			else
			{
				// Going to fullscreen mode, save/set window properties as needed
				dwSavedStyle = GetWindowLong( m_hwndMain, GWL_STYLE );
				GetWindowRect( m_hwndMain, &rcSaved );
				SetWindowLong( m_hwndMain, GWL_STYLE, WS_POPUP|WS_SYSMENU|WS_VISIBLE );
			}

			bOldWindowedState = m_pDeviceInfo->bWindowed;
		}

		// Inform the framework class of the driver change. It will internally
		// re-create valid surfaces, a d3ddevice, etc.
		if( FAILED( hr = Initialize3DEnvironment() ) )
		{
			// DisplayFrameworkError( hr, MSGERR_APPMUSTEXIT );
			SendMessage( m_hwndMain, WM_CLOSE, 0, 0 );
			return hr;
		}
    
		return S_OK;
	}


	//-----------------------------------------------------------------------------
	// Name: Render3DEnvironment()
	// Desc: Draws the scene.
	//-----------------------------------------------------------------------------
	HRESULT Render3DEnvironment()
	{
		HRESULT hr;

		// Check the cooperative level before rendering
		if( FAILED( hr = m_pDD->TestCooperativeLevel() ) )
		{
			switch( hr )
			{
				case DDERR_EXCLUSIVEMODEALREADYSET:
				case DDERR_NOEXCLUSIVEMODE:
					// Do nothing because some other app has exclusive mode
					return S_OK;

				case DDERR_WRONGMODE:
					// The display mode changed on us. Resize accordingly
					if( m_pDeviceInfo->bWindowed )
						return Change3DEnvironment();
					break;
			}
			return hr;
		}

		// Show the frame on the primary surface.
		if( FAILED( hr = m_pFramework->ShowFrame() ) )
		{
			if( DDERR_SURFACELOST != hr )
				return hr;

			m_pFramework->RestoreSurfaces();
			RestoreSurfaces();
		}

		return S_OK;
	}



	//-----------------------------------------------------------------------------
	// Name: Cleanup3DEnvironment()
	// Desc: Cleanup scene objects
	//-----------------------------------------------------------------------------
	void Cleanup3DEnvironment()
	{
		if( m_pFramework )
		{
			DeleteDeviceObjects();
			delete m_pFramework; m_pFramework = 0;

			FinalCleanup();
		}

		D3DEnum_FreeResources();
	}

   // Overridable functions for the 3D scene created by the app
    virtual HRESULT OneTimeSceneInit()     { return S_OK; }
    virtual HRESULT InitDeviceObjects()    { return S_OK; }
    virtual HRESULT DeleteDeviceObjects()  { return S_OK; }
    virtual HRESULT Render()               { return S_OK; }
    virtual HRESULT FrameMove( FLOAT )     { return S_OK; }
    virtual HRESULT RestoreSurfaces()      { return S_OK; }
    virtual HRESULT FinalCleanup()         { return S_OK; }

#endif

public:
	FakeGL(HWND hwndMain){
		m_hwndMain = hwndMain;

		RECT rect;
		GetClientRect(m_hwndMain, &rect);
		m_windowHeight = rect.bottom - rect.top;
		m_bD3DXReady = TRUE;

		m_pD3DDev = 0;
		m_pDD = 0;
		m_pD3D = 0;
		m_pPrimary = 0;
#ifdef USE_D3DXCONTEXT
		m_pD3DX = 0;
#endif
#ifdef USE_D3DFRAME
		m_clearColor = 0;
#endif
		m_hardwareTandL = false;

		m_glRenderStateDirty = true;

		m_glAlphaStateDirty = true;
		m_glAlphaFunc = GL_ALWAYS;
		m_glAlphaFuncRef = 0;
		m_glAlphaTest = false;

		m_glBlendStateDirty = true;
		m_glBlend = false;
		m_glBlendFuncSFactor = GL_ONE; // Not sure this is the default
		m_glBlendFuncDFactor = GL_ZERO; // Not sure this is the default

		m_glCullStateDirty = true;
		m_glCullFace = false;
		m_glCullFaceMode = GL_BACK;

		m_glDepthStateDirty = true;
		m_glDepthTest = false;
		m_glDepthMask = true;
		m_glDepthFunc = GL_ALWAYS; // not sure if this is the default

		m_glDepthRangeNear = 0; // not sure if this is the default
		m_glDepthRangeFar = 1.0; // not sure if this is the default

		m_glMatrixMode = GL_MODELVIEW; // Not sure this is the default

		m_glPolygonModeFront = GL_FILL;
		m_glPolygonModeBack = GL_FILL;

		m_glShadeModelStateDirty = true;
		m_glShadeModel = GL_SMOOTH;


		m_bViewPortDirty = true;
		m_glViewPortX = 0;
		m_glViewPortY = 0;
		m_glViewPortWidth = rect.right - rect.left;
		m_glViewPortHeight = rect.bottom - rect.top;

		m_vendor = 0;
		m_renderer = 0;
		m_extensions = 0;

		m_hintGenerateMipMaps = true;

		HRESULT hr = InitD3DX();
		if ( FAILED(hr) ) {
			InterpretError(hr);
		}

		hr = qD3DXCreateMatrixStack(0, &m_modelViewMatrixStack);
		hr = qD3DXCreateMatrixStack(0, &m_projectionMatrixStack);
		hr = qD3DXCreateMatrixStack(0, &m_textureMatrixStack);
		m_currentMatrixStack = m_modelViewMatrixStack;
		m_modelViewMatrixStack->LoadIdentity(); // Not sure this is correct
		m_projectionMatrixStack->LoadIdentity();
		m_textureMatrixStack->LoadIdentity();
		m_modelViewMatrixStateDirty = true;
		m_projectionMatrixStateDirty = true;
		m_textureMatrixStateDirty = true;
		m_currentMatrixStateDirty = &m_modelViewMatrixStateDirty;
		m_viewMatrixStateDirty = true;

		D3DXMatrixIdentity(&m_d3dViewMatrix);

		m_needBeginScene = true;

		m_stickyAlloc = 0;
		m_stickyAllocSize = 0;

		{
			// Check for multitexture.
			D3DDEVICEDESC7 deviceCaps;
			HRESULT hr = m_pD3DDev->GetCaps(&deviceCaps);
			if ( ! FAILED(hr)) {
				// Clamp texture blend stages to 2. Some cards can do eight, but that's more
				// than we need.
				int maxStages = deviceCaps.wMaxTextureBlendStages;

				if ( maxStages > 2 ){
					maxStages = 2;
				}
				m_textureState.SetMaxStages(maxStages);

				m_hardwareTandL = (deviceCaps.dwDevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;

				for(int i = 0; i < maxStages; i++ ) {
					m_pD3DDev->SetTextureStageState(i, D3DTSS_TEXCOORDINDEX, i);
				}
			}
		}

		// One-time render state initialization

		m_pD3DDev->SetRenderState( D3DRENDERSTATE_TEXTUREFACTOR, 0x00000000 );
		m_pD3DDev->SetRenderState( D3DRENDERSTATE_DITHERENABLE, TRUE );
		m_pD3DDev->SetRenderState( D3DRENDERSTATE_SPECULARENABLE, FALSE );
		m_pD3DDev->SetRenderState( D3DRENDERSTATE_TEXTUREPERSPECTIVE, TRUE );
		m_pD3DDev->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
	}

	~FakeGL(){
		delete [] m_stickyAlloc;
		ReleaseD3DX();
		RELEASENULL(m_modelViewMatrixStack);
		RELEASENULL(m_projectionMatrixStack);
		RELEASENULL(m_textureMatrixStack);
	}

	void cglAlphaFunc (GLenum func, GLclampf ref){
		if ( m_glAlphaFunc != func || m_glAlphaFuncRef != ref ) {
			SetRenderStateDirty();
			m_glAlphaFunc = func;
			m_glAlphaFuncRef = ref;
			m_glAlphaStateDirty = true;
		}
	}

	void cglBegin (GLenum mode){
		if ( m_needBeginScene ){
			HRESULT hr = m_pD3DDev->BeginScene();
			if ( FAILED(hr) ) {
				InterpretError(hr);
				return;
			}
			else
				m_needBeginScene = false;
		}

#if 0
		// statistics
		static int beginCount;
		static int stateChangeCount;
		static int primitivesCount;
		beginCount++;
		if ( m_glRenderStateDirty )
			stateChangeCount++;
		if ( m_glRenderStateDirty || ! m_OGLPrimitiveVertexBuffer.IsMergableMode(mode) )
			primitivesCount++;
#endif

		if ( m_glRenderStateDirty || ! m_OGLPrimitiveVertexBuffer.IsMergableMode(mode) ) {
			internalEnd();
			SetGLRenderState();
			DWORD typeDesc;
			typeDesc = D3DFVF_XYZ | D3DFVF_DIFFUSE;
			typeDesc |= (m_textureState.GetMaxStages() << D3DFVF_TEXCOUNT_SHIFT);

			if ( typeDesc != m_OGLPrimitiveVertexBuffer.GetVertexTypeDesc()) {
				m_OGLPrimitiveVertexBuffer.Initialize(m_pD3DDev, m_pD3D, m_hardwareTandL, typeDesc);
			}
			m_OGLPrimitiveVertexBuffer.Begin(mode);
		}
		else {
			m_OGLPrimitiveVertexBuffer.Append(mode);
		}
	}

	void cglBindTexture(GLenum target, GLuint texture){
		if ( target != GL_TEXTURE_2D ) {
			LocalDebugBreak();
			return;
		}
		if ( m_textureState.GetCurrentTexture() != texture ) {
			SetRenderStateDirty();
			m_textureState.SetCurrentTexture(texture);
			m_textures.BindTexture(texture);
		}
	}

	inline void cglMTexCoord2fSGIS(GLenum target, GLfloat s, GLfloat t){
		int textStage = target - TEXTURE0_SGIS;
		m_OGLPrimitiveVertexBuffer.SetTextureCoord(textStage, s, t);
	}
	
	void cglSelectTextureSGIS(GLenum target){
		int textStage = target - TEXTURE0_SGIS;
		m_textureState.SetCurrentStage(textStage);
		m_textures.BindTexture(m_textureState.GetCurrentTexture());
		// Does not, by itself, dirty the render state
	}

	void cglBlendFunc (GLenum sfactor, GLenum dfactor){
		if ( m_glBlendFuncSFactor != sfactor || m_glBlendFuncDFactor != dfactor ) {
			SetRenderStateDirty();
			m_glBlendFuncSFactor = sfactor;
			m_glBlendFuncDFactor = dfactor;
			m_glBlendStateDirty = true;
		}
	}

	void cglClear (GLbitfield mask){
		HRESULT hr;
		internalEnd();
		SetGLRenderState();
		DWORD clearMask = 0;
		if ( mask & GL_COLOR_BUFFER_BIT ) {
			clearMask |= D3DCLEAR_TARGET;
		}

		if ( mask & GL_DEPTH_BUFFER_BIT ) {
			clearMask |= D3DCLEAR_ZBUFFER;
		}
#ifdef USE_D3DXCONTEXT
		hr = m_pD3DX->Clear(clearMask);
#endif

#ifdef USE_D3DFRAME
		hr = m_pD3DDev->Clear( 0, 0, clearMask, m_clearColor, 1.0f, 0L );
#endif
		if ( FAILED(hr) ){
			InterpretError(hr);
		}
	}

	void cglClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha){
		D3DCOLOR clearColor = D3DRGBA(Clamp(red), Clamp(green), Clamp(blue), Clamp(alpha));
#ifdef USE_D3DXCONTEXT
		HRESULT hr = m_pD3DX->SetClearColor(clearColor);
		if( FAILED(hr) ) {
			InterpretError(hr);
		}
#endif
#ifdef USE_D3DFRAME
		m_clearColor = clearColor;
#endif
	}

	inline void cglColor3f (GLfloat red, GLfloat green, GLfloat blue){
		// Note: On x86 architectures this function will chew up a lot of time
		// converting floating point to integer by calling _ftol
		// unless the /QIfist flag is specified.
		m_OGLPrimitiveVertexBuffer.SetColor(D3DRGB(Clamp(red), Clamp(green), Clamp(blue)));
	}

	inline void cglColor3ubv (const GLubyte *v){
		m_OGLPrimitiveVertexBuffer.SetColor(RGBA_MAKE(v[0], v[1], v[2], 0xff));
	}

	inline void cglColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha){
		// Note: On x86 architectures this function will chew up a lot of time
		// converting floating point to integer by calling _ftol
		// unless the /QIfist flag is specified.
		m_OGLPrimitiveVertexBuffer.SetColor(D3DRGBA(Clamp(red), Clamp(green), Clamp(blue), Clamp(alpha)));
	}

	inline void cglColor4fv (const GLfloat *v){
		// Note: On x86 architectures this function will chew up a lot of time
		// converting floating point to integer by calling _ftol
		// unless the /QIfist flag is specified.
		m_OGLPrimitiveVertexBuffer.SetColor(D3DRGBA(Clamp(v[0]), Clamp(v[1]), Clamp(v[2]), Clamp(v[3])));
	}

	void cglCullFace (GLenum mode){
		if ( m_glCullFaceMode != mode ) {
			SetRenderStateDirty();
			m_glCullFaceMode = mode;
			m_glCullStateDirty = true;
		}
	}

	void cglDepthFunc (GLenum func){
		if ( m_glDepthFunc != func ) {
			SetRenderStateDirty();
			m_glDepthFunc = func;
			m_glDepthStateDirty = true;
		}
	}

	void cglDepthMask (GLboolean flag){
		if ( m_glDepthMask != (flag != 0) ) {
			SetRenderStateDirty();
			m_glDepthMask = flag != 0 ? true : false;
			m_glDepthStateDirty = true;
		}
	}

	void cglDepthRange (GLclampd zNear, GLclampd zFar){
		if ( m_glDepthRangeNear != zNear || m_glDepthRangeFar != zFar ) {
			SetRenderStateDirty();
			m_glDepthRangeNear = zNear;
			m_glDepthRangeFar = zFar;
			m_bViewPortDirty = true;
		}
	}

	void cglDisable (GLenum cap){
		EnableDisableSet(cap, false);
	}

	void cglDrawBuffer (GLenum /* mode */){
		// Do nothing. (Can DirectX render to the front buffer at all?)
	}

	void cglEnable (GLenum cap){
		EnableDisableSet(cap, true);
	}

	void EnableDisableSet(GLenum cap, bool value){
		switch ( cap ) {
		case GL_ALPHA_TEST:
			if ( m_glAlphaTest != value ) {
				SetRenderStateDirty();
				m_glAlphaTest = value;
				m_glAlphaStateDirty = true;
			}
			break;
		case GL_BLEND:
			if ( m_glBlend != value ) {
				SetRenderStateDirty();
				m_textureState.SetMainBlend(value);
				m_glBlend = value;
				m_glBlendStateDirty = true;
			}
			break;
		case GL_CULL_FACE:
			if ( m_glCullFace != value ) {
				SetRenderStateDirty();
				m_glCullFace = value;
				m_glCullStateDirty = true;
			}
			break;
		case GL_DEPTH_TEST:
			if ( m_glDepthTest != value ) {
				SetRenderStateDirty();
				m_glDepthTest = value;
				m_glDepthStateDirty = true;
			}
			break;
		case GL_TEXTURE_2D:
			if ( m_textureState.GetTexture2D() != value ) {
				SetRenderStateDirty();
				m_textureState.SetTexture2D(value);
			}
			break;

		case GL_TEXTURE_GEN_S:
		case GL_TEXTURE_GEN_T:
			break;
		case GL_NORMALIZE:
			break;
		case GL_AUTO_NORMAL:
			break;
		case GL_DITHER:
		case GL_FOG:			
			break;
		case GL_POLYGON_OFFSET_FILL:	// I fear for the shaders.
			break;
		default:
			LocalDebugBreak();
			break;
		}
	}

	void cglEnd (void){
		// internalEnd();
	}

	void internalEnd(){
		m_OGLPrimitiveVertexBuffer.End();
	}

	void cglFinish (void){
		// To Do: This is supposed to flush all pending commands
		internalEnd();
	}

	void cglFrustum (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar){
		SetRenderStateDirty();
		D3DXMATRIX m;
		// Note that D3D takes top, bottom arguments in opposite order
		qD3DXMatrixPerspectiveOffCenter(&m, left, right, bottom, top, zNear, zFar);
		m_currentMatrixStack->MultMatrixLocal(&m);
		*m_currentMatrixStateDirty = true;
	}

	void cglGetFloatv (GLenum pname, GLfloat *params){
		switch(pname){
		case GL_MODELVIEW_MATRIX:
			memcpy(params,m_modelViewMatrixStack->GetTop(), sizeof(D3DMATRIX));
			break;
		default:
			LocalDebugBreak();
			break;
		}
	}

	const GLubyte * cglGetString (GLenum name){
		const char* result = "";
		EnsureDriverInfo();
		switch ( name ) {
		case GL_VENDOR:
			result = m_vendor;
			break;
		case GL_RENDERER:
			result = m_renderer;
			break;
		case GL_VERSION:
			result = m_version;
			break;
		case GL_EXTENSIONS:
			result = m_extensions;
			break;
		default:
			break;
		}
		return (const GLubyte *) result;
	}

	void cglHint (GLenum /* target */, GLenum /* mode */){
		LocalDebugBreak();
	}

	void cglLoadIdentity (void){
		SetRenderStateDirty();
		m_currentMatrixStack->LoadIdentity();
		*m_currentMatrixStateDirty = true;
	}

	void cglLoadMatrixf (const GLfloat *m){
		SetRenderStateDirty();
		m_currentMatrixStack->LoadMatrix((D3DXMATRIX*) m);
		*m_currentMatrixStateDirty = true;
	}
	void cglMultMatrixf (const GLfloat *m){
		SetRenderStateDirty();
		m_currentMatrixStack->MultMatrixLocal((D3DXMATRIX*) m);
		*m_currentMatrixStateDirty = true;
	}

	void cglMatrixMode (GLenum mode){
		m_glMatrixMode = mode;
		switch ( mode ) {
		case GL_MODELVIEW:
			m_currentMatrixStack = m_modelViewMatrixStack;
			m_currentMatrixStateDirty = &m_modelViewMatrixStateDirty;
			break;
		case GL_PROJECTION:
			m_currentMatrixStack = m_projectionMatrixStack;
			m_currentMatrixStateDirty = &m_projectionMatrixStateDirty;
			break;
		case GL_TEXTURE:
			m_currentMatrixStack = m_textureMatrixStack;
			m_currentMatrixStateDirty = &m_textureMatrixStateDirty;
			break;
		default:
			LocalDebugBreak();
			break;
		}
	}

	void cglOrtho (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar){
		SetRenderStateDirty();
		D3DXMATRIX m;
		qD3DXMatrixOrthoOffCenter(&m, left, right, top, bottom, zNear, zFar);
		m_currentMatrixStack->MultMatrixLocal(&m);
		*m_currentMatrixStateDirty = true;
	}

	void cglPolygonMode (GLenum face, GLenum mode){
		SetRenderStateDirty();
		switch ( face ) {
		case GL_FRONT:
			m_glPolygonModeFront = mode;
			break;
		case GL_BACK:
			m_glPolygonModeBack = mode;
			break;
		case GL_FRONT_AND_BACK:
			m_glPolygonModeFront = mode;
			m_glPolygonModeBack = mode;
			break;
		default:
			LocalDebugBreak();
			break;
		}
	}

	void cglPopMatrix (void){
		SetRenderStateDirty();
		m_currentMatrixStack->Pop();
		*m_currentMatrixStateDirty = true;
	}

	void cglPushMatrix (void){
		m_currentMatrixStack->Push();
		// Doesn't dirty matrix state
	}

	void cglReadBuffer (GLenum /* mode */){
		// Not that we allow reading from various buffers anyway.
	}

	void cglReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels){
		if ( format != GL_RGB || type != GL_UNSIGNED_BYTE) {
			LocalDebugBreak();
			return;
		}
		internalEnd();
#ifdef USE_D3DXCONTEXT
		LPDIRECTDRAWSURFACE7 back = m_pD3DX->GetBackBuffer(0);
#endif
#ifdef USE_D3DFRAME
		LPDIRECTDRAWSURFACE7 back = m_pFramework->GetBackBuffer();
#endif
		if(back) {
			DDSURFACEDESC2 desc = {sizeof(desc) };
			HRESULT hr = back->Lock(NULL, &desc, DDLOCK_READONLY | DDLOCK_WAIT, 0);
			if ( FAILED(hr) ) {
				InterpretError(hr);
				return;
			}
			CopyBitsToRGB(pixels, x, y, width, height, &desc);
			back->Unlock(NULL);
			RELEASENULL(back);
		}
	}

	static WORD GetNumberOfBits( DWORD dwMask )
	{
		WORD wBits = 0;
		while( dwMask )
		{
			dwMask = dwMask & ( dwMask - 1 );  
			wBits++;
		}
		return wBits;
	}

	static WORD GetShift( DWORD dwMask )
	{
		for(WORD i = 0; i < 32; i++ ) {
			if ( (1 << i) & dwMask ) {
				return i;
			}
		}
		return 0; // no bits in mask.
	}

	// Extract the bits and replicate out to an eight bit value
	static DWORD ExtractAndNormalize(DWORD rgba, DWORD shift, DWORD bits, DWORD mask){
		DWORD v = (rgba & mask) >> shift;
		// Assume bits >= 4
		v = (v | (v << bits));
		v = v >> (bits*2 - 8);
		return v;
	}

	void CopyBitsToRGB(void* pixels, DWORD sx, DWORD sy, DWORD width, DWORD height, LPDDSURFACEDESC2 pDesc){
		if ( ! (pDesc->ddpfPixelFormat.dwFlags & DDPF_RGB) ) {
			return; // Can't handle non-RGB surfaces
		}
		// We have to flip the Y axis to convert from D3D to openGL
		long destEndOfLineSkip = -2 * (width * 3);
		unsigned char* pDest = ((unsigned char*) pixels) + (height - 1) * width * 3 ;
		switch ( pDesc->ddpfPixelFormat.dwRGBBitCount ) {
		default:
			return;
		case 16:
			{
				unsigned short* pSource = (unsigned short*)
					(((unsigned char*) pDesc->lpSurface) + sx * sizeof(unsigned short) + sy * pDesc->lPitch);
				DWORD endOfLineSkip = pDesc->lPitch / sizeof(unsigned short) - pDesc->dwWidth;
				DWORD rMask = pDesc->ddpfPixelFormat.dwRBitMask;
				DWORD gMask = pDesc->ddpfPixelFormat.dwGBitMask;
				DWORD bMask = pDesc->ddpfPixelFormat.dwBBitMask;
				DWORD rShift = GetShift(rMask);
				DWORD rBits = GetNumberOfBits(rMask);
				DWORD gShift = GetShift(gMask);
				DWORD gBits = GetNumberOfBits(gMask);
				DWORD bShift = GetShift(bMask);
				DWORD bBits = GetNumberOfBits(bMask);
				for(DWORD y = 0; y < height; y++ ) {
					for (DWORD x = 0; x < width; x++ ) {
						unsigned short rgba = *pSource++;
						*pDest++ = ExtractAndNormalize(rgba, rShift, rBits, rMask);
						*pDest++ = ExtractAndNormalize(rgba, gShift, gBits, gMask);
						*pDest++ = ExtractAndNormalize(rgba, bShift, bBits, bMask);
					}
					pSource += endOfLineSkip;
					pDest += destEndOfLineSkip;
				}
			}
			break;
		case 32:
			{
				unsigned long* pSource = (unsigned long*)
					(((unsigned char*) pDesc->lpSurface) + sx * sizeof(unsigned long) + sy * pDesc->lPitch);
				DWORD endOfLineSkip = pDesc->lPitch / sizeof(unsigned long) - pDesc->dwWidth;
				for(DWORD y = 0; y < height; y++ ) {
					for (DWORD x = 0; x < width; x++ ) {
						unsigned long rgba = *pSource++;
						*pDest++ = RGBA_GETRED(rgba);
						*pDest++ = RGBA_GETGREEN(rgba);
						*pDest++ = RGBA_GETBLUE(rgba);
					}
					pSource += endOfLineSkip;
					pDest += destEndOfLineSkip;
				}
			}
			break;
		}
	}

	void cglRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z){
		SetRenderStateDirty();
		D3DXMATRIX m;
		D3DXVECTOR3 v;
		v.x = x;
		v.y = y;
		v.z = z;
		// GL uses counterclockwise degrees, DX uses clockwise radians
		float dxAngle = angle * 3.14159 / 180;
		m_currentMatrixStack->RotateAxisLocal(&v, dxAngle);
		*m_currentMatrixStateDirty = true;
	}

	void cglScalef (GLfloat x, GLfloat y, GLfloat z){
		SetRenderStateDirty();
		D3DXMATRIX m;
		qD3DXMatrixScaling(&m, x, y, z);
		m_currentMatrixStack->MultMatrixLocal(&m);
		*m_currentMatrixStateDirty = true;
	}

	void cglShadeModel (GLenum mode){
		if ( m_glShadeModel != mode ) {
			SetRenderStateDirty();
			m_glShadeModel = mode;
			m_glShadeModelStateDirty = true;
		}
	}

	inline void cglTexCoord2f (GLfloat s, GLfloat t){
		m_OGLPrimitiveVertexBuffer.SetTextureCoord0(s, t);
	}

	void cglTexEnvf (GLenum /* target */, GLenum /* pname */, GLfloat param){
		// ignore target, which must be GL_TEXTURE_ENV
		// ignore pname, which must be GL_TEXTURE_ENV_MODE
		if ( m_textureState.GetTextEnvMode() != param ) {
			SetRenderStateDirty();
			m_textureState.SetTextEnvMode(param);
		}
	}

	static int MipMapSize(DWORD width, DWORD height){
		DWORD n = width < height? width : height;
		DWORD result = 1;
		while (n > (DWORD) (1 << result) ) {
			result++;
		}
		return result;
	}

#define LOAD_OURSELVES

	void cglTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width,
		GLsizei height, GLint /* border */, GLenum format, GLenum type, const GLvoid *pixels){
		HRESULT hr;
		if ( target != GL_TEXTURE_2D || type != GL_UNSIGNED_BYTE) {
			InterpretError(E_FAIL);
			return;
		}

		bool isDynamic = format == GL_LUMINANCE; // Lightmaps use this format.

		DWORD dxWidth = width;
		DWORD dxHeight = height;

		D3DX_SURFACEFORMAT srcPixelFormat = GLToDXPixelFormat(internalformat, format);
		D3DX_SURFACEFORMAT destPixelFormat = srcPixelFormat;
		// Can the surface handle that format?
		hr = qD3DXCheckTextureRequirements(m_pD3DDev, NULL, &dxWidth, &dxHeight, &destPixelFormat);
		if ( FAILED(hr) ) {
			if ( g_force16bitTextures ) {
				destPixelFormat = D3DX_SF_A4R4G4B4;
				hr = qD3DXCheckTextureRequirements(m_pD3DDev, NULL, NULL, NULL, &destPixelFormat);
				if ( FAILED(hr) ) {
					// Don't know what to do.
					InterpretError(E_FAIL);
					return;
				}
			}
			else {
				destPixelFormat = D3DX_SF_A8R8G8B8;
				hr = qD3DXCheckTextureRequirements(m_pD3DDev, NULL, NULL, NULL, &destPixelFormat);
				if ( FAILED(hr) ) {
					// The card can't handle this pixel format. Switch to D3DX_SF_A4R4G4B4
					destPixelFormat = D3DX_SF_A4R4G4B4;
					hr = qD3DXCheckTextureRequirements(m_pD3DDev, NULL, NULL, NULL, &destPixelFormat);
					if ( FAILED(hr) ) {
						// Don't know what to do.
						InterpretError(E_FAIL);
						return;
					}
				}
			}
		}

#ifdef LOAD_OURSELVES

		char* goodSizeBits = (char*) pixels;
		if ( dxWidth != (DWORD) width || dxHeight != (DWORD) height ) {
			// Most likely this is because there is a 256 x 256 limit on the texture size.
			goodSizeBits = new char[sizeof(DWORD) * dxWidth * dxHeight]; 
			DWORD* dest = ((DWORD*) goodSizeBits);
			for ( DWORD y = 0; y < dxHeight; y++) {
				DWORD sy = y * height / dxHeight;
				for(DWORD x = 0; x < dxWidth; x++) {
					DWORD sx = x * width / dxWidth;
					DWORD* source = ((DWORD*) pixels) + sy * dxWidth + sx;
					*dest++ = *source;
				}
			}
			width = dxWidth;
			height = dxHeight;
		}
		// To do: Convert the pixels on the fly while copying into the DX texture.
		char* compatablePixels;
		DWORD compatablePixelsPitch;

		hr = ConvertToCompatablePixels(internalformat, width, height, format,
				type, destPixelFormat, goodSizeBits, &compatablePixels, &compatablePixelsPitch);

		if ( goodSizeBits != pixels ) {
			delete [] goodSizeBits;
		}
		if ( FAILED(hr)) {
			InterpretError(hr);
			return;
		}

#endif

		// It the current texture of the right size?
		LPDIRECTDRAWSURFACE7 pTexture = m_textures.GetTexture();
		if ( pTexture ) {
			DDSURFACEDESC2 surface;
			memset(&surface, 0, sizeof(surface));
			surface.dwSize = sizeof(surface);
			hr = pTexture->GetSurfaceDesc(&surface);
			if ( FAILED(hr) ) {
				InterpretError(hr);
				return;
			}
			// Is this texture being resized or re-color-formatted?
			if ( level == 0 && 
				( surface.dwWidth != (DWORD) width || surface.dwHeight != (DWORD) height
					|| destPixelFormat != m_textures.GetCurrentEntry()->m_format)) {
				m_textures.SetTexture(NULL, D3DX_SF_UNKNOWN, 0);
				pTexture = 0;
			}
			// For non-square textures, OpenGL uses more MIPMAP levels than DirectX does.
			else if ( (surface.dwWidth >> level) <= 0 || (surface.dwHeight >> level) <= 0 ) {
				return;
			}
		}

		if( ! pTexture) {
#ifdef USE_D3DXCREATETEXTURE
			DWORD dxwidth = width;
			DWORD dxheight = height;
			D3DX_SURFACEFORMAT pixelFormat = destPixelFormat;
			DWORD numMapsGenerated = 0;
			hr = D3DXCreateTexture(m_pD3DDev, NULL, &dxwidth, &dxheight, &pixelFormat,
				NULL, &pTexture, &numMapsGenerated);
			if ( FAILED(hr) ) {
				InterpretError(hr);
				return;
			}
#else
			DDSURFACEDESC2 sd = {sizeof(sd)};
			D3DX_SURFACEFORMAT pixelFormat = destPixelFormat;
			sd.dwFlags = DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|
                           DDSD_PIXELFORMAT;
			sd.dwHeight = dxHeight;
			sd.dwWidth = dxWidth;
			qD3DXMakeDDPixelFormat(pixelFormat, &sd.ddpfPixelFormat);
			sd.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
			if ( m_hintGenerateMipMaps ) {
				sd.ddsCaps.dwCaps |= DDSCAPS_MIPMAP|DDSCAPS_COMPLEX;
			}
			sd.ddsCaps.dwCaps2 = DDSCAPS2_TEXTUREMANAGE;
			if ( isDynamic ) {
				sd.ddsCaps.dwCaps2 |= DDSCAPS2_HINTDYNAMIC;
			}
			else {
				sd.ddsCaps.dwCaps2 |= DDSCAPS2_OPAQUE; // DDSCAPS2_HINTSTATIC;
			}

			hr = m_pDD->CreateSurface(&sd, &pTexture, NULL);
			if ( FAILED(hr) ) {
				InterpretError(hr);
				return;
			}

			int bytesThisTexture = height * compatablePixelsPitch;
			if ( m_hintGenerateMipMaps ) {
				bytesThisTexture = bytesThisTexture * 4 / 3;
			}
			static int gNumBytesOfTextures = 0; // For debugging
			gNumBytesOfTextures += bytesThisTexture;
#endif
			m_textures.SetTexture(pTexture, pixelFormat, internalformat);
		}

#ifdef LOAD_OURSELVES

		glTexSubImage2D_Imp(pTexture, level, 0, 0, width, height, format, type, compatablePixels,
			compatablePixelsPitch);

#else
		// This function is useful because it can scale large textures to fit into smaller textures.
		hr = D3DXLoadTextureFromMemory(m_pD3DDev, pTexture, level, (void*) pixels, NULL, srcPixelFormat, D3DX_DEFAULT,
			NULL, D3DX_FT_DEFAULT);
#endif

  		if ( FAILED(hr) ) {
			InterpretError(hr);
			return;
		}
	}

	void cglTexParameterf (GLenum target, GLenum pname, GLfloat param){

		switch(target){
		case GL_TEXTURE_2D:
			{
				SetRenderStateDirty();
				TextureEntry* current = m_textures.GetCurrentEntry();
				m_textureState.DirtyTexture(m_textures.GetCurrentID());
				switch(pname) {
				case GL_TEXTURE_MIN_FILTER:
					current->m_glTexParameter2DMinFilter = param;
					break;
				case GL_TEXTURE_MAG_FILTER:
					current->m_glTexParameter2DMagFilter = param;
					break;
				case GL_TEXTURE_WRAP_S:
					current->m_glTexParameter2DWrapS = param;
					break;
				case GL_TEXTURE_WRAP_T:
					current->m_glTexParameter2DWrapT = param;
					break;
				case GL_TEXTURE_MAX_ANISOTROPY_EXT:
					current->m_maxAnisotropy = param;
					break;
				default:
					LocalDebugBreak();
				}
			}
			break;
		default:
			LocalDebugBreak();
			break;
		}
	}

	void cglTexSubImage2D (GLenum target, GLint level,
		GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
		GLenum format, GLenum type, const GLvoid *pixels){
		if ( target != GL_TEXTURE_2D ) {
			LocalDebugBreak();
			return;
		}
		if ( width <= 0 || height <= 0 ) {
			return;
		}

		LPDIRECTDRAWSURFACE7 pTexture = m_textures.GetTexture();
		if ( ! pTexture ) {
			return;
		}

		internalEnd(); // We may have a pending drawing using the old texture state.

		// To do: Convert the pixels on the fly while copying into the DX texture.

		char* compatablePixels = 0;
		DWORD compatablePixelsPitch;
		if ( FAILED(ConvertToCompatablePixels(m_textures.GetInternalFormat(),
				width, height,
				format, type, m_textures.GetSurfaceFormat(),
				pixels, &compatablePixels, &compatablePixelsPitch))) {
			LocalDebugBreak();
			return;
		}

		glTexSubImage2D_Imp(pTexture, level, xoffset, yoffset, width, height, format, type,
			compatablePixels, compatablePixelsPitch);
	}

	char* StickyAlloc(DWORD size){
		if ( m_stickyAllocSize < size ) {
			delete [] m_stickyAlloc;
			m_stickyAlloc = new char[size];
			m_stickyAllocSize = size;
		}
		return m_stickyAlloc;
	}

	void glTexSubImage2D_Imp (LPDIRECTDRAWSURFACE7 pTexture, GLint level,
		GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
		GLenum /* format */, GLenum /* type */, const char* compatablePixels, int compatablePixelsPitch){

		HRESULT hr = S_OK;

		// Walk MIPMAP chain

		LPDIRECTDRAWSURFACE7 lpDDLevel;
		
		{
			LPDIRECTDRAWSURFACE7 lpDDNextLevel; 
			DDSCAPS2 ddsCaps; 
 
			lpDDLevel = pTexture; 
			lpDDLevel->AddRef();
			memset(&ddsCaps, 0, sizeof(ddsCaps));
			ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP; 
			hr = DD_OK;
			while (hr == DD_OK && level > 0) 
			{ 
				hr = lpDDLevel->GetAttachedSurface(&ddsCaps, &lpDDNextLevel); 
				lpDDLevel->Release(); 
				lpDDLevel = lpDDNextLevel;
				level--;
			}
		}

		if ( FAILED(hr) ) {
			InterpretError(hr);
			RELEASENULL(lpDDLevel);
			return;
		}

		DDSURFACEDESC2 surfaceDesc;
		memset(&surfaceDesc, 0, sizeof(DDSURFACEDESC2));
		surfaceDesc.dwSize = sizeof(DDSURFACEDESC2);
		RECT lockRect;
		lockRect.top = yoffset;
		lockRect.left = xoffset;
		lockRect.bottom = yoffset + height;
		lockRect.right = xoffset + width;
		hr = lpDDLevel->Lock(&lockRect, &surfaceDesc,
			DDLOCK_NOSYSLOCK|DDLOCK_WAIT|DDLOCK_WRITEONLY, NULL);
		if ( FAILED(hr) ) {
			InterpretError(hr);
		}
		else {
			const char* sp = compatablePixels;
			char* dp = (char*) surfaceDesc.lpSurface;
			if ( compatablePixelsPitch != surfaceDesc.lPitch ) {
				for(int i = 0; i < height; i++ ) {
					memcpy(dp, sp, compatablePixelsPitch);
					sp += compatablePixelsPitch;
					dp += surfaceDesc.lPitch;
				}
			}
			else {
				memcpy(dp, sp, compatablePixelsPitch * height);
			}
			lpDDLevel->Unlock(&lockRect);
		}

		RELEASENULL(lpDDLevel);

		if ( FAILED(hr) ) {
			InterpretError(hr);
		}
	}

	void cglTranslatef (GLfloat x, GLfloat y, GLfloat z){
		SetRenderStateDirty();
		D3DXMATRIX m;
		qD3DXMatrixTranslation(&m, x, y, z);
		m_currentMatrixStack->MultMatrixLocal(&m);
		*m_currentMatrixStateDirty = true;
	}

	inline void cglVertex2f (GLfloat x, GLfloat y){
		m_OGLPrimitiveVertexBuffer.SetVertex(x, y, 0);
	}

	inline void cglVertex3f (GLfloat x, GLfloat y, GLfloat z){
		m_OGLPrimitiveVertexBuffer.SetVertex(x, y, z);
	}

	inline void cglVertex3fv (const GLfloat *v){
		m_OGLPrimitiveVertexBuffer.SetVertex(v[0], v[1], v[2]);
	}

	void cglViewport (GLint x, GLint y, GLsizei width, GLsizei height){
		if ( m_glViewPortX != x || m_glViewPortY != y ||
			m_glViewPortWidth != width || m_glViewPortHeight != height ) {
			SetRenderStateDirty();
			m_glViewPortX = x;
			m_glViewPortY = y;
			m_glViewPortWidth = width;
			m_glViewPortHeight = height;

			m_bViewPortDirty = true;
		}
	}

	void SwapBuffers(){
		HRESULT hr = S_OK;
		internalEnd();
		m_pD3DDev->EndScene();
		m_needBeginScene = true;
#ifdef USE_D3DXCONTEXT
		hr = m_pD3DX->UpdateFrame( D3DX_UPDATE_NOVSYNC );
		if ( hr == DDERR_SURFACELOST || hr == DDERR_SURFACEBUSY )
			hr = HandleWindowedModeChanges();
#endif
#ifdef USE_D3DFRAME
		if( FAILED( hr = m_pFramework->ShowFrame() ) )
		{
			if( DDERR_SURFACELOST != hr )
				return;

			m_pFramework->RestoreSurfaces();
			RestoreSurfaces();
		}
#endif
	}

	void SetGammaRamp(const unsigned char* gammaTable){
		DDCAPS caps = {sizeof(DDCAPS)};
		HRESULT hr;
		hr = m_pDD->GetCaps(&caps, NULL);
		if ( caps.dwCaps2 & DDCAPS2_PRIMARYGAMMA ) {
			DDGAMMARAMP gammaRamp;
			for(int i = 0; i < 256; i++ ) {
				WORD value = gammaTable[i];
				value = value + (value << 8); // * 257
				gammaRamp.red[i] = value;
				gammaRamp.green[i] = value;
				gammaRamp.blue[i] = value;
			}
/*
			if(m_pPrimary) {
				IDirectDrawGammaControl* lpDDGammaControl = 0;
				hr = m_pPrimary->QueryInterface(IID_IDirectDrawGammaControl,(void**)&lpDDGammaControl);
				if ( ! FAILED(hr) && lpDDGammaControl ) {
					DWORD dwFlags = 0;
					if ( caps.dwCaps2 & DDCAPS2_CANCALIBRATEGAMMA ) {
						dwFlags = DDSGR_CALIBRATE;
					}
					hr = lpDDGammaControl->SetGammaRamp(dwFlags, &gammaRamp);
				
					RELEASENULL(lpDDGammaControl);
				}
			}
			*/
		}
	}

	void Hint_GenerateMipMaps(int value){
		m_hintGenerateMipMaps = value != 0;
	}

	void EvictTextures(){
		m_pD3D->EvictManagedTextures();
	}
private:

	void SetRenderStateDirty(){
		if ( ! m_glRenderStateDirty ) {
			internalEnd();
			m_glRenderStateDirty = true;
		}
	}

	HRESULT HandleWindowedModeChanges()
	{
#ifdef USE_D3DFRAME
		return Change3DEnvironment();
#endif
		HRESULT hr;
		hr = m_pDD->TestCooperativeLevel();

		if( SUCCEEDED( hr ) )
		{
			// This means that mode changes had taken place, surfaces
			// were lost but still we are in the original mode, so we
			// simply restore all surfaces and keep going.
			if( FAILED( m_pDD->RestoreAllSurfaces() ) )
				return hr;
		}
		else if( hr == DDERR_WRONGMODE )
		{
			// This means that the desktop mode has changed
			// we can destroy and recreate everything back again.
			if(FAILED(hr = ReleaseD3DX()))
				return hr;
			if(FAILED(hr = InitD3DX()))
				return hr;
		}
		else if( hr == DDERR_EXCLUSIVEMODEALREADYSET )
		{
			// This means that some app took exclusive mode access
			// we need to sit in a loop till we get back to the right mode.
			do
			{
				Sleep( 500 );
			} while( DDERR_EXCLUSIVEMODEALREADYSET == 
					 (hr = m_pDD->TestCooperativeLevel()) );
			if( SUCCEEDED( hr ) )
			{
				// This means that the exclusive mode app relinquished its 
				// control and we are back to the safe mode, so simply restore
				if( FAILED( m_pDD->RestoreAllSurfaces() ) )
					return hr;
			}
			else if( DDERR_WRONGMODE == hr )
			{
				// This means that the exclusive mode app relinquished its 
				// control BUT we are back to some strange mode, so destroy
				// and recreate.
				if(FAILED(hr = ReleaseD3DX()))
					return hr;
				if(FAILED(hr = InitD3DX()))
					return hr;
			}
			else
			{
				// Busted!!
				return hr;
			}
		}
		else
		{
			// Busted!!
			return hr;
		}
		return S_OK;
	}

	void SetGLRenderState(){
		if ( ! m_glRenderStateDirty ) {
			return;
		}
		m_glRenderStateDirty = false;
		HRESULT hr;
		if ( m_glAlphaStateDirty ){
			m_glAlphaStateDirty = false;
			// Alpha test
			m_pD3DDev->SetRenderState( D3DRENDERSTATE_ALPHATESTENABLE,
				m_glAlphaTest ? TRUE : FALSE );
			m_pD3DDev->SetRenderState(D3DRENDERSTATE_ALPHAFUNC,
				m_glAlphaTest ? GLToDXCompare(m_glAlphaFunc) : D3DCMP_ALWAYS);
			m_pD3DDev->SetRenderState(D3DRENDERSTATE_ALPHAREF, 255 * m_glAlphaFuncRef);
		}
		if ( m_glBlendStateDirty ){
			m_glBlendStateDirty = false;
			// Alpha blending
			DWORD srcBlend = m_glBlend ? GLToDXSBlend(m_glBlendFuncSFactor) : D3DBLEND_ONE;
			DWORD destBlend = m_glBlend ? GLToDXDBlend(m_glBlendFuncDFactor) : D3DBLEND_ZERO;
			m_pD3DDev->SetRenderState( D3DRENDERSTATE_SRCBLEND,  srcBlend );
			m_pD3DDev->SetRenderState( D3DRENDERSTATE_DESTBLEND, destBlend );
			m_pD3DDev->SetRenderState( D3DRENDERSTATE_ALPHABLENDENABLE, m_glBlend ? TRUE : FALSE );
		}
		if ( m_glCullStateDirty ) {
			m_glCullStateDirty = false;
			D3DCULL cull = D3DCULL_NONE;
			if ( m_glCullFace ) {
				switch(m_glCullFaceMode){
				default:
				case GL_BACK:
					// Should deal with frontface function
					cull = D3DCULL_CCW;
					break;
				}
			}
			hr = m_pD3DDev->SetRenderState(D3DRENDERSTATE_CULLMODE, cull);
			if ( FAILED(hr) ){
				InterpretError(hr);
			}
		}
		if ( m_glShadeModelStateDirty ){
			m_glShadeModelStateDirty = false;
			// Shade model
			m_pD3DDev->SetRenderState( D3DRENDERSTATE_SHADEMODE, 
				m_glShadeModel == GL_SMOOTH ? D3DSHADE_GOURAUD : D3DSHADE_FLAT );
		}

		{
			m_textureState.SetTextureStageState(m_pD3DDev, &m_textures);
		}

		if ( m_glDepthStateDirty ) {
			m_glDepthStateDirty = false;
			m_pD3DDev->SetRenderState( D3DRENDERSTATE_ZENABLE, m_glDepthTest ? D3DZB_TRUE : D3DZB_FALSE);
			m_pD3DDev->SetRenderState( D3DRENDERSTATE_ZWRITEENABLE, m_glDepthMask ? TRUE : FALSE);
			DWORD zfunc = GLToDXCompare(m_glDepthFunc);
			m_pD3DDev->SetRenderState( D3DRENDERSTATE_ZFUNC, zfunc );
		}
		if ( m_modelViewMatrixStateDirty ) {
			m_modelViewMatrixStateDirty = false;
			m_pD3DDev->SetTransform( D3DTRANSFORMSTATE_WORLD, (LPD3DMATRIX) m_modelViewMatrixStack->GetTop() );
		}
		if ( m_viewMatrixStateDirty ) {
			m_viewMatrixStateDirty = false;
			m_pD3DDev->SetTransform( D3DTRANSFORMSTATE_VIEW,  (LPD3DMATRIX) & m_d3dViewMatrix );
		}
		if ( m_projectionMatrixStateDirty ) {
			m_projectionMatrixStateDirty = false;
			m_pD3DDev->SetTransform( D3DTRANSFORMSTATE_PROJECTION, (LPD3DMATRIX) m_projectionMatrixStack->GetTop() );
		}
		if ( m_textureMatrixStateDirty ) {
			m_textureMatrixStateDirty = false;
			m_pD3DDev->SetTransform( D3DTRANSFORMSTATE_TEXTURE0, (LPD3DMATRIX) m_textureMatrixStack->GetTop() );
		}
		if ( m_bViewPortDirty ) {
			m_bViewPortDirty = false;
			D3DVIEWPORT7 viewData;
			viewData.dwX = m_glViewPortX;
			viewData.dwY = m_windowHeight - (m_glViewPortY + m_glViewPortHeight);
			viewData.dwWidth  = m_glViewPortWidth;
			viewData.dwHeight = m_glViewPortHeight;
			viewData.dvMinZ = m_glDepthRangeNear;     
			viewData.dvMaxZ = m_glDepthRangeFar;

			if (r_secondaryview)
			{
				m_pD3DDev->EndScene();
				m_needBeginScene = true;
			}

			m_pD3DDev->SetViewport(&viewData);
		}
	}

	void EnsureDriverInfo() {
		if ( ! m_vendor ) {
			memset(&m_dddi, 0, sizeof(m_dddi));
			m_pDD->GetDeviceIdentifier(&m_dddi, 0);
			m_vendor = m_dddi.szDriver;
			m_renderer = m_dddi.szDescription;
			wsprintf(m_version, "%u.%u.%u.%u %u.%u.%u.%u %u", 
				HIWORD(m_dddi.liDriverVersion.HighPart),
				LOWORD(m_dddi.liDriverVersion.HighPart),
				HIWORD(m_dddi.liDriverVersion.LowPart),
				LOWORD(m_dddi.liDriverVersion.LowPart),
				m_dddi.dwVendorId,
				m_dddi.dwDeviceId,
				m_dddi.dwSubSysId,
				m_dddi.dwRevision,
				m_dddi.dwWHQLLevel
				);
			if ( m_textureState.GetMaxStages() > 1 ) {
				m_extensions = " GL_SGIS_multitexture GL_EXT_texture_object ";
			}
			else {
				m_extensions = " GL_EXT_texture_object ";
			}
		}
	}

	D3DX_SURFACEFORMAT GLToDXPixelFormat(GLint internalformat, GLenum format){
		D3DX_SURFACEFORMAT d3dFormat = D3DX_SF_UNKNOWN;
		if ( g_force16bitTextures ) {
			switch ( format ) {
			case GL_RGBA:
				switch ( internalformat ) {
				default:
				case 4:
//					d3dFormat = D3DX_SF_A1R5G5B5; break;
					d3dFormat = D3DX_SF_A4R4G4B4; break;
				case 3:
					d3dFormat = D3DX_SF_R5G6B5; break;
				}
				break;
			case GL_RGB: d3dFormat = D3DX_SF_R5G5B5; break;
			case GL_COLOR_INDEX: d3dFormat = D3DX_SF_PALETTE8; break;
			case GL_LUMINANCE: d3dFormat = D3DX_SF_L8; break;
			case GL_ALPHA: d3dFormat = D3DX_SF_A8; break;
			case GL_INTENSITY: d3dFormat = D3DX_SF_L8; break;
			case GL_RGBA4: d3dFormat = D3DX_SF_A4R4G4B4; break;
			default:
				InterpretError(E_FAIL);
			}
		}
		else {
			// for
			switch ( format ) {
			case GL_RGBA:
				switch ( internalformat ) {
				default:
				case 4:
					d3dFormat = D3DX_SF_A8R8G8B8; break;
				case 3:
					d3dFormat = D3DX_SF_X8R8G8B8; break;
				}
				break;
			case GL_RGB:
				d3dFormat = D3DX_SF_R8G8B8;
				break;
			case GL_COLOR_INDEX: d3dFormat = D3DX_SF_PALETTE8; break;
			case GL_LUMINANCE: d3dFormat = D3DX_SF_L8; break;
			case GL_ALPHA: d3dFormat = D3DX_SF_A8; break;
			case GL_INTENSITY: d3dFormat = D3DX_SF_L8; break;
			case GL_RGBA4: d3dFormat = D3DX_SF_A4R4G4B4; break;
			default:
				InterpretError(E_FAIL);
			}
		}
		return d3dFormat;
	}

// Avoid warning 4061, enumerant 'foo' in switch of enum 'bar' is not explicitly handled by a case label.
#pragma warning( push )
#pragma warning( disable : 4061)

	HRESULT ConvertToCompatablePixels(GLint internalformat,
		GLsizei width, GLsizei height,
		GLenum format, GLenum type,
		D3DX_SURFACEFORMAT dxPixelFormat,
		const GLvoid *pixels, char**  compatablePixels,
		DWORD* newPitch){
		HRESULT hr = S_OK;
		if ( type != GL_UNSIGNED_BYTE ) {
			return E_FAIL;
		}
		switch ( dxPixelFormat ) {
		default:
			LocalDebugBreak();
			break;
		case D3DX_SF_PALETTE8:
		case D3DX_SF_L8:
		case D3DX_SF_A8:
			{
				char* copy = StickyAlloc(width*height);
				memcpy(copy,pixels,width * height);
				*compatablePixels = copy;
				if ( newPitch ) {
					*newPitch = width;
				}
			}
			break;
		case D3DX_SF_A4R4G4B4:
			{
				int textureElementSize = 2;
				const unsigned char* glpixels = (const unsigned char*) pixels;
				char* dxpixels = StickyAlloc(textureElementSize * width * height);
				switch ( internalformat ) {
				default:
					LocalDebugBreak();
					break;
				case 1:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*) (dxpixels + (y*width+x)*textureElementSize);
								const unsigned char* sp = glpixels + (y*width+x);
								unsigned short v;
								unsigned short s = 0xf & (sp[0] >> 4);
								v = s; // blue
								v |= s << 4; // green
								v |= s << 8; // red
								v |= s << 12; // alpha
								*dp = v;
							}
						}
					}
					break;
				case 3:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*) (dxpixels + (y*width+x)*textureElementSize);
								const unsigned char* sp = glpixels + (y*width+x)*4;
								unsigned short v;
								v = (0xf & (sp[2] >> 4)); // blue
								v |= (0xf & (sp[1] >> 4)) << 4; // green
								v |= (0xf & (sp[0] >> 4)) << 8; // red
								v |= 0xf000; // alpha
								*dp = v;
							}
						}
					}
					break;
				case 4:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*)(dxpixels + (y*width+x)*textureElementSize);
								const unsigned char* sp = glpixels + (y*width+x)*4;
								unsigned short v;
								v = (0xf & (sp[2] >> 4)); // blue
								v |= (0xf & (sp[1] >> 4)) << 4; // green
								v |= (0xf & (sp[0] >> 4)) << 8; // red
								v |= (0xf & (sp[3] >> 4)) << 12; // alpha
								*dp = v;
							}
						}
					}
					break;
				}
				*compatablePixels = dxpixels;
				if ( newPitch ) {
					*newPitch = 2 * width;
				}
			}
			break;
		case D3DX_SF_R5G6B5:
			{
				int textureElementSize = 2;
				const char* glpixels = (const char*) pixels;
				char* dxpixels = StickyAlloc(textureElementSize * width * height);
				switch ( internalformat ) {
				default:
					LocalDebugBreak();
					break;
				case 1:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*) (dxpixels + (y*width+x)*textureElementSize);
								const char* sp = glpixels + (y*width+x);
								unsigned short v;
								v = (0x1f & (sp[0] >> 3)); // blue
								v |= (0x3f & (sp[0] >> 2)) << 5; // green
								v |= (0x1f & (sp[0] >> 3)) << 11; // red
								*dp = v;
							}
						}
					}
					break;
				case 3:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*) (dxpixels + (y*width+x)*textureElementSize);
								const char* sp = glpixels + (y*width+x)*4;
								unsigned short v;
								v = (0x1f & (sp[2] >> 3)); // blue
								v |= (0x3f & (sp[1] >> 2)) << 5; // green
								v |= (0x1f & (sp[0] >> 3)) << 11; // red
								*dp = v;
							}
						}
					}
					break;
				case 4:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*) (dxpixels + (y*width+x)*textureElementSize);
								const char* sp = glpixels + (y*width+x)*4;
								unsigned short v;
								v = (0x1f & (sp[2] >> 3)); // blue
								v |= (0x3f & (sp[1] >> 2)) << 5; // green
								v |= (0x1f & (sp[0] >> 3)) << 11; // red
								*dp = v;
							}
						}
					}
					break;
				}
				*compatablePixels = dxpixels;
				if ( newPitch ) {
					*newPitch = 2 * width;
				}
			}
			break;
		case D3DX_SF_R5G5B5:
			{
				int textureElementSize = 2;
				const char* glpixels = (const char*) pixels;
				char* dxpixels = StickyAlloc(textureElementSize * width * height);
				switch ( internalformat ) {
				default:
					LocalDebugBreak();
					break;
				case 1:
					{
#define RGBTOR5G5B5(R, G, B) (0x8000 |  (0x1f & ((B) >> 3)) | ((0x1f & ((G) >> 3)) << 5) | ((0x1f & ((R) >> 3)) << 10))
#define Y5TOR5G5B5(Y) (0x8000 | ((Y) << 10) | ((Y) << 5) | (Y))
						static const unsigned short table[32] = {
							Y5TOR5G5B5(0), Y5TOR5G5B5(1), Y5TOR5G5B5(2), Y5TOR5G5B5(3),
							Y5TOR5G5B5(4), Y5TOR5G5B5(5), Y5TOR5G5B5(6), Y5TOR5G5B5(7),
							Y5TOR5G5B5(8), Y5TOR5G5B5(9), Y5TOR5G5B5(10), Y5TOR5G5B5(11),
							Y5TOR5G5B5(12), Y5TOR5G5B5(13), Y5TOR5G5B5(14), Y5TOR5G5B5(15),
							Y5TOR5G5B5(16), Y5TOR5G5B5(17), Y5TOR5G5B5(18), Y5TOR5G5B5(19),
							Y5TOR5G5B5(20), Y5TOR5G5B5(21), Y5TOR5G5B5(22), Y5TOR5G5B5(23),
							Y5TOR5G5B5(24), Y5TOR5G5B5(25), Y5TOR5G5B5(26), Y5TOR5G5B5(27),
							Y5TOR5G5B5(28), Y5TOR5G5B5(29), Y5TOR5G5B5(30), Y5TOR5G5B5(31)
						};
						unsigned short* dp = (unsigned short*) dxpixels;
						const unsigned char* sp = (const unsigned char*) glpixels;
						int numPixels = height * width;
						int i = numPixels >> 2;
						while(i > 0) {
							*dp++ = table[(*sp++) >> 3];
							*dp++ = table[(*sp++) >> 3];
							*dp++ = table[(*sp++) >> 3];
							*dp++ = table[(*sp++) >> 3];
							--i;
						}

						i = numPixels & 3;
						while(i > 0) {
							*dp++ = table[(*sp++) >> 3];
							--i;
						}
					}
					break;
				case 3:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*) (dxpixels + (y*width+x)*textureElementSize);
								const unsigned char* sp = (const unsigned char*) glpixels + (y*width+x)*4;
								unsigned short v;
								v = (sp[2] >> 3); // blue
								v |= (sp[1] >> 3) << 5; // green
								v |= (sp[0] >> 3) << 10; // red
								v |= 0x8000; // alpha
								*dp = v;
							}
						}
					}
					break;
				case 4:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*) (dxpixels + (y*width+x)*textureElementSize);
								const unsigned char* sp = (const unsigned char*) glpixels + (y*width+x)*4;
								unsigned short v;
								v = (sp[2] >> 3); // blue
								v |= (sp[1] >> 3) << 5; // green
								v |= (sp[0] >> 3) << 10; // red
								v |= 0x8000; // alpha
								*dp = v;
							}
						}
					}
					break;
				}
				*compatablePixels = dxpixels;
				if ( newPitch ) {
					*newPitch = 2 * width;
				}
			}
			break;
		case D3DX_SF_A1R5G5B5:
			{
				int textureElementSize = 2;
				const char* glpixels = (const char*) pixels;
				char* dxpixels = StickyAlloc(textureElementSize * width * height);
				switch ( internalformat ) {
				default:
					LocalDebugBreak();
					break;
				case 1:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*) (dxpixels + (y*width+x)*textureElementSize);
								const char* sp = glpixels + (y*width+x);
								unsigned short v;
								v = (0x1f & (sp[0] >> 3)); // blue
								v |= (0x1f & (sp[0] >> 3)) << 5; // green
								v |= (0x1f & (sp[0] >> 3)) << 10; // red
								v |= (0x01 & (sp[0] >> 7)) << 15; // alpha
								*dp = v;
							}
						}
					}
					break;
				case 3:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*) (dxpixels + (y*width+x)*textureElementSize);
								const char* sp = glpixels + (y*width+x)*4;
								unsigned short v;
								v = (0x1f & (sp[2] >> 3)); // blue
								v |= (0x1f & (sp[1] >> 3)) << 5; // green
								v |= (0x1f & (sp[0] >> 3)) << 10; // red
								v |= 0x8000; // alpha
								*dp = v;
							}
						}
					}
					break;
				case 4:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned short* dp = (unsigned short*) (dxpixels + (y*width+x)*textureElementSize);
								const char* sp = glpixels + (y*width+x)*4;
								unsigned short v;
								v = (0x1f & (sp[2] >> 3)); // blue
								v |= (0x1f & (sp[1] >> 3)) << 5; // green
								v |= (0x1f & (sp[0] >> 3)) << 10; // red
								v |= (0x01 & (sp[3] >> 7)) << 15; // alpha
								*dp = v;
							}
						}
					}
					break;
				}
				*compatablePixels = dxpixels;
				if ( newPitch ) {
					*newPitch = 2 * width;
				}
			}
			break;
		case D3DX_SF_X8R8G8B8:
		case D3DX_SF_A8R8G8B8:
			{
				int textureElementSize = 4;
				const char* glpixels = (const char*) pixels;
				char* dxpixels = StickyAlloc(textureElementSize * width * height);
				switch ( internalformat ) {
				default:
					LocalDebugBreak();
					break;
				case 1:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								char* dp = dxpixels + (y*width+x)*textureElementSize;
								const char* sp = glpixels + (y*width+x);
								dp[0] = sp[0]; // blue
								dp[1] = sp[0]; // green
								dp[2] = sp[0]; // red
								dp[3] = sp[0];
							}
						}
					}
					break;
				case 3:
					if (format == GL_RGB)
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned char* dp = (unsigned char*) dxpixels + (y*width+x)*textureElementSize;
								const unsigned char* sp = (unsigned char*) glpixels + (y*width+x)*3;
								dp[0] = sp[2]; // blue
								dp[1] = sp[1]; // green
								dp[2] = sp[0]; // red
								dp[3] = 0xff;
							}
						}
					}
					else
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								unsigned char* dp = (unsigned char*) dxpixels + (y*width+x)*textureElementSize;
								const unsigned char* sp = (unsigned char*) glpixels + (y*width+x)*4;
								dp[0] = sp[2]; // blue
								dp[1] = sp[1]; // green
								dp[2] = sp[0]; // red
								dp[3] = 0xff;
							}
						}
					}
					break;
				case 4:
					{
						for(int y = 0; y < height; y++){
							for(int x = 0; x < width; x++){
								char* dp = dxpixels + (y*width+x)*textureElementSize;
								const char* sp = glpixels + (y*width+x)*4;
								dp[0] = sp[2]; // blue
								dp[1] = sp[1]; // green
								dp[2] = sp[0]; // red
								dp[3] = sp[3]; // alpha
							}
						}
					}
					break;
				}
				*compatablePixels = dxpixels;
				if ( newPitch ) {
					*newPitch = 4 * width;
				}
			}
		}

		return hr;
	}
};

#pragma warning( pop )

// TODO Fix this warning instead of disableing it
#pragma warning(disable:4273)

void APIENTRY D3DAlphaFunc (GLenum func, GLclampf ref){
	gFakeGL->cglAlphaFunc(func, ref);
}

void APIENTRY D3DBegin (GLenum mode){
	gFakeGL->cglBegin(mode);
}

void APIENTRY D3DBlendFunc (GLenum sfactor, GLenum dfactor){
	gFakeGL->cglBlendFunc(sfactor, dfactor);
}

void APIENTRY D3DClear (GLbitfield mask){
	gFakeGL->cglClear(mask);
}

void APIENTRY D3DClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha){
	gFakeGL->cglClearColor(red, green, blue, alpha);
}

void APIENTRY D3DColor3f (GLfloat red, GLfloat green, GLfloat blue){
	if (red > 1)	red = 1;
	if (green > 1)	green = 1;
	if (blue > 1)	blue = 1;
	if (red < 0)	red = 0;
	if (green < 0)	green = 0;
	if (blue < 0)	blue = 0;
	gFakeGL->cglColor3f(red, green, blue);
}

void APIENTRY D3DColor3ubv (const GLubyte *v){
	gFakeGL->cglColor3ubv(v);
}
void APIENTRY D3DColor3ub (GLubyte v1, GLubyte v2, GLubyte v3)
{
	gFakeGL->cglColor3f(v1/255.0, v2/255.0, v3/255.0);
}
void APIENTRY D3DColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha){
	if (red>1)	red = 1;
	if (green>1)	green = 1;
	if (blue>1)	blue = 1;
	if (alpha>1)	alpha = 1;
	if (red < 0)	red = 0;
	if (green < 0)	green = 0;
	if (blue < 0)	blue = 0;
	if (alpha < 0)	alpha = 0;
	gFakeGL->cglColor4f(red, green, blue, alpha);
}

void APIENTRY D3DColor4fv (const GLfloat *v){
	gFakeGL->cglColor4fv(v);
}

void APIENTRY D3DColor4ubv (const GLubyte *v)	//no bounds checking needed
{
	gFakeGL->cglColor4f(v[0]/255.0, v[1]/255.0, v[2]/255.0, v[3]/255.0);
}
void APIENTRY D3DColor4ub (GLubyte v1, GLubyte v2, GLubyte v3, GLubyte v4)
{
	gFakeGL->cglColor4f(v1/255.0, v2/255.0, v3/255.0, v4/255.0);
}

void APIENTRY D3DCullFace (GLenum mode){
	gFakeGL->cglCullFace(mode);
}

void APIENTRY D3DDepthFunc (GLenum func){
	gFakeGL->cglDepthFunc(func);
}

void APIENTRY D3DDepthMask (GLboolean flag){
	gFakeGL->cglDepthMask(flag);
}

void APIENTRY D3DDepthRange (GLclampd zNear, GLclampd zFar){
	gFakeGL->cglDepthRange(zNear, zFar);
}

void APIENTRY D3DDisable (GLenum cap){
	gFakeGL->cglDisable(cap);
}

void APIENTRY D3DDrawBuffer (GLenum mode){
	gFakeGL->cglDrawBuffer(mode);
}

void APIENTRY D3DEnable (GLenum cap){
	gFakeGL->cglEnable(cap);
}

void APIENTRY D3DEnd (void){
	return; // Does nothing
//	gFakeGL->glEnd();
}

void APIENTRY D3DFinish (void){
	gFakeGL->cglFinish();
}

void APIENTRY D3DFrustum (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar){
	gFakeGL->cglFrustum(left, right, bottom, top, zNear, zFar);
}

void APIENTRY D3DGetFloatv (GLenum pname, GLfloat *params){
	gFakeGL->cglGetFloatv(pname, params);
}

const GLubyte * APIENTRY D3DGetString (GLenum name){
	return gFakeGL->cglGetString(name);
}

void APIENTRY D3DHint (GLenum target, GLenum mode){
	gFakeGL->cglHint(target, mode);
}

void APIENTRY D3DLoadIdentity (void){
	gFakeGL->cglLoadIdentity();
}

void APIENTRY D3DLoadMatrixf (const GLfloat *m){
	gFakeGL->cglLoadMatrixf(m);
}

void APIENTRY D3DMatrixMode (GLenum mode){
	gFakeGL->cglMatrixMode(mode);
}

void APIENTRY D3DOrtho (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar){
	gFakeGL->cglOrtho(left, right, top, bottom, zNear, zFar);
}

void APIENTRY D3DPolygonMode (GLenum face, GLenum mode){
	gFakeGL->cglPolygonMode(face, mode);
}

void APIENTRY D3DPopMatrix (void){
	gFakeGL->cglPopMatrix();
}

void APIENTRY D3DPushMatrix (void){
	gFakeGL->cglPushMatrix();
}

void APIENTRY D3DReadBuffer (GLenum mode){
	gFakeGL->cglReadBuffer(mode);
}

void APIENTRY D3DReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels){
	gFakeGL->cglReadPixels(x, y, width, height, format, type, pixels);
}

void APIENTRY D3DRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z){
	gFakeGL->cglRotatef(angle, x, y, z);
}

void APIENTRY D3DScalef (GLfloat x, GLfloat y, GLfloat z){
	gFakeGL->cglScalef(x, y, z);
}

void APIENTRY D3DShadeModel (GLenum mode){
	gFakeGL->cglShadeModel(mode);
}

void APIENTRY D3DTexCoord2f (GLfloat s, GLfloat t){
	gFakeGL->cglTexCoord2f(s, t);
}

void APIENTRY D3DTexEnvf (GLenum target, GLenum pname, GLfloat param){
	gFakeGL->cglTexEnvf(target, pname, param);
}

void APIENTRY D3DTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels){
	gFakeGL->cglTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);	
}

void APIENTRY D3DTexParameterf (GLenum target, GLenum pname, GLfloat param){
	gFakeGL->cglTexParameterf(target, pname, param);
}

void APIENTRY D3DTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels){
	gFakeGL->cglTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void APIENTRY D3DTranslatef (GLfloat x, GLfloat y, GLfloat z){
	gFakeGL->cglTranslatef(x, y, z);
}

void APIENTRY D3DVertex2f (GLfloat x, GLfloat y){
	gFakeGL->cglVertex2f(x, y);
}

void APIENTRY D3DVertex3f (GLfloat x, GLfloat y, GLfloat z){
	gFakeGL->cglVertex3f(x, y, z);
}

void APIENTRY D3DVertex3fv (const GLfloat *v){
	gFakeGL->cglVertex3fv(v);
}

void APIENTRY D3DViewport (GLint x, GLint y, GLsizei width, GLsizei height){
	gFakeGL->cglViewport(x, y, width, height);
}


int APIENTRY D3DGetError (void)
{
	return 0;
}

HDC gHDC;
HGLRC gHGLRC;

extern "C" {

extern HWND mainwindow;

};


HGLRC WINAPI D3DwglCreateContext(HDC /* hdc */){
	return (HGLRC) new FakeGL(mainwindow);
}

BOOL  WINAPI D3DwglDeleteContext(HGLRC hglrc){
	FakeGL* fgl = (FakeGL*) hglrc;
	delete fgl;
	return true;
}

HGLRC WINAPI D3DwglGetCurrentContext(VOID){
	return gHGLRC;
}

HDC   WINAPI D3DwglGetCurrentDC(VOID){
	return gHDC;
}

static void APIENTRY D3DBindTextureExt(GLenum target, GLuint texture){
	gFakeGL->cglBindTexture(target, texture);
}

static void APIENTRY D3DMTexCoord2fSGIS(GLenum target, GLfloat s, GLfloat t){
	gFakeGL->cglMTexCoord2fSGIS(target, s, t);
}

static void APIENTRY D3DSelectTextureSGIS(GLenum target){
	gFakeGL->cglSelectTextureSGIS(target);
}

// type cast unsafe conversion from 
#pragma warning( push )
#pragma warning( disable : 4191)

typedef struct {
	char *funcname;
	PROC functionp;
} d3dglfunc_t;
extern d3dglfunc_t glfuncs[];

PROC  WINAPI D3DwglGetProcAddress(LPCSTR s)
{
	int i;
	static LPCSTR kBindTextureEXT = "glBindTextureEXT";
	static LPCSTR kMTexCoord2fSGIS = "glMTexCoord2fSGIS"; // Multitexture
	static LPCSTR kSelectTextureSGIS = "glSelectTextureSGIS";
	if ( strcmp(s, kBindTextureEXT) == 0){
		return (PROC) D3DBindTextureExt;
	}
	else if ( strcmp(s, kMTexCoord2fSGIS) == 0){
		return (PROC) D3DMTexCoord2fSGIS;
	}
	else if ( strcmp(s, kSelectTextureSGIS) == 0){
		return (PROC) D3DSelectTextureSGIS;
	}
	for (i = 0; glfuncs[i].funcname; i++)
	{
		if (!strcmp(s, glfuncs[i].funcname))
			return glfuncs[i].functionp;
	}

	// LocalDebugBreak();
	return 0;
}

#pragma warning( pop )

BOOL  WINAPI D3DwglMakeCurrent(HDC hdc, HGLRC hglrc){
	gHDC = hdc;
	gHGLRC = hglrc;
	gFakeGL = (FakeGL*) hglrc;
	return TRUE;
}

extern "C"{

void d3dSetMode(int fullscreen, int width, int height, int bpp, int zbpp);
void d3dEvictTextures();
BOOL APIENTRY FakeSwapBuffers(HDC hdc);
void d3dSetGammaRamp(const unsigned char* gammaTable);
void d3dInitSetForce16BitTextures(int force16bitTextures);
void d3dHint_GenerateMipMaps(int value);
float d3dGetD3DDriverVersion();
void D3DInitialize(void);
};

void d3dEvictTextures(){
	gFakeGL->EvictTextures();
}

void d3dSetMode(int fullscreen, int width, int height, int bpp, int zbpp){
	gFullScreen = fullscreen != 0;
	gWidth = width;
	gHeight = height;
	gBpp = bpp;
	gZbpp = zbpp;
}

BOOL APIENTRY FakeSwapBuffers(HDC hdc){
	if ( ! gFakeGL ) {
		return false;
	}
	gFakeGL->SwapBuffers();

	return true;
}

void d3dSetGammaRamp(const unsigned char* gammaTable){
	gFakeGL->SetGammaRamp(gammaTable);
}

void d3dInitSetForce16BitTextures(int force16bitTextures){
	// called before gFakeGL exits. That's why we set a global
	g_force16bitTextures = force16bitTextures != 0;
}

void d3dHint_GenerateMipMaps(int value){
	gFakeGL->Hint_GenerateMipMaps(value);
}

float d3dGetD3DDriverVersion(){
	return 0.73f;
}

void APIENTRY D3DTexCoord2fv(const GLfloat *f)
{
	D3DTexCoord2f(f[0], f[1]);
}
void APIENTRY D3DTexCoord1f(GLfloat f)
{
	D3DTexCoord2f(f, f);
}
void APIENTRY D3DTexParameteri (GLenum target, GLenum pname, GLint param)
{
	D3DTexParameterf(target, pname, param);
}
void APIENTRY D3DTexEnvi (GLenum target, GLenum pname, GLint param)
{
	D3DTexEnvf(target, pname, param);
}
void APIENTRY D3DMultMatrixf (const GLfloat *m)
{
	gFakeGL->cglMultMatrixf(m);
}

void APIENTRY D3DNormal3f(GLfloat x, GLfloat y, GLfloat z)
{}
void APIENTRY D3DNormal3fv (const GLfloat *v)
{D3DNormal3f(v[0], v[1], v[2]);}
void APIENTRY D3DFogf (GLenum pname, GLfloat param)
{}
void APIENTRY D3DFogi (GLenum pname, GLint param)
{}
void APIENTRY D3DFogfv (GLenum pname, const GLfloat *params)
{}
void APIENTRY D3DGetIntegerv (GLenum pname, GLint *params)
{
	switch(pname)
	{
	case GL_MAX_TEXTURE_SIZE:
		params[0]=2048;
		break;
	case GL_MAX_TEXTURE_UNITS_ARB:
		params[0]=2;
		break;
	default:
		Sys_Error("Bad D3DGetIntegerv\n");
	}
}
void APIENTRY D3DNewList (GLuint list, GLenum mode)
{}
void APIENTRY D3DEndList (void)
{}
void APIENTRY D3DCallList (GLuint list)
{}
void APIENTRY D3DTexGeni (GLenum coord, GLenum pname, GLint param)
{}



int texarraystride;
bool texarrayenabled;
const float *texarray;

int vertarraystride;
bool vertarrayenabled;
const float *vertarray;

bool colourarrayenabled;
int colourarraystride;
const qbyte *colourarray;

void APIENTRY D3DDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	int *index;
	if (!texarrayenabled || !vertarrayenabled)
		return;	//please explain?

	D3DBegin(mode);
	if (colourarrayenabled)
	{
		for (index = (int*)indices; count--; index++)
		{
			D3DTexCoord2fv(texarray + *index*texarraystride);
			D3DColor4ubv(colourarray + *index*colourarraystride);
			D3DVertex3fv(vertarray + *index*vertarraystride);
		}

	}
	else
	{
		for (index = (int*)indices; count--; index++)
		{
			D3DTexCoord2fv(texarray + *index*texarraystride);
			D3DVertex3fv(vertarray + *index*vertarraystride);
		}
	}
	D3DEnd();
}
void APIENTRY D3DVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	vertarray = (float *)pointer;
	if (size != 3 || type != GL_FLOAT || (stride%4))
		Sys_Error("D3DVertexPointer is limited");

	if (!stride)
		stride = sizeof(float)*size;

	vertarraystride = stride/4;
}
void APIENTRY D3DTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	texarray = (float *)pointer;
	if (size != 2 || type != GL_FLOAT || (stride%4))
		Sys_Error("D3DTexCoordPointer is limited");

	if (!stride)
		stride = sizeof(float)*size;

	texarraystride = stride/4;
}
void APIENTRY D3DColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	colourarray = (qbyte *)pointer;
	if (size != 4 || type != GL_UNSIGNED_BYTE || (stride%4))
		Sys_Error("D3DColourPointer is limited");

	if (!stride)
		stride = sizeof(float)*size;

	colourarraystride = stride/4;
}
void APIENTRY D3DEnableClientState(unsigned int e)
{
	switch(e)
	{
	case GL_TEXTURE_COORD_ARRAY:
		texarrayenabled = true;
		break;
	case GL_COLOR_ARRAY:
		colourarrayenabled = true;
		break;
	case GL_VERTEX_ARRAY:
		vertarrayenabled = true;
		break;
	}
}
void APIENTRY D3DDisableClientState(unsigned int e)
{
	switch(e)
	{
	case GL_TEXTURE_COORD_ARRAY:
		texarrayenabled = false;
		break;
	case GL_COLOR_ARRAY:
		colourarrayenabled = false;
		break;
	case GL_VERTEX_ARRAY:
		vertarrayenabled = false;
		break;
	}
}




#if 1

#pragma comment(lib, "../libs/dxsdk7/lib/ddraw.lib")
#pragma comment(lib, "../libs/dxsdk7/lib/d3dx.lib")

#else
HMODULE ddrawdll;
#endif

void D3DInitialize(void)
{
#if 1
	qD3DXMatrixScaling				= D3DXMatrixScaling;
	qD3DXGetErrorString				= D3DXGetErrorString;
	qD3DXMatrixPerspectiveOffCenter	= D3DXMatrixPerspectiveOffCenter;
	qD3DXMatrixOrthoOffCenter		= D3DXMatrixOrthoOffCenter;
	qD3DXInitialize					= D3DXInitialize;
	qD3DXUninitialize				= D3DXUninitialize;
	qD3DXCreateContextEx			= D3DXCreateContextEx;
	qD3DXCreateMatrixStack			= D3DXCreateMatrixStack;
	qD3DXCheckTextureRequirements	= D3DXCheckTextureRequirements;
	qD3DXMakeDDPixelFormat			= D3DXMakeDDPixelFormat;
	qD3DXMatrixTranslation			= D3DXMatrixTranslation;
#else
	if (!ddrawdll)
		ddrawdll = LoadLibrary("d3drm.dll");	//yeah, right, these are staticly linked. DLLS get speed hits.
	qD3DXMatrixScaling				= (qD3DXMatrixScaling_t)				GetProcAddress(ddrawdll, "D3DXMatrixScaling");	
	qD3DXGetErrorString				= (qD3DXGetErrorString_t)				GetProcAddress(ddrawdll, "D3DXGetErrorString");	
	qD3DXMatrixPerspectiveOffCenter	= (qD3DXMatrixPerspectiveOffCenter_t)	GetProcAddress(ddrawdll, "D3DXMatrixPerspectiveOffCenter");
	qD3DXMatrixOrthoOffCenter		= (qD3DXMatrixOrthoOffCenter_t)			GetProcAddress(ddrawdll, "D3DXMatrixOrthoOffCenter");
	qD3DXInitialize					= (qD3DXInitialize_t)					GetProcAddress(ddrawdll, "D3DXInitialize");
	qD3DXUninitialize				= (qD3DXUninitialize_t)					GetProcAddress(ddrawdll, "D3DXUninitialize");
	qD3DXCreateContextEx			= (qD3DXCreateContextEx_t)				GetProcAddress(ddrawdll, "D3DXCreateContextEx");
	qD3DXCreateMatrixStack			= (qD3DXCreateMatrixStack_t)			GetProcAddress(ddrawdll, "D3DXCreateMatrixStack");
	qD3DXCheckTextureRequirements	= (qD3DXCheckTextureRequirements_t)		GetProcAddress(ddrawdll, "D3DXCheckTextureRequirements");
	qD3DXMakeDDPixelFormat			= (qD3DXMakeDDPixelFormat_t)			GetProcAddress(ddrawdll, "D3DXMakeDDPixelFormat");
	qD3DXMatrixTranslation			= (qD3DXMatrixTranslation_t)			GetProcAddress(ddrawdll, "D3DXMatrixTranslation");
#endif

	if (!qD3DXCreateMatrixStack || !qD3DXMatrixScaling || !qD3DXMatrixTranslation || !qD3DXMatrixPerspectiveOffCenter
		|| !qD3DXMatrixOrthoOffCenter || !qD3DXGetErrorString || !qD3DXInitialize || !qD3DXUninitialize
		|| !qD3DXCreateContextEx || !qD3DXCheckTextureRequirements || !qD3DXMakeDDPixelFormat)
		Sys_Error("You don't have directx 7");
/*
	qglAlphaFunc		= D3DAlphaFunc;
	qglBegin			= D3DBegin;
	qglBlendFunc		= D3DBlendFunc;
	qglClear			= D3DClear;
	qglClearColor		= D3DClearColor;
	qglColor3f			= D3DColor3f;
	qglColor3ub			= D3DColor3ub;
	qglColor4f			= D3DColor4f;
	qglColor4fv			= D3DColor4fv;
	qglColor4ub			= D3DColor4ub;
	qglColor4ubv		= D3DColor4ubv;
	qglCullFace			= D3DCullFace;
	qglDepthFunc		= D3DDepthFunc;
	qglDepthMask		= D3DDepthMask;
	qglDepthRange		= D3DDepthRange;
	qglDisable			= D3DDisable;
	qglDrawBuffer		= D3DDrawBuffer;
	qglEnable			= D3DEnable;
	qglEnd				= D3DEnd;
	qglFinish			= D3DFinish;
	qglFrustum			= D3DFrustum;
	qglGetFloatv		= D3DGetFloatv;
	qglGetIntegerv		= D3DGetIntegerv;
	qglGetString		= D3DGetString;
	qglHint				= D3DHint;
	qglLoadIdentity		= D3DLoadIdentity;
	qglLoadMatrixf		= D3DLoadMatrixf;
	qglNormal3f			= D3DNormal3f;
	qglNormal3fv		= D3DNormal3fv;
	qglMatrixMode		= D3DMatrixMode;
	qglMultMatrixf		= D3DMultMatrixf;
	qglOrtho			= D3DOrtho;
	qglPolygonMode		= D3DPolygonMode;
	qglPopMatrix		= D3DPopMatrix;
	qglPushMatrix		= D3DPushMatrix;
	qglReadBuffer		= D3DReadBuffer;
	qglReadPixels		= D3DReadPixels;
	qglRotatef			= D3DRotatef;
	qglScalef			= D3DScalef;
	qglShadeModel		= D3DShadeModel;
	qglTexCoord1f		= D3DTexCoord1f;
	qglTexCoord2f		= D3DTexCoord2f;
	qglTexCoord2fv		= D3DTexCoord2fv;
	qglTexEnvf			= D3DTexEnvf;
	qglTexEnvi			= D3DTexEnvi;
	qglTexGeni			= D3DTexGeni;
	qglTexImage2D		= D3DTexImage2D;
	qglTexParameteri	= D3DTexParameteri;
	qglTexParameterf	= D3DTexParameterf;
	qglTexSubImage2D	= D3DTexSubImage2D;
	qglTranslatef		= D3DTranslatef;
	qglVertex2f			= D3DVertex2f;
	qglVertex3f			= D3DVertex3f;
	qglVertex3fv		= D3DVertex3fv;
	qglViewport			= D3DViewport;

	qglDrawElements		= D3DDrawElements;
	qglVertexPointer	= D3DVertexPointer;
	qglTexCoordPointer	= D3DTexCoordPointer;
	qglEnableClientState	= D3DEnableClientState;
	qglDisableClientState	= D3DDisableClientState;
*/
	qwglCreateContext		= D3DwglCreateContext;
	qwglDeleteContext		= D3DwglDeleteContext;
	qwglGetCurrentContext	= D3DwglGetCurrentContext;
	qwglGetCurrentDC		= D3DwglGetCurrentDC;
	qwglGetProcAddress		= D3DwglGetProcAddress;
	qwglMakeCurrent			= D3DwglMakeCurrent;
	qSwapBuffers			= FakeSwapBuffers;
}




d3dglfunc_t glfuncs[] = {
	{"glAlphaFunc",		(PROC)D3DAlphaFunc},
	{"glBegin",			(PROC)D3DBegin},
	{"glBlendFunc",		(PROC)D3DBlendFunc},
	{"glClear",			(PROC)D3DClear},
	{"glClearColor",	(PROC)D3DClearColor},
	{"glClearDepth",	NULL},
	{"glClearStencil",	NULL},
	{"glColor3f",		(PROC)D3DColor3f},
	{"glColor3ub",		(PROC)D3DColor3ub},
	{"glColor4f",		(PROC)D3DColor4f},
	{"glColor4fv",		(PROC)D3DColor4fv},
	{"glColor4ub",		(PROC)D3DColor4ub},
	{"glColor4ubv",		(PROC)D3DColor4ubv},
	{"glColorMask",		NULL},//(PROC)D3DColorMask},
	{"glCullFace",		(PROC)D3DCullFace},
	{"glDepthFunc",		(PROC)D3DDepthFunc},
	{"glDepthMask",		(PROC)D3DDepthMask},
	{"glDepthRange",	(PROC)D3DDepthRange},
	{"glDisable",		(PROC)D3DDisable},
	{"glDrawBuffer",	(PROC)D3DDrawBuffer},
	{"glDrawPixels",	NULL},//(PROC)D3DDrawPixels},
	{"glEnable",		(PROC)D3DEnable},
	{"glEnd",			(PROC)D3DEnd},
	{"glFlush",			NULL},//(PROC)D3DFlush},
	{"glFinish",		(PROC)D3DFinish},
	{"glFrustum",		(PROC)D3DFrustum},
	{"glGetFloatv",		(PROC)D3DGetFloatv},
	{"glGetIntegerv",	(PROC)D3DGetIntegerv},
	{"glGetString",		(PROC)D3DGetString},



	{"glHint",			(PROC)D3DHint},
	{"glLoadIdentity",	(PROC)D3DLoadIdentity},
	{"glLoadMatrixf",	(PROC)D3DLoadMatrixf},
	{"glNormal3f",		(PROC)D3DNormal3f},
	{"glNormal3fv",		(PROC)D3DNormal3fv},
	{"glMatrixMode",	(PROC)D3DMatrixMode},
	{"glMultMatrixf",	(PROC)D3DMultMatrixf},
	{"glOrtho",			(PROC)D3DOrtho},
	{"glPolygonMode",	(PROC)D3DPolygonMode},
	{"glPopMatrix",		(PROC)D3DPopMatrix},
	{"glPushMatrix",	(PROC)D3DPushMatrix},
	{"glReadBuffer",	(PROC)D3DReadBuffer},
	{"glReadPixels",	(PROC)D3DReadPixels},
	{"glRotatef",		(PROC)D3DRotatef},
	{"glScalef",		(PROC)D3DScalef},
	{"glShadeModel",	(PROC)D3DShadeModel},
	{"glTexCoord1f",	(PROC)D3DTexCoord1f},
	{"glTexCoord2f",	(PROC)D3DTexCoord2f},
	{"glTexCoord2fv",	(PROC)D3DTexCoord2fv},
	{"glTexEnvf",		(PROC)D3DTexEnvf},
	{"glTexEnvi",		(PROC)D3DTexEnvi},
	{"glTexGeni",		(PROC)D3DTexGeni},
	{"glTexImage2D",	(PROC)D3DTexImage2D},
	{"glTexParameteri",	(PROC)D3DTexParameteri},
	{"glTexParameterf",	(PROC)D3DTexParameterf},
	{"glTexSubImage2D",	(PROC)D3DTexSubImage2D},
	{"glTranslatef",	(PROC)D3DTranslatef},
	{"glVertex2f",		(PROC)D3DVertex2f},
	{"glVertex3f",		(PROC)D3DVertex3f},
	{"glVertex3fv",		(PROC)D3DVertex3fv},
	{"glViewport",		(PROC)D3DViewport},

	{"glDrawElements",			(PROC)D3DDrawElements},
	{"glVertexPointer",			(PROC)D3DVertexPointer},
	{"glTexCoordPointer",		(PROC)D3DTexCoordPointer},
	{"glColorPointer",			(PROC)D3DColorPointer},
	{"glEnableClientState",		(PROC)D3DEnableClientState},
	{"glDisableClientState",	(PROC)D3DDisableClientState},

	{"glGetError",				(PROC)D3DGetError},
/*
	qwglCreateContext		= D3DwglCreateContext;
	qwglDeleteContext		= D3DwglDeleteContext;
	qwglGetCurrentContext	= D3DwglGetCurrentContext;
	qwglGetCurrentDC		= D3DwglGetCurrentDC;
	qwglGetProcAddress		= D3DwglGetProcAddress;
	qwglMakeCurrent			= D3DwglMakeCurrent;
	qSwapBuffers			= FakeSwapBuffers;*/
	{NULL}
};


qboolean D3DVID_Init(rendererstate_t *info, unsigned char *palette)
{
	strcpy(info->glrenderer, "D3D");
	return GLVID_Init(info, palette);
}

extern "C" {
#include "gl_draw.h"
}

rendererinfo_t d3drendererinfo = {
		"Direct3D",
		{
			"faked3d",
			"crap"
		},
		QR_OPENGL,


		GLDraw_SafePicFromWad,
		GLDraw_CachePic,
		GLDraw_SafeCachePic,
		GLDraw_Init,
		GLDraw_ReInit,
		GLDraw_Character,
		GLDraw_ColouredCharacter,
		GLDraw_TinyCharacter,
		GLDraw_String,
		GLDraw_Alt_String,
		GLDraw_Crosshair,
		GLDraw_DebugChar,
		GLDraw_Pic,
		GLDraw_ScalePic,
		GLDraw_SubPic,
		GLDraw_TransPic,
		GLDraw_TransPicTranslate,
		GLDraw_ConsoleBackground,
		GLDraw_EditorBackground,
		GLDraw_TileClear,
		GLDraw_Fill,
		GLDraw_FillRGB,
		GLDraw_FadeScreen,
		GLDraw_BeginDisc,
		GLDraw_EndDisc,

		GLDraw_Image,
		GLDraw_ImageColours,

		GLR_Init,
		GLR_DeInit,
		GLR_ReInit,
		GLR_RenderView,


		NULL,
		NULL,

		GLR_NewMap,
		GLR_PreNewMap,
		GLR_LightPoint,
		GLR_PushDlights,


		Surf_AddStain,
		Surf_LessenStains,

		MediaGL_ShowFrameBGR_24_Flip,
		MediaGL_ShowFrameRGBA_32,
		MediaGL_ShowFrame8bit,


		GLMod_Init,
		GLMod_ClearAll,
		GLMod_ForName,
		GLMod_FindName,
		GLMod_Extradata,
		GLMod_TouchModel,

		GLMod_NowLoadExternal,
		GLMod_Think,

		Mod_GetTag,
		Mod_TagNumForName,
		Mod_SkinForName,
		Mod_FrameForName,
		Mod_GetFrameDuration,

		D3DVID_Init,
		GLVID_DeInit,
		GLVID_LockBuffer,
		GLVID_UnlockBuffer,
		GLD_BeginDirectRect,
		GLD_EndDirectRect,
		GLVID_ForceLockState,
		GLVID_ForceUnlockedAndReturnState,
		GLVID_SetPalette,
		GLVID_ShiftPalette,
		GLVID_GetRGBInfo,

		NULL,	//setcaption


		GLSCR_UpdateScreen,

		""
};
extern "C" {
rendererinfo_t *pd3drendererinfo = &d3drendererinfo;
}

#endif

#endif

