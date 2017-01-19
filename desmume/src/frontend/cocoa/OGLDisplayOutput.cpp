/*
	Copyright (C) 2014-2017 DeSmuME team

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

#include "OGLDisplayOutput.h"
#include "cocoa_globals.h"
#include "utilities.h"
#include "../../utils/colorspacehandler/colorspacehandler.h"
#include "../../filter/videofilter.h"

#include <sstream>


// VERTEX SHADER FOR HUD OUTPUT
static const char *HUDOutputVertShader_100 = {"\
	ATTRIBUTE vec2 inPosition; \n\
	ATTRIBUTE vec2 inTexCoord0; \n\
	\n\
	uniform vec2 viewSize; \n\
	\n\
	VARYING vec2 texCoord[1]; \n\
	\n\
	void main() \n\
	{ \n\
		mat2 projection	= mat2(	vec2(2.0/viewSize.x,            0.0), \n\
								vec2(           0.0, 2.0/viewSize.y)); \n\
		\n\
		texCoord[0] = inTexCoord0; \n\
		gl_Position = vec4(projection * inPosition, 0.0, 1.0);\n\
	} \n\
"};

// FRAGMENT SHADER FOR HUD OUTPUT
static const char *HUDOutputFragShader_110 = {"\
	VARYING vec2 texCoord[1];\n\
	uniform sampler2D tex;\n\
	\n\
	void main()\n\
	{\n\
		OUT_FRAG_COLOR = SAMPLE4_TEX_2D(tex, texCoord[0]);\n\
	}\n\
"};

// VERTEX SHADER FOR DISPLAY OUTPUT
static const char *Sample1x1OutputVertShader_100 = {"\
	ATTRIBUTE vec2 inPosition; \n\
	ATTRIBUTE vec2 inTexCoord0; \n\
	\n\
	uniform vec2 viewSize; \n\
	uniform float scalar; \n\
	uniform float angleDegrees; \n\
	\n\
	VARYING vec2 texCoord[1]; \n\
	\n\
	void main() \n\
	{ \n\
		float angleRadians = radians(angleDegrees); \n\
		\n\
		mat2 projection	= mat2(	vec2(2.0/viewSize.x,            0.0), \n\
								vec2(           0.0, 2.0/viewSize.y)); \n\
		\n\
		mat2 rotation	= mat2(	vec2( cos(angleRadians), sin(angleRadians)), \n\
								vec2(-sin(angleRadians), cos(angleRadians))); \n\
		\n\
		mat2 scale		= mat2(	vec2(scalar,    0.0), \n\
								vec2(   0.0, scalar)); \n\
		\n\
		texCoord[0] = inTexCoord0; \n\
		gl_Position = vec4(projection * rotation * scale * inPosition, 0.0, 1.0);\n\
	} \n\
"};

static const char *BicubicSample4x4Output_VertShader_110 = {"\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08|09\n\
	//                       05|00|01|10\n\
	//                       04|03|02|11\n\
	//                       15|14|13|12\n\
	\n\
	ATTRIBUTE vec2 inPosition;\n\
	ATTRIBUTE vec2 inTexCoord0;\n\
	\n\
	uniform vec2 viewSize; \n\
	uniform float scalar; \n\
	uniform float angleDegrees; \n\
	\n\
	VARYING vec2 texCoord[16];\n\
	\n\
	void main()\n\
	{\n\
		float angleRadians = radians(angleDegrees); \n\
		\n\
		mat2 projection	= mat2(	vec2(2.0/viewSize.x,            0.0), \n\
								vec2(           0.0, 2.0/viewSize.y)); \n\
		\n\
		mat2 rotation	= mat2(	vec2( cos(angleRadians), sin(angleRadians)), \n\
								vec2(-sin(angleRadians), cos(angleRadians))); \n\
		\n\
		mat2 scale		= mat2(	vec2(scalar,    0.0), \n\
								vec2(   0.0, scalar)); \n\
		\n\
		vec2 xystart = floor(inTexCoord0 - 0.5) + 0.5;\n\
		\n\
		texCoord[ 6] = xystart + vec2(-1.0,-1.0);\n\
		texCoord[ 7] = xystart + vec2( 0.0,-1.0);\n\
		texCoord[ 8] = xystart + vec2( 1.0,-1.0);\n\
		texCoord[ 9] = xystart + vec2( 2.0,-1.0);\n\
		\n\
		texCoord[ 5] = xystart + vec2(-1.0, 0.0);\n\
		texCoord[ 0] = xystart + vec2( 0.0, 0.0); // Center pixel\n\
		texCoord[ 1] = xystart + vec2( 1.0, 0.0);\n\
		texCoord[10] = xystart + vec2( 2.0, 0.0);\n\
		\n\
		texCoord[ 4] = xystart + vec2(-1.0, 1.0);\n\
		texCoord[ 3] = xystart + vec2( 0.0, 1.0);\n\
		texCoord[ 2] = xystart + vec2( 1.0, 1.0);\n\
		texCoord[11] = xystart + vec2( 2.0, 1.0);\n\
		\n\
		texCoord[15] = xystart + vec2(-1.0, 2.0);\n\
		texCoord[14] = xystart + vec2( 0.0, 2.0);\n\
		texCoord[13] = xystart + vec2( 1.0, 2.0);\n\
		texCoord[12] = xystart + vec2( 2.0, 2.0);\n\
		\n\
		gl_Position = vec4(projection * rotation * scale * inPosition, 0.0, 1.0);\n\
	}\n\
"};

static const char *BicubicSample5x5Output_VertShader_110 = {"\
	//---------------------------------------\n\
	// Input Pixel Mapping:  20|21|22|23|24\n\
	//                       19|06|07|08|09\n\
	//                       18|05|00|01|10\n\
	//                       17|04|03|02|11\n\
	//                       16|15|14|13|12\n\
	\n\
	ATTRIBUTE vec2 inPosition;\n\
	ATTRIBUTE vec2 inTexCoord0;\n\
	\n\
	uniform vec2 viewSize; \n\
	uniform float scalar; \n\
	uniform float angleDegrees; \n\
	\n\
	VARYING vec2 texCoord[25];\n\
	\n\
	void main()\n\
	{\n\
		float angleRadians = radians(angleDegrees); \n\
		\n\
		mat2 projection	= mat2(	vec2(2.0/viewSize.x,            0.0), \n\
								vec2(           0.0, 2.0/viewSize.y)); \n\
		\n\
		mat2 rotation	= mat2(	vec2( cos(angleRadians), sin(angleRadians)), \n\
								vec2(-sin(angleRadians), cos(angleRadians))); \n\
		\n\
		mat2 scale		= mat2(	vec2(scalar,    0.0), \n\
								vec2(   0.0, scalar)); \n\
		\n\
		vec2 xystart = floor(inTexCoord0 - 0.5) + 0.5;\n\
		\n\
		texCoord[20] = xystart + vec2(-2.0,-2.0);\n\
		texCoord[21] = xystart + vec2(-1.0,-2.0);\n\
		texCoord[22] = xystart + vec2( 0.0,-2.0);\n\
		texCoord[23] = xystart + vec2( 1.0,-2.0);\n\
		texCoord[24] = xystart + vec2( 2.0,-2.0);\n\
		\n\
		texCoord[19] = xystart + vec2(-2.0,-1.0);\n\
		texCoord[ 6] = xystart + vec2(-1.0,-1.0);\n\
		texCoord[ 7] = xystart + vec2( 0.0,-1.0);\n\
		texCoord[ 8] = xystart + vec2( 1.0,-1.0);\n\
		texCoord[ 9] = xystart + vec2( 2.0,-1.0);\n\
		\n\
		texCoord[18] = xystart + vec2(-2.0, 0.0);\n\
		texCoord[ 5] = xystart + vec2(-1.0, 0.0);\n\
		texCoord[ 0] = xystart + vec2( 0.0, 0.0); // Center pixel\n\
		texCoord[ 1] = xystart + vec2( 1.0, 0.0);\n\
		texCoord[10] = xystart + vec2( 2.0, 0.0);\n\
		\n\
		texCoord[17] = xystart + vec2(-2.0, 1.0);\n\
		texCoord[ 4] = xystart + vec2(-1.0, 1.0);\n\
		texCoord[ 3] = xystart + vec2( 0.0, 1.0);\n\
		texCoord[ 2] = xystart + vec2( 1.0, 1.0);\n\
		texCoord[11] = xystart + vec2( 2.0, 1.0);\n\
		\n\
		texCoord[16] = xystart + vec2(-2.0, 2.0);\n\
		texCoord[15] = xystart + vec2(-1.0, 2.0);\n\
		texCoord[14] = xystart + vec2( 0.0, 2.0);\n\
		texCoord[13] = xystart + vec2( 1.0, 2.0);\n\
		texCoord[12] = xystart + vec2( 2.0, 2.0);\n\
		\n\
		gl_Position = vec4(projection * rotation * scale * inPosition, 0.0, 1.0);\n\
	}\n\
"};

static const char *BicubicSample6x6Output_VertShader_110 = {"\
	//---------------------------------------\n\
	// Input Pixel Mapping: 20|21|22|23|24|25\n\
	//                      19|06|07|08|09|26\n\
	//                      18|05|00|01|10|27\n\
	//                      17|04|03|02|11|28\n\
	//                      16|15|14|13|12|29\n\
	//                      35|34|33|32|31|30\n\
	\n\
	ATTRIBUTE vec2 inPosition;\n\
	ATTRIBUTE vec2 inTexCoord0;\n\
	\n\
	uniform vec2 viewSize; \n\
	uniform float scalar; \n\
	uniform float angleDegrees; \n\
	\n\
	VARYING vec2 texCoord[36];\n\
	\n\
	void main()\n\
	{\n\
		float angleRadians = radians(angleDegrees); \n\
		\n\
		mat2 projection	= mat2(	vec2(2.0/viewSize.x,            0.0), \n\
								vec2(           0.0, 2.0/viewSize.y)); \n\
		\n\
		mat2 rotation	= mat2(	vec2( cos(angleRadians), sin(angleRadians)), \n\
								vec2(-sin(angleRadians), cos(angleRadians))); \n\
		\n\
		mat2 scale		= mat2(	vec2(scalar,    0.0), \n\
								vec2(   0.0, scalar)); \n\
		\n\
		vec2 xystart = floor(inTexCoord0 - 0.5) + 0.5;\n\
		\n\
		texCoord[20] = xystart + vec2(-2.0,-2.0);\n\
		texCoord[21] = xystart + vec2(-1.0,-2.0);\n\
		texCoord[22] = xystart + vec2( 0.0,-2.0);\n\
		texCoord[23] = xystart + vec2( 1.0,-2.0);\n\
		texCoord[24] = xystart + vec2( 2.0,-2.0);\n\
		texCoord[25] = xystart + vec2( 3.0,-2.0);\n\
		\n\
		texCoord[19] = xystart + vec2(-2.0,-1.0);\n\
		texCoord[ 6] = xystart + vec2(-1.0,-1.0);\n\
		texCoord[ 7] = xystart + vec2( 0.0,-1.0);\n\
		texCoord[ 8] = xystart + vec2( 1.0,-1.0);\n\
		texCoord[ 9] = xystart + vec2( 2.0,-1.0);\n\
		texCoord[26] = xystart + vec2( 3.0,-1.0);\n\
		\n\
		texCoord[18] = xystart + vec2(-2.0, 0.0);\n\
		texCoord[ 5] = xystart + vec2(-1.0, 0.0);\n\
		texCoord[ 0] = xystart + vec2( 0.0, 0.0); // Center pixel\n\
		texCoord[ 1] = xystart + vec2( 1.0, 0.0);\n\
		texCoord[10] = xystart + vec2( 2.0, 0.0);\n\
		texCoord[27] = xystart + vec2( 3.0, 0.0);\n\
		\n\
		texCoord[17] = xystart + vec2(-2.0, 1.0);\n\
		texCoord[ 4] = xystart + vec2(-1.0, 1.0);\n\
		texCoord[ 3] = xystart + vec2( 0.0, 1.0);\n\
		texCoord[ 2] = xystart + vec2( 1.0, 1.0);\n\
		texCoord[11] = xystart + vec2( 2.0, 1.0);\n\
		texCoord[28] = xystart + vec2( 3.0, 1.0);\n\
		\n\
		texCoord[16] = xystart + vec2(-2.0, 2.0);\n\
		texCoord[15] = xystart + vec2(-1.0, 2.0);\n\
		texCoord[14] = xystart + vec2( 0.0, 2.0);\n\
		texCoord[13] = xystart + vec2( 1.0, 2.0);\n\
		texCoord[12] = xystart + vec2( 2.0, 2.0);\n\
		texCoord[29] = xystart + vec2( 3.0, 2.0);\n\
		\n\
		texCoord[35] = xystart + vec2(-2.0, 3.0);\n\
		texCoord[34] = xystart + vec2(-1.0, 3.0);\n\
		texCoord[33] = xystart + vec2( 0.0, 3.0);\n\
		texCoord[32] = xystart + vec2( 1.0, 3.0);\n\
		texCoord[31] = xystart + vec2( 2.0, 3.0);\n\
		texCoord[30] = xystart + vec2( 3.0, 3.0);\n\
		\n\
		gl_Position = vec4(projection * rotation * scale * inPosition, 0.0, 1.0);\n\
	}\n\
"};

// FRAGMENT SHADER FOR DISPLAY OUTPUT
static const char *PassthroughOutputFragShader_110 = {"\
	VARYING vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	\n\
	void main()\n\
	{\n\
		OUT_FRAG_COLOR.rgb = SAMPLE3_TEX_RECT(tex, texCoord[0]);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

// VERTEX SHADER FOR VIDEO FILTERS
static const char *Sample1x1_VertShader_110 = {"\
	// For reference, the pixels are mapped as follows:\n\
	// Start at center pixel 0 (p0), and circle around it\n\
	// in a clockwise fashion, starting from the right.\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  20|21|22|23|24\n\
	//                       19|06|07|08|09\n\
	//                       18|05|00|01|10\n\
	//                       17|04|03|02|11\n\
	//                       16|15|14|13|12\n\
	\n\
	ATTRIBUTE vec2 inPosition;\n\
	ATTRIBUTE vec2 inTexCoord0;\n\
	VARYING vec2 texCoord[1];\n\
	\n\
	void main()\n\
	{\n\
		texCoord[0] = inTexCoord0;\n\
		gl_Position = vec4(inPosition, 0.0, 1.0);\n\
	}\n\
"};

static const char *Sample2x2_VertShader_110 = {"\
	//---------------------------------------\n\
	// Input Pixel Mapping:  00|01\n\
	//                       03|02\n\
	\n\
	ATTRIBUTE vec2 inPosition;\n\
	ATTRIBUTE vec2 inTexCoord0;\n\
	VARYING vec2 texCoord[4];\n\
	\n\
	void main()\n\
	{\n\
		texCoord[0] = inTexCoord0 + vec2( 0.0, 0.0); // Center pixel\n\
		texCoord[1] = inTexCoord0 + vec2( 1.0, 0.0);\n\
		texCoord[3] = inTexCoord0 + vec2( 0.0, 1.0);\n\
		texCoord[2] = inTexCoord0 + vec2( 1.0, 1.0);\n\
		\n\
		gl_Position = vec4(inPosition, 0.0, 1.0);\n\
	}\n\
"};

static const char *Sample3x3_VertShader_110 = {"\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08\n\
	//                       05|00|01\n\
	//                       04|03|02\n\
	\n\
	ATTRIBUTE vec2 inPosition;\n\
	ATTRIBUTE vec2 inTexCoord0;\n\
	VARYING vec2 texCoord[9];\n\
	\n\
	void main()\n\
	{\n\
		texCoord[6] = inTexCoord0 + vec2(-1.0,-1.0);\n\
		texCoord[7] = inTexCoord0 + vec2( 0.0,-1.0);\n\
		texCoord[8] = inTexCoord0 + vec2( 1.0,-1.0);\n\
		\n\
		texCoord[5] = inTexCoord0 + vec2(-1.0, 0.0);\n\
		texCoord[0] = inTexCoord0 + vec2( 0.0, 0.0); // Center pixel\n\
		texCoord[1] = inTexCoord0 + vec2( 1.0, 0.0);\n\
		\n\
		texCoord[4] = inTexCoord0 + vec2(-1.0, 1.0);\n\
		texCoord[3] = inTexCoord0 + vec2( 0.0, 1.0);\n\
		texCoord[2] = inTexCoord0 + vec2( 1.0, 1.0);\n\
		\n\
		gl_Position = vec4(inPosition, 0.0, 1.0);\n\
	}\n\
"};

static const char *Sample4x4_VertShader_110 = {"\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08|09\n\
	//                       05|00|01|10\n\
	//                       04|03|02|11\n\
	//                       15|14|13|12\n\
	\n\
	ATTRIBUTE vec2 inPosition;\n\
	ATTRIBUTE vec2 inTexCoord0;\n\
	VARYING vec2 texCoord[16];\n\
	\n\
	void main()\n\
	{\n\
		texCoord[ 6] = inTexCoord0 + vec2(-1.0,-1.0);\n\
		texCoord[ 7] = inTexCoord0 + vec2( 0.0,-1.0);\n\
		texCoord[ 8] = inTexCoord0 + vec2( 1.0,-1.0);\n\
		texCoord[ 9] = inTexCoord0 + vec2( 2.0,-1.0);\n\
		\n\
		texCoord[ 5] = inTexCoord0 + vec2(-1.0, 0.0);\n\
		texCoord[ 0] = inTexCoord0 + vec2( 0.0, 0.0); // Center pixel\n\
		texCoord[ 1] = inTexCoord0 + vec2( 1.0, 0.0);\n\
		texCoord[10] = inTexCoord0 + vec2( 2.0, 0.0);\n\
		\n\
		texCoord[ 4] = inTexCoord0 + vec2(-1.0, 1.0);\n\
		texCoord[ 3] = inTexCoord0 + vec2( 0.0, 1.0);\n\
		texCoord[ 2] = inTexCoord0 + vec2( 1.0, 1.0);\n\
		texCoord[11] = inTexCoord0 + vec2( 2.0, 1.0);\n\
		\n\
		texCoord[15] = inTexCoord0 + vec2(-1.0, 2.0);\n\
		texCoord[14] = inTexCoord0 + vec2( 0.0, 2.0);\n\
		texCoord[13] = inTexCoord0 + vec2( 1.0, 2.0);\n\
		texCoord[12] = inTexCoord0 + vec2( 2.0, 2.0);\n\
		\n\
		gl_Position = vec4(inPosition, 0.0, 1.0);\n\
	}\n\
"};

static const char *Sample5x5_VertShader_110 = {"\
	//---------------------------------------\n\
	// Input Pixel Mapping:  20|21|22|23|24\n\
	//                       19|06|07|08|09\n\
	//                       18|05|00|01|10\n\
	//                       17|04|03|02|11\n\
	//                       16|15|14|13|12\n\
	\n\
	ATTRIBUTE vec2 inPosition;\n\
	ATTRIBUTE vec2 inTexCoord0;\n\
	VARYING vec2 texCoord[25];\n\
	\n\
	void main()\n\
	{\n\
		texCoord[20] = inTexCoord0 + vec2(-2.0,-2.0);\n\
		texCoord[21] = inTexCoord0 + vec2(-1.0,-2.0);\n\
		texCoord[22] = inTexCoord0 + vec2( 0.0,-2.0);\n\
		texCoord[23] = inTexCoord0 + vec2( 1.0,-2.0);\n\
		texCoord[24] = inTexCoord0 + vec2( 2.0,-2.0);\n\
		\n\
		texCoord[19] = inTexCoord0 + vec2(-2.0,-1.0);\n\
		texCoord[ 6] = inTexCoord0 + vec2(-1.0,-1.0);\n\
		texCoord[ 7] = inTexCoord0 + vec2( 0.0,-1.0);\n\
		texCoord[ 8] = inTexCoord0 + vec2( 1.0,-1.0);\n\
		texCoord[ 9] = inTexCoord0 + vec2( 2.0,-1.0);\n\
		\n\
		texCoord[18] = inTexCoord0 + vec2(-2.0, 0.0);\n\
		texCoord[ 5] = inTexCoord0 + vec2(-1.0, 0.0);\n\
		texCoord[ 0] = inTexCoord0 + vec2( 0.0, 0.0); // Center pixel\n\
		texCoord[ 1] = inTexCoord0 + vec2( 1.0, 0.0);\n\
		texCoord[10] = inTexCoord0 + vec2( 2.0, 0.0);\n\
		\n\
		texCoord[17] = inTexCoord0 + vec2(-2.0, 1.0);\n\
		texCoord[ 4] = inTexCoord0 + vec2(-1.0, 1.0);\n\
		texCoord[ 3] = inTexCoord0 + vec2( 0.0, 1.0);\n\
		texCoord[ 2] = inTexCoord0 + vec2( 1.0, 1.0);\n\
		texCoord[11] = inTexCoord0 + vec2( 2.0, 1.0);\n\
		\n\
		texCoord[16] = inTexCoord0 + vec2(-2.0, 2.0);\n\
		texCoord[15] = inTexCoord0 + vec2(-1.0, 2.0);\n\
		texCoord[14] = inTexCoord0 + vec2( 0.0, 2.0);\n\
		texCoord[13] = inTexCoord0 + vec2( 1.0, 2.0);\n\
		texCoord[12] = inTexCoord0 + vec2( 2.0, 2.0);\n\
		\n\
		gl_Position = vec4(inPosition, 0.0, 1.0);\n\
	}\n\
"};

// FRAGMENT SHADER PASSTHROUGH FOR FILTERS
static const char *PassthroughFragShader_110 = {"\
	VARYING vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	\n\
	void main()\n\
	{\n\
		OUT_FRAG_COLOR.rgb = SAMPLE3_TEX_RECT(tex, texCoord[0]);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

// FRAGMENT SHADER FOR DEPOSTERIZATION
static const char *FilterDeposterizeFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	\n\
	vec3 InterpLTE(vec3 pixA, vec3 pixB, vec3 threshold)\n\
	{\n\
		vec3 interpPix = mix(pixA, pixB, 0.5);\n\
		vec3 pixCompare = vec3( lessThanEqual(abs(pixB - pixA), threshold) );\n\
		\n\
		return mix(pixA, interpPix, pixCompare);\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08\n\
	//                       05|00|01\n\
	//                       04|03|02\n\
	//\n\
	// Output Pixel Mapping:    A\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[9];\n\
		src[0] = SAMPLE3_TEX_RECT(tex, texCoord[0]);\n\
		src[1] = SAMPLE3_TEX_RECT(tex, texCoord[1]);\n\
		src[2] = SAMPLE3_TEX_RECT(tex, texCoord[2]);\n\
		src[3] = SAMPLE3_TEX_RECT(tex, texCoord[3]);\n\
		src[4] = SAMPLE3_TEX_RECT(tex, texCoord[4]);\n\
		src[5] = SAMPLE3_TEX_RECT(tex, texCoord[5]);\n\
		src[6] = SAMPLE3_TEX_RECT(tex, texCoord[6]);\n\
		src[7] = SAMPLE3_TEX_RECT(tex, texCoord[7]);\n\
		src[8] = SAMPLE3_TEX_RECT(tex, texCoord[8]);\n\
		\n\
		const vec3 threshold = vec3(0.1020);\n\
		\n\
		float weight[2];\n\
		weight[0] = 0.90;\n\
		weight[1] = weight[0] * 0.60;\n\
		\n\
		vec3 blend[9];\n\
		blend[0] = src[0];\n\
		blend[1] = InterpLTE(src[0], src[1], threshold);\n\
		blend[2] = InterpLTE(src[0], src[2], threshold);\n\
		blend[3] = InterpLTE(src[0], src[3], threshold);\n\
		blend[4] = InterpLTE(src[0], src[4], threshold);\n\
		blend[5] = InterpLTE(src[0], src[5], threshold);\n\
		blend[6] = InterpLTE(src[0], src[6], threshold);\n\
		blend[7] = InterpLTE(src[0], src[7], threshold);\n\
		blend[8] = InterpLTE(src[0], src[8], threshold);\n\
		\n\
		OUT_FRAG_COLOR.rgb =	mix(\n\
									mix(\n\
										mix(\n\
											mix(blend[0], blend[5], weight[0]), mix(blend[0], blend[1], weight[0]),\n\
										0.50),\n\
										mix(\n\
											mix(blend[0], blend[7], weight[0]), mix(blend[0], blend[3], weight[0]),\n\
										0.50),\n\
									0.50),\n\
									mix(\n\
										mix(\n\
											mix(blend[0], blend[6], weight[1]), mix(blend[0], blend[2], weight[1]),\n\
										0.50),\n\
										mix(\n\
											mix(blend[0], blend[8], weight[1]), mix(blend[0], blend[4], weight[1]),\n\
										0.50),\n\
									0.50),\n\
								0.25);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *FilterBicubicBSplineFragShader_110 = {"\
	VARYING vec2 texCoord[16];\n\
	uniform sampler2DRect tex;\n\
	\n\
	vec4 WeightBSpline(float f)\n\
	{\n\
		return	vec4(((1.0-f)*(1.0-f)*(1.0-f)) / 6.0,\n\
				     (4.0 - 6.0*f*f + 3.0*f*f*f) / 6.0,\n\
				     (1.0 + 3.0*f + 3.0*f*f - 3.0*f*f*f) / 6.0,\n\
				     f*f*f / 6.0);\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08|09\n\
	//                       05|00|01|10\n\
	//                       04|03|02|11\n\
	//                       15|14|13|12\n\
	\n\
	void main()\n\
	{\n\
		vec2 f = fract(texCoord[0]);\n\
		vec4 wx = WeightBSpline(f.x);\n\
		vec4 wy = WeightBSpline(f.y);\n\
		\n\
		// Normalize weights\n\
		wx /= dot(wx, vec4(1.0));\n\
		wy /= dot(wy, vec4(1.0));\n\
		\n\
		OUT_FRAG_COLOR.rgb	= (SAMPLE3_TEX_RECT(tex, texCoord[ 6]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 7]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 8]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 9]) * wx.a) * wy.r\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 5]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 1]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[10]) * wx.a) * wy.g\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 4]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 3]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 2]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[11]) * wx.a) * wy.b\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[15]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[14]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[13]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[12]) * wx.a) * wy.a;\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *FilterBicubicBSplineFastFragShader_110 = {"\
	VARYING vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	\n\
	void main()\n\
	{\n\
		vec2 texCenterPosition = floor(texCoord[0] - 0.5) + 0.5;\n\
		vec2 f = abs(texCoord[0] - texCenterPosition);\n\
		\n\
		vec2 w0 = ((1.0-f)*(1.0-f)*(1.0-f)) / 6.0;\n\
		vec2 w1 = (4.0 - 6.0*f*f + 3.0*f*f*f) / 6.0;\n\
		vec2 w3 = f*f*f / 6.0;\n\
		vec2 w2 = 1.0 - w0 - w1 - w3;\n\
		\n\
		vec2 s0 = w0 + w1;\n\
		vec2 s1 = w2 + w3;\n\
		\n\
		vec2 t0 = texCenterPosition - 1.0 + (w1 / s0);\n\
		vec2 t1 = texCenterPosition + 1.0 + (w3 / s1);\n\
		\n\
		OUT_FRAG_COLOR.rgb	= (SAMPLE3_TEX_RECT(tex, vec2(t0.x, t0.y)) * s0.x +\n\
							   SAMPLE3_TEX_RECT(tex, vec2(t1.x, t0.y)) * s1.x) * s0.y +\n\
							  (SAMPLE3_TEX_RECT(tex, vec2(t0.x, t1.y)) * s0.x +\n\
							   SAMPLE3_TEX_RECT(tex, vec2(t1.x, t1.y)) * s1.x) * s1.y;\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *FilterBicubicMitchellNetravaliFragShader_110 = {"\
	VARYING vec2 texCoord[16];\n\
	uniform sampler2DRect tex;\n\
	\n\
	vec4 WeightMitchellNetravali(float f)\n\
	{\n\
		return	vec4((1.0 - 9.0*f + 15.0*f*f - 7.0*f*f*f) / 18.0,\n\
				     (16.0 - 36.0*f*f + 21.0*f*f*f) / 18.0,\n\
				     (1.0 + 9.0*f + 27.0*f*f - 21.0*f*f*f) / 18.0,\n\
				     (7.0*f*f*f - 6.0*f*f) / 18.0);\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08|09\n\
	//                       05|00|01|10\n\
	//                       04|03|02|11\n\
	//                       15|14|13|12\n\
	\n\
	void main()\n\
	{\n\
		vec2 f = fract(texCoord[0]);\n\
		vec4 wx = WeightMitchellNetravali(f.x);\n\
		vec4 wy = WeightMitchellNetravali(f.y);\n\
		\n\
		// Normalize weights\n\
		wx /= dot(wx, vec4(1.0));\n\
		wy /= dot(wy, vec4(1.0));\n\
		\n\
		OUT_FRAG_COLOR.rgb	= (SAMPLE3_TEX_RECT(tex, texCoord[ 6]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 7]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 8]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 9]) * wx.a) * wy.r\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 5]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 1]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[10]) * wx.a) * wy.g\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 4]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 3]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 2]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[11]) * wx.a) * wy.b\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[15]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[14]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[13]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[12]) * wx.a) * wy.a;\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *FilterBicubicMitchellNetravaliFastFragShader_110 = {"\
	VARYING vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	\n\
	void main()\n\
	{\n\
		vec2 texCenterPosition = floor(texCoord[0] - 0.5) + 0.5;\n\
		vec2 f = abs(texCoord[0] - texCenterPosition);\n\
		\n\
		vec2 w0 = (1.0 - 9.0*f + 15.0*f*f - 7.0*f*f*f) / 18.0;\n\
		vec2 w1 = (16.0 - 36.0*f*f + 21.0*f*f*f) / 18.0;\n\
		vec2 w3 = (7.0*f*f*f - 6.0*f*f) / 18.0;\n\
		vec2 w2 = 1.0 - w0 - w1 - w3;\n\
		\n\
		vec2 s0 = w0 + w1;\n\
		vec2 s1 = w2 + w3;\n\
		\n\
		vec2 t0 = texCenterPosition - 1.0 + (w1 / s0);\n\
		vec2 t1 = texCenterPosition + 1.0 + (w3 / s1);\n\
		\n\
		OUT_FRAG_COLOR.rgb	= (SAMPLE3_TEX_RECT(tex, vec2(t0.x, t0.y)) * s0.x +\n\
							   SAMPLE3_TEX_RECT(tex, vec2(t1.x, t0.y)) * s1.x) * s0.y +\n\
							  (SAMPLE3_TEX_RECT(tex, vec2(t0.x, t1.y)) * s0.x +\n\
							   SAMPLE3_TEX_RECT(tex, vec2(t1.x, t1.y)) * s1.x) * s1.y;\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *FilterLanczos2FragShader_110 = {"\
	#define PI 3.1415926535897932384626433832795\n\
	#define RADIUS 2.0\n\
	#define FIX(c) max(abs(c), 1e-5)\n\
	\n\
	VARYING vec2 texCoord[16];\n\
	uniform sampler2DRect tex;\n\
	\n\
	vec4 WeightLanczos2(float f)\n\
	{\n\
		vec4 sample = FIX(PI * vec4(1.0 + f, f, 1.0 - f, 2.0 - f));\n\
		return ( sin(sample) * sin(sample / RADIUS) / (sample * sample) );\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08|09\n\
	//                       05|00|01|10\n\
	//                       04|03|02|11\n\
	//                       15|14|13|12\n\
	\n\
	void main()\n\
	{\n\
		vec2 f = fract(texCoord[0]);\n\
		vec4 wx = WeightLanczos2(f.x);\n\
		vec4 wy = WeightLanczos2(f.y);\n\
		\n\
		// Normalize weights\n\
		wx /= dot(wx, vec4(1.0));\n\
		wy /= dot(wy, vec4(1.0));\n\
		\n\
		OUT_FRAG_COLOR.rgb	= (SAMPLE3_TEX_RECT(tex, texCoord[ 6]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 7]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 8]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 9]) * wx.a) * wy.r\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 5]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 1]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[10]) * wx.a) * wy.g\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 4]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 3]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 2]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[11]) * wx.a) * wy.b\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[15]) * wx.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[14]) * wx.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[13]) * wx.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[12]) * wx.a) * wy.a;\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *FilterLanczos3FragShader_110 = {"\
	#define PI 3.1415926535897932384626433832795\n\
	#define RADIUS 3.0\n\
	#define FIX(c) max(abs(c), 1e-5)\n\
	\n\
#if GPU_TIER >= SHADERSUPPORT_HIGH_TIER\n\
	VARYING vec2 texCoord[36];\n\
#elif GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
	VARYING vec2 texCoord[25];\n\
#else\n\
	VARYING vec2 texCoord[16];\n\
#endif\n\
	uniform sampler2DRect tex;\n\
	\n\
	vec3 weight3(float x)\n\
	{\n\
		vec3 sample = FIX(2.0 * PI * vec3(x - 1.5, x - 0.5, x + 0.5));\n\
		return ( sin(sample) * sin(sample / RADIUS) / (sample * sample) );\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping: 20|21|22|23|24|25\n\
	//                      19|06|07|08|09|26\n\
	//                      18|05|00|01|10|27\n\
	//                      17|04|03|02|11|28\n\
	//                      16|15|14|13|12|29\n\
	//                      35|34|33|32|31|30\n\
	\n\
	void main()\n\
	{\n\
		vec2 f = fract(texCoord[0]);\n\
		vec3 wx1 = weight3(0.5 - f.x * 0.5);\n\
		vec3 wx2 = weight3(1.0 - f.x * 0.5);\n\
		vec3 wy1 = weight3(0.5 - f.y * 0.5);\n\
		vec3 wy2 = weight3(1.0 - f.y * 0.5);\n\
		\n\
		// Normalize weights\n\
		float sumX = dot(wx1, vec3(1.0)) + dot(wx2, vec3(1.0));\n\
		float sumY = dot(wy1, vec3(1.0)) + dot(wy2, vec3(1.0));\n\
		wx1 /= sumX;\n\
		wx2 /= sumX;\n\
		wy1 /= sumY;\n\
		wy2 /= sumY;\n\
		\n\
#if GPU_TIER >= SHADERSUPPORT_HIGH_TIER\n\
		OUT_FRAG_COLOR.rgb	= (SAMPLE3_TEX_RECT(tex, texCoord[20]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[21]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[22]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[23]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[24]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[25]) * wx2.b) * wy1.r\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[19]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 6]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 7]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 8]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 9]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[26]) * wx2.b) * wy2.r\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[18]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 5]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 1]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[10]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[27]) * wx2.b) * wy1.g\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[17]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 4]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 3]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 2]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[11]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[28]) * wx2.b) * wy2.g\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[16]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[15]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[14]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[13]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[12]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[29]) * wx2.b) * wy1.b\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[35]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[34]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[33]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[32]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[31]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[30]) * wx2.b) * wy2.b;\n\
#elif GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
		OUT_FRAG_COLOR.rgb	= (SAMPLE3_TEX_RECT(tex, texCoord[20]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[21]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[22]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[23]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[24]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0,-2.0)) * wx2.b) * wy1.r\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[19]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 6]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 7]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 8]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 9]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0,-1.0)) * wx2.b) * wy2.r\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[18]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 5]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 1]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[10]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0, 0.0)) * wx2.b) * wy1.g\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[17]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 4]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 3]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 2]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[11]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0, 1.0)) * wx2.b) * wy2.g\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[16]) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[15]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[14]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[13]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[12]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0, 2.0)) * wx2.b) * wy1.b\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2(-2.0, 3.0)) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2(-1.0, 3.0)) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 0.0, 3.0)) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 1.0, 3.0)) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 2.0, 3.0)) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0, 3.0)) * wx2.b) * wy2.b;\n\
#else\n\
		OUT_FRAG_COLOR.rgb	= (SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2(-2.0,-2.0)) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2(-1.0,-2.0)) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 0.0,-2.0)) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 1.0,-2.0)) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 2.0,-2.0)) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0,-2.0)) * wx2.b) * wy1.r\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2(-2.0,-1.0)) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 6]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 7]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 8]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 9]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0,-1.0)) * wx2.b) * wy2.r\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2(-2.0, 0.0)) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 5]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 1]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[10]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0, 0.0)) * wx2.b) * wy1.g\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2(-2.0, 1.0)) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 4]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 3]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 2]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[11]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0, 1.0)) * wx2.b) * wy2.g\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2(-2.0, 2.0)) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[15]) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[14]) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[13]) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[12]) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0, 2.0)) * wx2.b) * wy1.b\n\
							+ (SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2(-2.0, 3.0)) * wx1.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2(-1.0, 3.0)) * wx2.r\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 0.0, 3.0)) * wx1.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 1.0, 3.0)) * wx2.g\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 2.0, 3.0)) * wx1.b\n\
							+  SAMPLE3_TEX_RECT(tex, texCoord[ 0] + vec2( 3.0, 3.0)) * wx2.b) * wy2.b;\n\
#endif\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *Scalar2xScanlineFragShader_110 = {"\
	VARYING vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  00\n\
	//\n\
	// Output Pixel Mapping: A|B\n\
	//                       C|D\n\
	\n\
	void main()\n\
	{\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		float w = mix( mix(1.000, 0.875, f.x), mix(0.875, 0.750, f.x), f.y );\n\
		\n\
		OUT_FRAG_COLOR = vec4(SAMPLE3_TEX_RECT(tex, texCoord[0]) * w, 1.0);\n\
	}\n\
"};

static const char *Scalar2xEPXFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	\n\
	float reduce(vec3 color)\n\
	{\n\
		return dot(color, vec3(65536.0, 256.0, 1.0));\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  --|07|--\n\
	//                       05|00|01\n\
	//                       --|03|--\n\
	//\n\
	// Output Pixel Mapping:   0|1\n\
	//                         2|3\n\
	\n\
	void main()\n\
	{\n\
		vec3 src7 = SAMPLE3_TEX_RECT(tex, texCoord[7]);\n\
		vec3 src5 = SAMPLE3_TEX_RECT(tex, texCoord[5]);\n\
		vec3 src0 = SAMPLE3_TEX_RECT(tex, texCoord[0]);\n\
		vec3 src1 = SAMPLE3_TEX_RECT(tex, texCoord[1]);\n\
		vec3 src3 = SAMPLE3_TEX_RECT(tex, texCoord[3]);\n\
		\n\
		float v7 = reduce(src7);\n\
		float v5 = reduce(src5);\n\
		float v1 = reduce(src1);\n\
		float v3 = reduce(src3);\n\
		\n\
		bool pixCompare = (v5 != v1) && (v7 != v3);\n\
		\n\
		vec3 newFragColor[4];\n\
		newFragColor[0] = (pixCompare && (v7 == v5)) ? src7 : src0;\n\
		newFragColor[1] = (pixCompare && (v1 == v7)) ? src1 : src0;\n\
		newFragColor[2] = (pixCompare && (v5 == v3)) ? src5 : src0;\n\
		newFragColor[3] = (pixCompare && (v3 == v1)) ? src3 : src0;\n\
		\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		OUT_FRAG_COLOR.rgb = mix( mix(newFragColor[0], newFragColor[1], f.x), mix(newFragColor[2], newFragColor[3], f.x), f.y );\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *Scalar2xEPXPlusFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	\n\
	float dist(vec3 pixA, vec3 pixB)\n\
	{\n\
		return dot(abs(pixA - pixB), vec3(2.0, 3.0, 3.0));\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  --|07|--\n\
	//                       05|00|01\n\
	//                       --|03|--\n\
	//\n\
	// Output Pixel Mapping:   0|1\n\
	//                         2|3\n\
	\n\
	void main()\n\
	{\n\
		vec3 src7 = SAMPLE3_TEX_RECT(tex, texCoord[7]);\n\
		vec3 src5 = SAMPLE3_TEX_RECT(tex, texCoord[5]);\n\
		vec3 src0 = SAMPLE3_TEX_RECT(tex, texCoord[0]);\n\
		vec3 src1 = SAMPLE3_TEX_RECT(tex, texCoord[1]);\n\
		vec3 src3 = SAMPLE3_TEX_RECT(tex, texCoord[3]);\n\
		\n\
		vec3 newFragColor[4];\n\
		newFragColor[0] = ( dist(src5, src7) < min(dist(src5, src3), dist(src1, src7)) ) ? mix(src5, src7, 0.5) : src0;\n\
		newFragColor[1] = ( dist(src1, src7) < min(dist(src5, src7), dist(src1, src3)) ) ? mix(src1, src7, 0.5) : src0;\n\
		newFragColor[2] = ( dist(src5, src3) < min(dist(src5, src7), dist(src1, src3)) ) ? mix(src5, src3, 0.5) : src0;\n\
		newFragColor[3] = ( dist(src1, src3) < min(dist(src5, src3), dist(src1, src7)) ) ? mix(src1, src3, 0.5) : src0;\n\
		\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		OUT_FRAG_COLOR.rgb = mix( mix(newFragColor[0], newFragColor[1], f.x), mix(newFragColor[2], newFragColor[3], f.x), f.y );\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *Scalar2xSaIFragShader_110 = {"\
	VARYING vec2 texCoord[16];\n\
	uniform sampler2DRect tex;\n\
	\n\
	const vec3 dt = vec3(65536.0, 256.0, 1.0);\n\
	\n\
	float GetResult(float v1, float v2, float v3, float v4)\n\
	{\n\
		return sign(abs(v1-v3)+abs(v1-v4))-sign(abs(v2-v3)+abs(v2-v4));\n\
	}\n\
	\n\
	float reduce(vec3 color)\n\
	{\n\
		return dot(color, dt);\n\
	}\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08|09\n\
	//                       05|00|01|10\n\
	//                       04|03|02|11\n\
	//                       15|14|13|12\n\
	//\n\
	// Output Pixel Mapping:     0|1\n\
	//                           2|3\n\
	\n\
	//---------------------------------------\n\
	// 2xSaI Pixel Mapping:    I|E|F|J\n\
	//                         G|A|B|K\n\
	//                         H|C|D|L\n\
	//                         M|N|O|P\n\
	\n\
	void main()\n\
	{\n\
		float Iv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[6]).rgb );\n\
		float Ev = reduce( SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb );\n\
		float Fv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb );\n\
		float Jv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[9]).rgb );\n\
		\n\
		float Gv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb );\n\
		vec3 Ac = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb; float Av = reduce(Ac);\n\
		vec3 Bc = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb; float Bv = reduce(Bc);\n\
		float Kv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[10]).rgb );\n\
		\n\
		float Hv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb );\n\
		vec3 Cc = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb; float Cv = reduce(Cc);\n\
		vec3 Dc = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb; float Dv = reduce(Dc);\n\
		float Lv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[11]).rgb );\n\
		\n\
		float Mv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[15]).rgb );\n\
		float Nv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[14]).rgb );\n\
		float Ov = reduce( SAMPLE3_TEX_RECT(tex, texCoord[13]).rgb );\n\
		// Pv is unused, so skip this one.\n\
		\n\
		bool compAD = (Av == Dv);\n\
		bool compBC = (Bv == Cv);\n\
		\n\
		vec3 newFragColor[4];\n\
		newFragColor[0] = Ac;\n\
		newFragColor[1] = Ac;\n\
		newFragColor[2] = Ac;\n\
		newFragColor[3] = Ac;\n\
		\n\
		if (compAD && !compBC)\n\
		{\n\
			newFragColor[1] = ((Av == Ev) && (Bv == Lv)) || ((Av == Cv) && (Av == Fv) && (Bv != Ev) && (Bv == Jv)) ? Ac : mix(Ac, Bc, 0.5);\n\
			newFragColor[2] = ((Av == Gv) && (Cv == Ov)) || ((Av == Bv) && (Av == Hv) && (Cv != Gv) && (Cv == Mv)) ? Ac : mix(Ac, Cc, 0.5);\n\
		}\n\
		else if (!compAD && compBC)\n\
		{\n\
			newFragColor[1] = ((Bv == Fv) && (Av == Hv)) || ((Bv == Ev) && (Bv == Dv) && (Av != Fv) && (Av == Iv)) ? Bc : mix(Ac, Bc, 0.5);\n\
			newFragColor[2] = ((Cv == Hv) && (Av == Fv)) || ((Cv == Gv) && (Cv == Dv) && (Av != Hv) && (Av == Iv)) ? Cc : mix(Ac, Cc, 0.5);\n\
			newFragColor[3] = Bc;\n\
		}\n\
		else if (compAD && compBC)\n\
		{\n\
			newFragColor[1] = (Av == Bv) ? Ac : mix(Ac, Bc, 0.5);\n\
			newFragColor[2] = (Av == Bv) ? Ac : mix(Ac, Cc, 0.5);\n\
			\n\
			float r = (Av == Bv) ? 1.0 : GetResult(Av, Bv, Gv, Ev) - GetResult(Bv, Av, Kv, Fv) - GetResult(Bv, Av, Hv, Nv) + GetResult(Av, Bv, Lv, Ov);\n\
			newFragColor[3] = (r > 0.0) ? Ac : ( (r < 0.0) ? Bc : mix( mix(Ac, Bc, 0.5), mix(Cc, Dc, 0.5), 0.5) );\n\
		}\n\
		else\n\
		{\n\
			newFragColor[1] = ((Av == Cv) && (Av == Fv) && (Bv != Ev) && (Bv == Jv)) ? Ac : ( ((Bv == Ev) && (Bv == Dv) && (Av != Fv) && (Av == Iv)) ? Bc : mix(Ac, Bc, 0.5) );\n\
			newFragColor[2] = ((Av == Bv) && (Av == Hv) && (Cv != Gv) && (Cv == Mv)) ? Ac : ( ((Cv == Gv) && (Cv == Dv) && (Av != Hv) && (Av == Iv)) ? Cc : mix(Ac, Cc, 0.5) );\n\
			newFragColor[3] = mix( mix(Ac, Bc, 0.5), mix(Cc, Dc, 0.5), 0.5 );\n\
		}\n\
		\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		OUT_FRAG_COLOR.rgb = mix( mix(newFragColor[0], newFragColor[1], f.x), mix(newFragColor[2], newFragColor[3], f.x), f.y );\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *ScalarSuper2xSaIFragShader_110 = {"\
	VARYING vec2 texCoord[16];\n\
	uniform sampler2DRect tex;\n\
	\n\
	const vec3 dt = vec3(65536.0, 256.0, 1.0);\n\
	\n\
	float GetResult(float v1, float v2, float v3, float v4)\n\
	{\n\
		return sign(abs(v1-v3)+abs(v1-v4))-sign(abs(v2-v3)+abs(v2-v4));\n\
	}\n\
	\n\
	float reduce(vec3 color)\n\
	{\n\
		return dot(color, dt);\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08|09\n\
	//                       05|00|01|10\n\
	//                       04|03|02|11\n\
	//                       15|14|13|12\n\
	//\n\
	// Output Pixel Mapping:     A|B\n\
	//                           C|D\n\
	\n\
	//---------------------------------------\n\
	// S2xSaI Pixel Mapping:   I|E|F|J\n\
	//                         G|A|B|K\n\
	//                         H|C|D|L\n\
	//                         M|N|O|P\n\
	\n\
	void main()\n\
	{\n\
		float Iv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[6]).rgb );\n\
		float Ev = reduce( SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb );\n\
		float Fv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb );\n\
		float Jv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[9]).rgb );\n\
		\n\
		float Gv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb );\n\
		vec3 Ac = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb; float Av = reduce(Ac);\n\
		vec3 Bc = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb; float Bv = reduce(Bc);\n\
		float Kv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[10]).rgb );\n\
		\n\
		float Hv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb );\n\
		vec3 Cc = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb; float Cv = reduce(Cc);\n\
		vec3 Dc = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb; float Dv = reduce(Dc);\n\
		float Lv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[11]).rgb );\n\
		\n\
		float Mv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[15]).rgb );\n\
		float Nv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[14]).rgb );\n\
		float Ov = reduce( SAMPLE3_TEX_RECT(tex, texCoord[13]).rgb );\n\
		float Pv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[12]).rgb );\n\
		\n\
		bool compAD = (Av == Dv);\n\
		bool compBC = (Bv == Cv);\n\
		\n\
		vec3 newFragColor[4];\n\
		newFragColor[0] = ( (compBC && !compAD && (Hv == Cv) && (Cv != Fv)) || ((Gv == Cv) && (Dv == Cv) && (Hv != Av) && (Cv != Iv)) ) ? mix(Ac, Cc, 0.5) : Ac;\n\
		newFragColor[1] = Bc;\n\
		newFragColor[2] = ( (compAD && !compBC && (Gv == Av) && (Av != Ov)) || ((Av == Hv) && (Av == Bv) && (Gv != Cv) && (Av != Mv)) ) ? mix(Ac, Cc, 0.5) : Cc;\n\
		newFragColor[3] = Dc;\n\
		\n\
		if (compBC && !compAD)\n\
		{\n\
			newFragColor[1] = newFragColor[3] = Cc;\n\
		}\n\
		else if (!compBC && compAD)\n\
		{\n\
			newFragColor[1] = newFragColor[3] = Ac;\n\
		}\n\
		else if (compBC && compAD)\n\
		{\n\
			float r = GetResult(Bv, Av, Hv, Nv) + GetResult(Bv, Av, Gv, Ev) + GetResult(Bv, Av, Ov, Lv) + GetResult(Bv, Av, Fv, Kv);\n\
			newFragColor[1] = newFragColor[3] = (r > 0.0) ? Bc : ( (r < 0.0) ? Ac : mix(Ac, Bc, 0.5) );\n\
		}\n\
		else\n\
		{\n\
			newFragColor[1] = ( (Bv == Dv) && (Bv == Ev) && (Av != Fv) && (Bv != Iv) ) ? mix(Ac, Bc, 0.75) : ( ( (Av == Cv) && (Av == Fv) && (Ev != Bv) && (Av != Jv) ) ? mix(Ac, Bc, 0.25) : mix(Ac, Bc, 0.5) );\n\
			newFragColor[3] = ( (Bv == Dv) && (Dv == Nv) && (Cv != Ov) && (Dv != Mv) ) ? mix(Cc, Dc, 0.75) : ( ( (Av == Cv) && (Cv == Ov) && (Nv != Dv) && (Cv != Pv) ) ? mix(Cc, Dc, 0.25) : mix(Cc, Dc, 0.5) );\n\
		}\n\
		\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		OUT_FRAG_COLOR.rgb = mix( mix(newFragColor[0], newFragColor[1], f.x), mix(newFragColor[2], newFragColor[3], f.x), f.y );\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *ScalarSuperEagle2xFragShader_110 = {"\
	VARYING vec2 texCoord[16];\n\
	uniform sampler2DRect tex;\n\
	\n\
	const vec3 dt = vec3(65536.0, 256.0, 1.0);\n\
	\n\
	float GetResult(float v1, float v2, float v3, float v4)\n\
	{\n\
		return sign(abs(v1-v3)+abs(v1-v4))-sign(abs(v2-v3)+abs(v2-v4));\n\
	}\n\
	\n\
	float reduce(vec3 color)\n\
	{\n\
		return dot(color, dt);\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  --|07|08|--\n\
	//                       05|00|01|10\n\
	//                       04|03|02|11\n\
	//                       --|14|13|--\n\
	//\n\
	// Output Pixel Mapping:     0|1\n\
	//                           2|3\n\
	\n\
	//---------------------------------------\n\
	// SEagle Pixel Mapping:   -|E|F|-\n\
	//                         G|A|B|K\n\
	//                         H|C|D|L\n\
	//                         -|N|O|-\n\
	\n\
	void main()\n\
	{\n\
		float Ev = reduce( SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb );\n\
		float Fv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb );\n\
		\n\
		float Gv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb );\n\
		vec3 Ac = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb; float Av = reduce(Ac);\n\
		vec3 Bc = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb; float Bv = reduce(Bc);\n\
		float Kv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[10]).rgb );\n\
		\n\
		float Hv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb );\n\
		vec3 Cc = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb; float Cv = reduce(Cc);\n\
		vec3 Dc = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb; float Dv = reduce(Dc);\n\
		float Lv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[11]).rgb );\n\
		\n\
		float Nv = reduce( SAMPLE3_TEX_RECT(tex, texCoord[14]).rgb );\n\
		float Ov = reduce( SAMPLE3_TEX_RECT(tex, texCoord[13]).rgb );\n\
		\n\
		bool compAD = (Av == Dv);\n\
		bool compBC = (Bv == Cv);\n\
		\n\
		vec3 newFragColor[4];\n\
		newFragColor[0] = Ac;\n\
		newFragColor[1] = Bc;\n\
		newFragColor[2] = Cc;\n\
		newFragColor[3] = Dc;\n\
		\n\
		if (compBC && !compAD)\n\
		{\n\
			newFragColor[0] = (Cv == Hv || Bv == Fv) ? mix(Ac, Cc, 0.75) : mix(Ac, Bc, 0.5);\n\
			newFragColor[1] = Cc;\n\
			//newFragColor[2] = Cc;\n\
			newFragColor[3] = mix( mix(Dc, Cc, 0.5), mix(Dc, Cc, 0.75), float(Bv == Kv || Cv == Nv) );\n\
		}\n\
		else if (!compBC && compAD)\n\
		{\n\
			//newFragColor[0] = Ac;\n\
			newFragColor[1] = mix( mix(Ac, Bc, 0.5), mix(Ac, Bc, 0.25), float(Av == Ev || Dv == Lv) );\n\
			newFragColor[2] = mix( mix(Cc, Dc, 0.5), mix(Ac, Cc, 0.25), float(Dv == Ov || Av == Gv) );\n\
			newFragColor[3] = Ac;\n\
		}\n\
		else if (compBC && compAD)\n\
		{\n\
			float r = GetResult(Bv, Av, Hv, Nv) + GetResult(Bv, Av, Gv, Ev) + GetResult(Bv, Av, Ov, Lv) + GetResult(Bv, Av, Fv, Kv);\n\
			if (r > 0.0)\n\
			{\n\
				newFragColor[0] = mix(Ac, Bc, 0.5);\n\
				newFragColor[1] = Cc;\n\
				//newFragColor[2] = Cc;\n\
				newFragColor[3] = newFragColor[0];\n\
			}\n\
			else if (r < 0.0)\n\
			{\n\
				//newFragColor[0] = Ac;\n\
				newFragColor[1] = mix(Ac, Bc, 0.5);\n\
				newFragColor[2] = newFragColor[1];\n\
				newFragColor[3] = Ac;\n\
			}\n\
			else\n\
			{\n\
				//newFragColor[0] = Ac;\n\
				newFragColor[1] = Cc;\n\
				//newFragColor[2] = Cc;\n\
				newFragColor[3] = Ac;\n\
			}\n\
		}\n\
		else\n\
		{\n\
			newFragColor[0] = mix(mix(Bc, Cc, 0.5), Ac, 0.75);\n\
			newFragColor[1] = mix(mix(Ac, Dc, 0.5), Bc, 0.75);\n\
			newFragColor[2] = mix(mix(Ac, Dc, 0.5), Cc, 0.75);\n\
			newFragColor[3] = mix(mix(Bc, Cc, 0.5), Dc, 0.75);\n\
		}\n\
		\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		OUT_FRAG_COLOR.rgb = mix( mix(newFragColor[0], newFragColor[1], f.x), mix(newFragColor[2], newFragColor[3], f.x), f.y );\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *ScalerLQ2xFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	uniform sampler3D lut;\n\
	\n\
	float reduce(vec3 color)\n\
	{\n\
		return dot(color, vec3(65536.0, 256.0, 1.0));\n\
	}\n\
	\n\
	vec3 Lerp(vec3 weight, vec3 p1, vec3 p2, vec3 p3)\n\
	{\n\
		return p1*weight.r + p2*weight.g + p3*weight.b;\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08\n\
	//                       05|00|01\n\
	//                       04|03|02\n\
	//\n\
	// Output Pixel Mapping:  00|01\n\
	//                        02|03\n\
	\n\
	//---------------------------------------\n\
	// LQ2x Pixel Mapping:    0|1|2\n\
	//                        3|4|5\n\
	//                        6|7|8\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[9];\n\
		src[0] = SAMPLE3_TEX_RECT(tex, texCoord[6]).rgb;\n\
		src[1] = SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb;\n\
		src[2] = SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb;\n\
		src[3] = SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb;\n\
		src[4] = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb;\n\
		src[5] = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb;\n\
		src[6] = SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb;\n\
		src[7] = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb;\n\
		src[8] = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb;\n\
		\n\
		float v[9];\n\
		v[0] = reduce(src[0]);\n\
		v[1] = reduce(src[1]);\n\
		v[2] = reduce(src[2]);\n\
		v[3] = reduce(src[3]);\n\
		v[4] = reduce(src[4]);\n\
		v[5] = reduce(src[5]);\n\
		v[6] = reduce(src[6]);\n\
		v[7] = reduce(src[7]);\n\
		v[8] = reduce(src[8]);\n\
		\n\
		float pattern	= (float(v[0] != v[4]) * 1.0) +\n\
						  (float(v[1] != v[4]) * 2.0) +\n\
						  (float(v[2] != v[4]) * 4.0) +\n\
						  (float(v[3] != v[4]) * 8.0) +\n\
						  (float(v[5] != v[4]) * 16.0) +\n\
						  (float(v[6] != v[4]) * 32.0) +\n\
						  (float(v[7] != v[4]) * 64.0) +\n\
						  (float(v[8] != v[4]) * 128.0);\n\
		\n\
		float compare	= (float(v[1] != v[5]) * 1.0) +\n\
						  (float(v[5] != v[7]) * 2.0) +\n\
						  (float(v[7] != v[3]) * 4.0) +\n\
						  (float(v[3] != v[1]) * 8.0);\n\
		\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		float k = (f.y*2.0) + f.x;\n\
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/4.0, (compare+0.5)/16.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/4.0, (compare+0.5)/16.0));\n\
		\n\
		vec3 dst[3];\n\
		dst[0] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.r)), step(7.0*30.95/255.0, p.r)), step(6.0*30.95/255.0, p.r)), step(5.0*30.95/255.0, p.r)), step(4.0*30.95/255.0, p.r)), step(3.0*30.95/255.0, p.r)), step(2.0*30.95/255.0, p.r)), step(1.0*30.95/255.0, p.r));\n\
		dst[1] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.g)), step(7.0*30.95/255.0, p.g)), step(6.0*30.95/255.0, p.g)), step(5.0*30.95/255.0, p.g)), step(4.0*30.95/255.0, p.g)), step(3.0*30.95/255.0, p.g)), step(2.0*30.95/255.0, p.g)), step(1.0*30.95/255.0, p.g));\n\
		dst[2] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.b)), step(7.0*30.95/255.0, p.b)), step(6.0*30.95/255.0, p.b)), step(5.0*30.95/255.0, p.b)), step(4.0*30.95/255.0, p.b)), step(3.0*30.95/255.0, p.b)), step(2.0*30.95/255.0, p.b)), step(1.0*30.95/255.0, p.b));\n\
		\n\
		OUT_FRAG_COLOR.rgb = Lerp(w, dst[0], dst[1], dst[2]);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *ScalerLQ2xSFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	uniform sampler3D lut;\n\
	\n\
	vec3 Lerp(vec3 weight, vec3 p1, vec3 p2, vec3 p3)\n\
	{\n\
		return p1*weight.r + p2*weight.g + p3*weight.b;\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08\n\
	//                       05|00|01\n\
	//                       04|03|02\n\
	//\n\
	// Output Pixel Mapping:  00|01\n\
	//                        02|03\n\
	\n\
	//---------------------------------------\n\
	// LQ2xS Pixel Mapping:   0|1|2\n\
	//                        3|4|5\n\
	//                        6|7|8\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[9];\n\
		src[0] = SAMPLE3_TEX_RECT(tex, texCoord[6]).rgb;\n\
		src[1] = SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb;\n\
		src[2] = SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb;\n\
		src[3] = SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb;\n\
		src[4] = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb;\n\
		src[5] = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb;\n\
		src[6] = SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb;\n\
		src[7] = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb;\n\
		src[8] = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb;\n\
		\n\
		float b[9];\n\
		float minBright = 10.0;\n\
		float maxBright = 0.0;\n\
		\n\
		for (int i = 0; i < 9; i++)\n\
		{\n\
			b[i] = (src[i].r + src[i].r + src[i].r) + (src[i].g + src[i].g + src[i].g) + (src[i].b + src[i].b);\n\
			minBright = min(minBright, b[i]);\n\
			maxBright = max(maxBright, b[i]);\n\
		}\n\
		\n\
		float diffBright = (maxBright - minBright) / 16.0;\n\
		float pattern = step((0.5*1.0/127.5), diffBright) *	((float(abs(b[0] - b[4]) > diffBright) * 1.0) +\n\
															 (float(abs(b[1] - b[4]) > diffBright) * 2.0) +\n\
															 (float(abs(b[2] - b[4]) > diffBright) * 4.0) +\n\
															 (float(abs(b[3] - b[4]) > diffBright) * 8.0) +\n\
															 (float(abs(b[5] - b[4]) > diffBright) * 16.0) +\n\
															 (float(abs(b[6] - b[4]) > diffBright) * 32.0) +\n\
															 (float(abs(b[7] - b[4]) > diffBright) * 64.0) +\n\
															 (float(abs(b[8] - b[4]) > diffBright) * 128.0));\n\
		\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		float k = (f.y*2.0) + f.x;\n\
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/4.0, 0.5/16.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/4.0, 0.5/16.0));\n\
		\n\
		vec3 dst[3];\n\
		dst[0] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.r)), step(7.0*30.95/255.0, p.r)), step(6.0*30.95/255.0, p.r)), step(5.0*30.95/255.0, p.r)), step(4.0*30.95/255.0, p.r)), step(3.0*30.95/255.0, p.r)), step(2.0*30.95/255.0, p.r)), step(1.0*30.95/255.0, p.r));\n\
		dst[1] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.g)), step(7.0*30.95/255.0, p.g)), step(6.0*30.95/255.0, p.g)), step(5.0*30.95/255.0, p.g)), step(4.0*30.95/255.0, p.g)), step(3.0*30.95/255.0, p.g)), step(2.0*30.95/255.0, p.g)), step(1.0*30.95/255.0, p.g));\n\
		dst[2] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.b)), step(7.0*30.95/255.0, p.b)), step(6.0*30.95/255.0, p.b)), step(5.0*30.95/255.0, p.b)), step(4.0*30.95/255.0, p.b)), step(3.0*30.95/255.0, p.b)), step(2.0*30.95/255.0, p.b)), step(1.0*30.95/255.0, p.b));\n\
		\n\
		OUT_FRAG_COLOR.rgb = Lerp(w, dst[0], dst[1], dst[2]);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *ScalerHQ2xFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	uniform sampler3D lut;\n\
	\n\
	bool InterpDiff(vec3 p1, vec3 p2)\n\
	{\n\
		vec3 diff = p1 - p2;\n\
		vec3 yuv =	vec3( diff.r + diff.g + diff.b,\n\
					      diff.r - diff.b,\n\
						 -diff.r + (2.0*diff.g) - diff.b );\n\
		yuv = abs(yuv);\n\
		\n\
		return any( greaterThan(yuv, vec3(192.0/255.0, 28.0/255.0, 48.0/255.0)) );\n\
	}\n\
	\n\
	vec3 Lerp(vec3 weight, vec3 p1, vec3 p2, vec3 p3)\n\
	{\n\
		return p1*weight.r + p2*weight.g + p3*weight.b;\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08\n\
	//                       05|00|01\n\
	//                       04|03|02\n\
	//\n\
	// Output Pixel Mapping:  00|01\n\
	//                        02|03\n\
	\n\
	//---------------------------------------\n\
	// HQ2x Pixel Mapping:    0|1|2\n\
	//                        3|4|5\n\
	//                        6|7|8\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[9];\n\
		src[0] = SAMPLE3_TEX_RECT(tex, texCoord[6]).rgb;\n\
		src[1] = SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb;\n\
		src[2] = SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb;\n\
		src[3] = SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb;\n\
		src[4] = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb;\n\
		src[5] = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb;\n\
		src[6] = SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb;\n\
		src[7] = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb;\n\
		src[8] = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb;\n\
		\n\
		float pattern	= (float(InterpDiff(src[0], src[4])) * 1.0) +\n\
						  (float(InterpDiff(src[1], src[4])) * 2.0) +\n\
						  (float(InterpDiff(src[2], src[4])) * 4.0) +\n\
						  (float(InterpDiff(src[3], src[4])) * 8.0) +\n\
						  (float(InterpDiff(src[5], src[4])) * 16.0) +\n\
						  (float(InterpDiff(src[6], src[4])) * 32.0) +\n\
						  (float(InterpDiff(src[7], src[4])) * 64.0) +\n\
						  (float(InterpDiff(src[8], src[4])) * 128.0);\n\
		\n\
		float compare	= (float(InterpDiff(src[1], src[5])) * 1.0) +\n\
						  (float(InterpDiff(src[5], src[7])) * 2.0) +\n\
						  (float(InterpDiff(src[7], src[3])) * 4.0) +\n\
						  (float(InterpDiff(src[3], src[1])) * 8.0);\n\
		\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		float k = (f.y*2.0) + f.x;\n\
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/4.0, (compare+0.5)/16.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/4.0, (compare+0.5)/16.0));\n\
		\n\
		vec3 dst[3];\n\
		dst[0] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.r)), step(7.0*30.95/255.0, p.r)), step(6.0*30.95/255.0, p.r)), step(5.0*30.95/255.0, p.r)), step(4.0*30.95/255.0, p.r)), step(3.0*30.95/255.0, p.r)), step(2.0*30.95/255.0, p.r)), step(1.0*30.95/255.0, p.r));\n\
		dst[1] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.g)), step(7.0*30.95/255.0, p.g)), step(6.0*30.95/255.0, p.g)), step(5.0*30.95/255.0, p.g)), step(4.0*30.95/255.0, p.g)), step(3.0*30.95/255.0, p.g)), step(2.0*30.95/255.0, p.g)), step(1.0*30.95/255.0, p.g));\n\
		dst[2] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.b)), step(7.0*30.95/255.0, p.b)), step(6.0*30.95/255.0, p.b)), step(5.0*30.95/255.0, p.b)), step(4.0*30.95/255.0, p.b)), step(3.0*30.95/255.0, p.b)), step(2.0*30.95/255.0, p.b)), step(1.0*30.95/255.0, p.b));\n\
		\n\
		OUT_FRAG_COLOR.rgb = Lerp(w, dst[0], dst[1], dst[2]);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *ScalerHQ2xSFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	uniform sampler3D lut;\n\
	\n\
	vec3 Lerp(vec3 weight, vec3 p1, vec3 p2, vec3 p3)\n\
	{\n\
		return p1*weight.r + p2*weight.g + p3*weight.b;\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08\n\
	//                       05|00|01\n\
	//                       04|03|02\n\
	//\n\
	// Output Pixel Mapping:  00|01\n\
	//                        02|03\n\
	\n\
	//---------------------------------------\n\
	// HQ2xS Pixel Mapping:   0|1|2\n\
	//                        3|4|5\n\
	//                        6|7|8\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[9];\n\
		src[0] = SAMPLE3_TEX_RECT(tex, texCoord[6]).rgb;\n\
		src[1] = SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb;\n\
		src[2] = SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb;\n\
		src[3] = SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb;\n\
		src[4] = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb;\n\
		src[5] = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb;\n\
		src[6] = SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb;\n\
		src[7] = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb;\n\
		src[8] = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb;\n\
		\n\
		float b[9];\n\
		float minBright = 10.0;\n\
		float maxBright = 0.0;\n\
		\n\
		for (int i = 0; i < 9; i++)\n\
		{\n\
			b[i] = (src[i].r + src[i].r + src[i].r) + (src[i].g + src[i].g + src[i].g) + (src[i].b + src[i].b);\n\
			minBright = min(minBright, b[i]);\n\
			maxBright = max(maxBright, b[i]);\n\
		}\n\
		\n\
		float diffBright = (maxBright - minBright) * (7.0/16.0);\n\
		float pattern = step((3.5*7.0/892.5), diffBright) *	((float(abs(b[0] - b[4]) > diffBright) * 1.0) +\n\
															 (float(abs(b[1] - b[4]) > diffBright) * 2.0) +\n\
															 (float(abs(b[2] - b[4]) > diffBright) * 4.0) +\n\
															 (float(abs(b[3] - b[4]) > diffBright) * 8.0) +\n\
															 (float(abs(b[5] - b[4]) > diffBright) * 16.0) +\n\
															 (float(abs(b[6] - b[4]) > diffBright) * 32.0) +\n\
															 (float(abs(b[7] - b[4]) > diffBright) * 64.0) +\n\
															 (float(abs(b[8] - b[4]) > diffBright) * 128.0));\n\
		\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		float k = (f.y*2.0) + f.x;\n\
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/4.0, 0.5/16.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/4.0, 0.5/16.0));\n\
		\n\
		vec3 dst[3];\n\
		dst[0] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.r)), step(7.0*30.95/255.0, p.r)), step(6.0*30.95/255.0, p.r)), step(5.0*30.95/255.0, p.r)), step(4.0*30.95/255.0, p.r)), step(3.0*30.95/255.0, p.r)), step(2.0*30.95/255.0, p.r)), step(1.0*30.95/255.0, p.r));\n\
		dst[1] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.g)), step(7.0*30.95/255.0, p.g)), step(6.0*30.95/255.0, p.g)), step(5.0*30.95/255.0, p.g)), step(4.0*30.95/255.0, p.g)), step(3.0*30.95/255.0, p.g)), step(2.0*30.95/255.0, p.g)), step(1.0*30.95/255.0, p.g));\n\
		dst[2] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.b)), step(7.0*30.95/255.0, p.b)), step(6.0*30.95/255.0, p.b)), step(5.0*30.95/255.0, p.b)), step(4.0*30.95/255.0, p.b)), step(3.0*30.95/255.0, p.b)), step(2.0*30.95/255.0, p.b)), step(1.0*30.95/255.0, p.b));\n\
		\n\
		OUT_FRAG_COLOR.rgb = Lerp(w, dst[0], dst[1], dst[2]);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *ScalerHQ3xFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	uniform sampler3D lut;\n\
	\n\
	bool InterpDiff(vec3 p1, vec3 p2)\n\
	{\n\
		vec3 diff = p1 - p2;\n\
		vec3 yuv =	vec3( diff.r + diff.g + diff.b,\n\
						  diff.r - diff.b,\n\
						 -diff.r + (2.0*diff.g) - diff.b );\n\
		yuv = abs(yuv);\n\
		\n\
		return any( greaterThan(yuv, vec3(192.0/255.0, 28.0/255.0, 48.0/255.0)) );\n\
	}\n\
	\n\
	vec3 Lerp(vec3 weight, vec3 p1, vec3 p2, vec3 p3)\n\
	{\n\
		return p1*weight.r + p2*weight.g + p3*weight.b;\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:   06|07|08\n\
	//                        05|00|01\n\
	//                        04|03|02\n\
	//\n\
	// Output Pixel Mapping: 00|01|02|03\n\
	//                       04|05|06|07\n\
	//                       08|09|10|11\n\
	//                       12|13|14|15\n\
	\n\
	//---------------------------------------\n\
	// HQ3x Pixel Mapping:      0|1|2\n\
	//                          3|4|5\n\
	//                          6|7|8\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[9];\n\
		src[0] = SAMPLE3_TEX_RECT(tex, texCoord[6]).rgb;\n\
		src[1] = SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb;\n\
		src[2] = SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb;\n\
		src[3] = SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb;\n\
		src[4] = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb;\n\
		src[5] = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb;\n\
		src[6] = SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb;\n\
		src[7] = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb;\n\
		src[8] = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb;\n\
		\n\
		float pattern	= (float(InterpDiff(src[0], src[4])) * 1.0) +\n\
						  (float(InterpDiff(src[1], src[4])) * 2.0) +\n\
						  (float(InterpDiff(src[2], src[4])) * 4.0) +\n\
						  (float(InterpDiff(src[3], src[4])) * 8.0) +\n\
						  (float(InterpDiff(src[5], src[4])) * 16.0) +\n\
						  (float(InterpDiff(src[6], src[4])) * 32.0) +\n\
						  (float(InterpDiff(src[7], src[4])) * 64.0) +\n\
						  (float(InterpDiff(src[8], src[4])) * 128.0);\n\
		\n\
		float compare	= (float(InterpDiff(src[1], src[5])) * 1.0) +\n\
						  (float(InterpDiff(src[5], src[7])) * 2.0) +\n\
						  (float(InterpDiff(src[7], src[3])) * 4.0) +\n\
						  (float(InterpDiff(src[3], src[1])) * 8.0);\n\
		\n\
		vec2 f = mix( vec2(0.0,0.0), mix(vec2(1.0,1.0), vec2(2.0,2.0), step(0.6, fract(texCoord[0]))), step(0.3, fract(texCoord[0])) );\n\
		float k = (f.y*3.0) + f.x;\n\
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/9.0, (compare+0.5)/16.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/9.0, (compare+0.5)/16.0));\n\
		\n\
		vec3 dst[3];\n\
		dst[0] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.r)), step(7.0*30.95/255.0, p.r)), step(6.0*30.95/255.0, p.r)), step(5.0*30.95/255.0, p.r)), step(4.0*30.95/255.0, p.r)), step(3.0*30.95/255.0, p.r)), step(2.0*30.95/255.0, p.r)), step(1.0*30.95/255.0, p.r));\n\
		dst[1] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.g)), step(7.0*30.95/255.0, p.g)), step(6.0*30.95/255.0, p.g)), step(5.0*30.95/255.0, p.g)), step(4.0*30.95/255.0, p.g)), step(3.0*30.95/255.0, p.g)), step(2.0*30.95/255.0, p.g)), step(1.0*30.95/255.0, p.g));\n\
		dst[2] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.b)), step(7.0*30.95/255.0, p.b)), step(6.0*30.95/255.0, p.b)), step(5.0*30.95/255.0, p.b)), step(4.0*30.95/255.0, p.b)), step(3.0*30.95/255.0, p.b)), step(2.0*30.95/255.0, p.b)), step(1.0*30.95/255.0, p.b));\n\
		\n\
		OUT_FRAG_COLOR.rgb = Lerp(w, dst[0], dst[1], dst[2]);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *ScalerHQ3xSFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	uniform sampler3D lut;\n\
	\n\
	vec3 Lerp(vec3 weight, vec3 p1, vec3 p2, vec3 p3)\n\
	{\n\
		return p1*weight.r + p2*weight.g + p3*weight.b;\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:   06|07|08\n\
	//                        05|00|01\n\
	//                        04|03|02\n\
	//\n\
	// Output Pixel Mapping: 00|01|02|03\n\
	//                       04|05|06|07\n\
	//                       08|09|10|11\n\
	//                       12|13|14|15\n\
	\n\
	//---------------------------------------\n\
	// HQ3xS Pixel Mapping:     0|1|2\n\
	//                          3|4|5\n\
	//                          6|7|8\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[9];\n\
		src[0] = SAMPLE3_TEX_RECT(tex, texCoord[6]).rgb;\n\
		src[1] = SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb;\n\
		src[2] = SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb;\n\
		src[3] = SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb;\n\
		src[4] = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb;\n\
		src[5] = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb;\n\
		src[6] = SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb;\n\
		src[7] = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb;\n\
		src[8] = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb;\n\
		\n\
		float b[9];\n\
		float minBright = 10.0;\n\
		float maxBright = 0.0;\n\
		\n\
		for (int i = 0; i < 9; i++)\n\
		{\n\
			b[i] = (src[i].r + src[i].r + src[i].r) + (src[i].g + src[i].g + src[i].g) + (src[i].b + src[i].b);\n\
			minBright = min(minBright, b[i]);\n\
			maxBright = max(maxBright, b[i]);\n\
		}\n\
		\n\
		float diffBright = (maxBright - minBright) * (7.0/16.0);\n\
		float pattern = step((3.5*7.0/892.5), diffBright) *	((float(abs(b[0] - b[4]) > diffBright) * 1.0) +\n\
															 (float(abs(b[1] - b[4]) > diffBright) * 2.0) +\n\
															 (float(abs(b[2] - b[4]) > diffBright) * 4.0) +\n\
															 (float(abs(b[3] - b[4]) > diffBright) * 8.0) +\n\
															 (float(abs(b[5] - b[4]) > diffBright) * 16.0) +\n\
															 (float(abs(b[6] - b[4]) > diffBright) * 32.0) +\n\
															 (float(abs(b[7] - b[4]) > diffBright) * 64.0) +\n\
															 (float(abs(b[8] - b[4]) > diffBright) * 128.0));\n\
		\n\
		vec2 f = mix( vec2(0.0,0.0), mix(vec2(1.0,1.0), vec2(2.0,2.0), step(0.6, fract(texCoord[0]))), step(0.3, fract(texCoord[0])) );\n\
		float k = (f.y*3.0) + f.x;\n\
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/9.0, 0.5/16.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/9.0, 0.5/16.0));\n\
		\n\
		vec3 dst[3];\n\
		dst[0] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.r)), step(7.0*30.95/255.0, p.r)), step(6.0*30.95/255.0, p.r)), step(5.0*30.95/255.0, p.r)), step(4.0*30.95/255.0, p.r)), step(3.0*30.95/255.0, p.r)), step(2.0*30.95/255.0, p.r)), step(1.0*30.95/255.0, p.r));\n\
		dst[1] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.g)), step(7.0*30.95/255.0, p.g)), step(6.0*30.95/255.0, p.g)), step(5.0*30.95/255.0, p.g)), step(4.0*30.95/255.0, p.g)), step(3.0*30.95/255.0, p.g)), step(2.0*30.95/255.0, p.g)), step(1.0*30.95/255.0, p.g));\n\
		dst[2] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.b)), step(7.0*30.95/255.0, p.b)), step(6.0*30.95/255.0, p.b)), step(5.0*30.95/255.0, p.b)), step(4.0*30.95/255.0, p.b)), step(3.0*30.95/255.0, p.b)), step(2.0*30.95/255.0, p.b)), step(1.0*30.95/255.0, p.b));\n\
		\n\
		OUT_FRAG_COLOR.rgb = Lerp(w, dst[0], dst[1], dst[2]);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *ScalerHQ4xFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	uniform sampler3D lut;\n\
	\n\
	bool InterpDiff(vec3 p1, vec3 p2)\n\
	{\n\
		vec3 diff = p1 - p2;\n\
		vec3 yuv =	vec3( diff.r + diff.g + diff.b,\n\
						  diff.r - diff.b,\n\
						 -diff.r + (2.0*diff.g) - diff.b );\n\
		yuv = abs(yuv);\n\
		\n\
		return any( greaterThan(yuv, vec3(192.0/255.0, 28.0/255.0, 48.0/255.0)) );\n\
	}\n\
	\n\
	vec3 Lerp(vec3 weight, vec3 p1, vec3 p2, vec3 p3)\n\
	{\n\
		return p1*weight.r + p2*weight.g + p3*weight.b;\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:   06|07|08\n\
	//                        05|00|01\n\
	//                        04|03|02\n\
	//\n\
	// Output Pixel Mapping: 00|01|02|03\n\
	//                       04|05|06|07\n\
	//                       08|09|10|11\n\
	//                       12|13|14|15\n\
	\n\
	//---------------------------------------\n\
	// HQ4x Pixel Mapping:      0|1|2\n\
	//                          3|4|5\n\
	//                          6|7|8\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[9];\n\
		src[0] = SAMPLE3_TEX_RECT(tex, texCoord[6]).rgb;\n\
		src[1] = SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb;\n\
		src[2] = SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb;\n\
		src[3] = SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb;\n\
		src[4] = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb;\n\
		src[5] = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb;\n\
		src[6] = SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb;\n\
		src[7] = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb;\n\
		src[8] = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb;\n\
		\n\
		float pattern	= (float(InterpDiff(src[0], src[4])) * 1.0) +\n\
						  (float(InterpDiff(src[1], src[4])) * 2.0) +\n\
						  (float(InterpDiff(src[2], src[4])) * 4.0) +\n\
						  (float(InterpDiff(src[3], src[4])) * 8.0) +\n\
						  (float(InterpDiff(src[5], src[4])) * 16.0) +\n\
						  (float(InterpDiff(src[6], src[4])) * 32.0) +\n\
						  (float(InterpDiff(src[7], src[4])) * 64.0) +\n\
						  (float(InterpDiff(src[8], src[4])) * 128.0);\n\
		\n\
		float compare	= (float(InterpDiff(src[1], src[5])) * 1.0) +\n\
						  (float(InterpDiff(src[5], src[7])) * 2.0) +\n\
						  (float(InterpDiff(src[7], src[3])) * 4.0) +\n\
						  (float(InterpDiff(src[3], src[1])) * 8.0);\n\
		\n\
		vec2 f = mix( mix(vec2(0.0,0.0), vec2(1.0,1.0), step(0.25, fract(texCoord[0]))), mix(vec2(2.0,2.0), vec2(3.0,3.0), step(0.75, fract(texCoord[0]))), step(0.5, fract(texCoord[0])) );\n\
		float k = (f.y*4.0) + f.x;\n\
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/16.0, (compare+0.5)/16.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/16.0, (compare+0.5)/16.0));\n\
		\n\
		vec3 dst[3];\n\
		dst[0] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.r)), step(7.0*30.95/255.0, p.r)), step(6.0*30.95/255.0, p.r)), step(5.0*30.95/255.0, p.r)), step(4.0*30.95/255.0, p.r)), step(3.0*30.95/255.0, p.r)), step(2.0*30.95/255.0, p.r)), step(1.0*30.95/255.0, p.r));\n\
		dst[1] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.g)), step(7.0*30.95/255.0, p.g)), step(6.0*30.95/255.0, p.g)), step(5.0*30.95/255.0, p.g)), step(4.0*30.95/255.0, p.g)), step(3.0*30.95/255.0, p.g)), step(2.0*30.95/255.0, p.g)), step(1.0*30.95/255.0, p.g));\n\
		dst[2] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.b)), step(7.0*30.95/255.0, p.b)), step(6.0*30.95/255.0, p.b)), step(5.0*30.95/255.0, p.b)), step(4.0*30.95/255.0, p.b)), step(3.0*30.95/255.0, p.b)), step(2.0*30.95/255.0, p.b)), step(1.0*30.95/255.0, p.b));\n\
		\n\
		OUT_FRAG_COLOR.rgb = Lerp(w, dst[0], dst[1], dst[2]);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *ScalerHQ4xSFragShader_110 = {"\
	VARYING vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	uniform sampler3D lut;\n\
	\n\
	vec3 Lerp(vec3 weight, vec3 p1, vec3 p2, vec3 p3)\n\
	{\n\
		return p1*weight.r + p2*weight.g + p3*weight.b;\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:   06|07|08\n\
	//                        05|00|01\n\
	//                        04|03|02\n\
	//\n\
	// Output Pixel Mapping: 00|01|02|03\n\
	//                       04|05|06|07\n\
	//                       08|09|10|11\n\
	//                       12|13|14|15\n\
	\n\
	//---------------------------------------\n\
	// HQ4xS Pixel Mapping:     0|1|2\n\
	//                          3|4|5\n\
	//                          6|7|8\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[9];\n\
		src[0] = SAMPLE3_TEX_RECT(tex, texCoord[6]).rgb;\n\
		src[1] = SAMPLE3_TEX_RECT(tex, texCoord[7]).rgb;\n\
		src[2] = SAMPLE3_TEX_RECT(tex, texCoord[8]).rgb;\n\
		src[3] = SAMPLE3_TEX_RECT(tex, texCoord[5]).rgb;\n\
		src[4] = SAMPLE3_TEX_RECT(tex, texCoord[0]).rgb;\n\
		src[5] = SAMPLE3_TEX_RECT(tex, texCoord[1]).rgb;\n\
		src[6] = SAMPLE3_TEX_RECT(tex, texCoord[4]).rgb;\n\
		src[7] = SAMPLE3_TEX_RECT(tex, texCoord[3]).rgb;\n\
		src[8] = SAMPLE3_TEX_RECT(tex, texCoord[2]).rgb;\n\
		\n\
		float b[9];\n\
		float minBright = 10.0;\n\
		float maxBright = 0.0;\n\
		\n\
		for (int i = 0; i < 9; i++)\n\
		{\n\
			b[i] = (src[i].r + src[i].r + src[i].r) + (src[i].g + src[i].g + src[i].g) + (src[i].b + src[i].b);\n\
			minBright = min(minBright, b[i]);\n\
			maxBright = max(maxBright, b[i]);\n\
		}\n\
		\n\
		float diffBright = (maxBright - minBright) * (7.0/16.0);\n\
		float pattern = step((3.5*7.0/892.5), diffBright) *	((float(abs(b[0] - b[4]) > diffBright) * 1.0) +\n\
															 (float(abs(b[1] - b[4]) > diffBright) * 2.0) +\n\
															 (float(abs(b[2] - b[4]) > diffBright) * 4.0) +\n\
															 (float(abs(b[3] - b[4]) > diffBright) * 8.0) +\n\
															 (float(abs(b[5] - b[4]) > diffBright) * 16.0) +\n\
															 (float(abs(b[6] - b[4]) > diffBright) * 32.0) +\n\
															 (float(abs(b[7] - b[4]) > diffBright) * 64.0) +\n\
															 (float(abs(b[8] - b[4]) > diffBright) * 128.0));\n\
		\n\
		vec2 f = mix( mix(vec2(0.0,0.0), vec2(1.0,1.0), step(0.25, fract(texCoord[0]))), mix(vec2(2.0,2.0), vec2(3.0,3.0), step(0.75, fract(texCoord[0]))), step(0.5, fract(texCoord[0])) );\n\
		float k = (f.y*4.0) + f.x;\n\
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/16.0, 0.5/16.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/16.0, 0.5/16.0));\n\
		\n\
		vec3 dst[3];\n\
		dst[0] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.r)), step(7.0*30.95/255.0, p.r)), step(6.0*30.95/255.0, p.r)), step(5.0*30.95/255.0, p.r)), step(4.0*30.95/255.0, p.r)), step(3.0*30.95/255.0, p.r)), step(2.0*30.95/255.0, p.r)), step(1.0*30.95/255.0, p.r));\n\
		dst[1] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.g)), step(7.0*30.95/255.0, p.g)), step(6.0*30.95/255.0, p.g)), step(5.0*30.95/255.0, p.g)), step(4.0*30.95/255.0, p.g)), step(3.0*30.95/255.0, p.g)), step(2.0*30.95/255.0, p.g)), step(1.0*30.95/255.0, p.g));\n\
		dst[2] = mix(src[0], mix(src[1], mix(src[2], mix(src[3], mix(src[4], mix(src[5], mix(src[6], mix(src[7], src[8], step(8.0*30.95/255.0, p.b)), step(7.0*30.95/255.0, p.b)), step(6.0*30.95/255.0, p.b)), step(5.0*30.95/255.0, p.b)), step(4.0*30.95/255.0, p.b)), step(3.0*30.95/255.0, p.b)), step(2.0*30.95/255.0, p.b)), step(1.0*30.95/255.0, p.b));\n\
		\n\
		OUT_FRAG_COLOR.rgb = Lerp(w, dst[0], dst[1], dst[2]);\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *Scaler2xBRZFragShader_110 = {"\
	#define BLEND_NONE 0\n\
	#define BLEND_NORMAL 1\n\
	#define BLEND_DOMINANT 2\n\
	#define LUMINANCE_WEIGHT 1.0\n\
	#define EQUAL_COLOR_TOLERANCE 30.0/255.0\n\
	#define STEEP_DIRECTION_THRESHOLD 2.2\n\
	#define DOMINANT_DIRECTION_THRESHOLD 3.6\n\
	#define M_PI 3.1415926535897932384626433832795\n\
	\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
	VARYING vec2 texCoord[25];\n\
#else\n\
	VARYING vec2 texCoord[16];\n\
#endif\n\
	uniform sampler2DRect tex;\n\
	\n\
	float reduce(const vec3 color)\n\
	{\n\
		return dot(color, vec3(65536.0, 256.0, 1.0));\n\
	}\n\
	\n\
	float DistYCbCr(const vec3 pixA, const vec3 pixB)\n\
	{\n\
		const vec3 w = vec3(0.2627, 0.6780, 0.0593);\n\
		const float scaleB = 0.5 / (1.0 - w.b);\n\
		const float scaleR = 0.5 / (1.0 - w.r);\n\
		vec3 diff = pixA - pixB;\n\
		float Y = dot(diff, w);\n\
		float Cb = scaleB * (diff.b - Y);\n\
		float Cr = scaleR * (diff.r - Y);\n\
		\n\
		return sqrt( ((LUMINANCE_WEIGHT*Y) * (LUMINANCE_WEIGHT*Y)) + (Cb * Cb) + (Cr * Cr) );\n\
	}\n\
	\n\
	bool IsPixEqual(const vec3 pixA, const vec3 pixB)\n\
	{\n\
		return (DistYCbCr(pixA, pixB) < EQUAL_COLOR_TOLERANCE);\n\
	}\n\
	\n\
	bool IsBlendingNeeded(const ivec4 blend)\n\
	{\n\
		return any(notEqual(blend, ivec4(BLEND_NONE)));\n\
	}\n\
	\n\
#if GPU_TIER < SHADERSUPPORT_MID_TIER\n\
	void ScalePixel(const ivec4 blend, const vec3 k[9], inout vec3 dst[4])\n\
	{\n\
		// This is the optimized version of xBRZ's blending logic. It's behavior\n\
		// should be identical to the original blending logic below.\n\
		float v0 = reduce(k[0]);\n\
		float v4 = reduce(k[4]);\n\
		float v5 = reduce(k[5]);\n\
		float v7 = reduce(k[7]);\n\
		float v8 = reduce(k[8]);\n\
		\n\
		float dist_01_04 = DistYCbCr(k[1], k[4]);\n\
		float dist_03_08 = DistYCbCr(k[3], k[8]);\n\
		bool haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_01_04 <= dist_03_08) && (v0 != v4) && (v5 != v4);\n\
		bool haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_03_08 <= dist_01_04) && (v0 != v8) && (v7 != v8);\n\
		bool needBlend = (blend[2] != BLEND_NONE);\n\
		bool doLineBlend = (  blend[2] >= BLEND_DOMINANT ||\n\
					       !((blend[1] != BLEND_NONE && !IsPixEqual(k[0], k[4])) ||\n\
					         (blend[3] != BLEND_NONE && !IsPixEqual(k[0], k[8])) ||\n\
					         (IsPixEqual(k[4], k[3]) && IsPixEqual(k[3], k[2]) && IsPixEqual(k[2], k[1]) && IsPixEqual(k[1], k[8]) && !IsPixEqual(k[0], k[2])) ) );\n\
		\n\
		vec3 blendPix = ( DistYCbCr(k[0], k[1]) <= DistYCbCr(k[0], k[3]) ) ? k[1] : k[3];\n\
		dst[1] = mix(dst[1], blendPix, (needBlend && doLineBlend && haveSteepLine) ? 0.25 : 0.00);\n\
		dst[2] = mix(dst[2], blendPix, (needBlend) ? ((doLineBlend) ? ((haveShallowLine) ? ((haveSteepLine) ? 5.0/6.0 : 0.75) : ((haveSteepLine) ? 0.75 : 0.50)) : 1.0 - (M_PI/4.0)) : 0.00);\n\
		dst[3] = mix(dst[3], blendPix, (needBlend && doLineBlend && haveShallowLine) ? 0.25 : 0.00);\n\
		\n\
		// Let's keep xBRZ's original blending logic around for reference.\n\
		/*\n\
		if (blend[2] == BLEND_NONE)\n\
		{\n\
			return;\n\
		}\n\
		\n\
		vec3 blendPix = ( DistYCbCr(k[0], k[1]) <= DistYCbCr(k[0], k[3]) ) ? k[1] : k[3];\n\
		\n\
		if ( DoLineBlend(blend, k) )\n\
		{\n\
			float v0 = reduce(k[0]);\n\
			float v4 = reduce(k[4]);\n\
			float v5 = reduce(k[5]);\n\
			float v7 = reduce(k[7]);\n\
			float v8 = reduce(k[8]);\n\
			\n\
			float dist_01_04 = DistYCbCr(k[1], k[4]);\n\
			float dist_03_08 = DistYCbCr(k[3], k[8]);\n\
			bool haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_01_04 <= dist_03_08) && (v0 != v4) && (v5 != v4);\n\
			bool haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_03_08 <= dist_01_04) && (v0 != v8) && (v7 != v8);\n\
			\n\
			if (haveShallowLine)\n\
			{\n\
				if (haveSteepLine)\n\
				{\n\
					// Blend line steep and shallow\n\
					dst[3] = mix(dst[3], blendPix, 0.25);\n\
					dst[1] = mix(dst[1], blendPix, 0.25);\n\
					dst[2] = mix(dst[2], blendPix, 5.0/6.0);\n\
				}\n\
				else\n\
				{\n\
					// Blend line shallow\n\
					dst[3] = mix(dst[3], blendPix, 0.25);\n\
					dst[2] = mix(dst[2], blendPix, 0.75);\n\
				}\n\
			}\n\
			else\n\
			{\n\
				if (haveSteepLine)\n\
				{\n\
					// Blend line steep\n\
					dst[1] = mix(dst[1], blendPix, 0.25);\n\
					dst[2] = mix(dst[2], blendPix, 0.75);\n\
				}\n\
				else\n\
				{\n\
					// Blend line diagonal\n\
					dst[2] = mix(dst[2], blendPix, 0.50);\n\
				}\n\
			}\n\
		}\n\
		else\n\
		{\n\
			// Blend corner\n\
			dst[2] = mix(dst[2], blendPix, 1.0 - (M_PI/4.0));\n\
		}\n\
		*/\n\
	}\n\
#endif\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  --|21|22|23|--\n\
	//                       19|06|07|08|09\n\
	//                       18|05|00|01|10\n\
	//                       17|04|03|02|11\n\
	//                       --|15|14|13|--\n\
	//\n\
	// Output Pixel Mapping:     00|01\n\
	//                           03|02\n\
	\n\
	void main()\n\
	{\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
		vec3 src[25];\n\
		src[ 0] = SAMPLE3_TEX_RECT(tex, texCoord[ 0]).rgb;\n\
		src[ 1] = SAMPLE3_TEX_RECT(tex, texCoord[ 1]).rgb;\n\
		src[ 2] = SAMPLE3_TEX_RECT(tex, texCoord[ 2]).rgb;\n\
		src[ 3] = SAMPLE3_TEX_RECT(tex, texCoord[ 3]).rgb;\n\
		src[ 4] = SAMPLE3_TEX_RECT(tex, texCoord[ 4]).rgb;\n\
		src[ 5] = SAMPLE3_TEX_RECT(tex, texCoord[ 5]).rgb;\n\
		src[ 6] = SAMPLE3_TEX_RECT(tex, texCoord[ 6]).rgb;\n\
		src[ 7] = SAMPLE3_TEX_RECT(tex, texCoord[ 7]).rgb;\n\
		src[ 8] = SAMPLE3_TEX_RECT(tex, texCoord[ 8]).rgb;\n\
		src[ 9] = SAMPLE3_TEX_RECT(tex, texCoord[ 9]).rgb;\n\
		src[10] = SAMPLE3_TEX_RECT(tex, texCoord[10]).rgb;\n\
		src[11] = SAMPLE3_TEX_RECT(tex, texCoord[11]).rgb;\n\
		src[12] = SAMPLE3_TEX_RECT(tex, texCoord[12]).rgb;\n\
		src[13] = SAMPLE3_TEX_RECT(tex, texCoord[13]).rgb;\n\
		src[14] = SAMPLE3_TEX_RECT(tex, texCoord[14]).rgb;\n\
		src[15] = SAMPLE3_TEX_RECT(tex, texCoord[15]).rgb;\n\
		src[16] = SAMPLE3_TEX_RECT(tex, texCoord[16]).rgb;\n\
		src[17] = SAMPLE3_TEX_RECT(tex, texCoord[17]).rgb;\n\
		src[18] = SAMPLE3_TEX_RECT(tex, texCoord[18]).rgb;\n\
		src[19] = SAMPLE3_TEX_RECT(tex, texCoord[19]).rgb;\n\
		src[20] = SAMPLE3_TEX_RECT(tex, texCoord[20]).rgb;\n\
		src[21] = SAMPLE3_TEX_RECT(tex, texCoord[21]).rgb;\n\
		src[22] = SAMPLE3_TEX_RECT(tex, texCoord[22]).rgb;\n\
		src[23] = SAMPLE3_TEX_RECT(tex, texCoord[23]).rgb;\n\
		src[24] = SAMPLE3_TEX_RECT(tex, texCoord[24]).rgb;\n\
#else\n\
		vec3 src[16];\n\
		src[ 0] = SAMPLE3_TEX_RECT(tex, texCoord[ 0]).rgb;\n\
		src[ 1] = SAMPLE3_TEX_RECT(tex, texCoord[ 1]).rgb;\n\
		src[ 2] = SAMPLE3_TEX_RECT(tex, texCoord[ 2]).rgb;\n\
		src[ 3] = SAMPLE3_TEX_RECT(tex, texCoord[ 3]).rgb;\n\
		src[ 4] = SAMPLE3_TEX_RECT(tex, texCoord[ 4]).rgb;\n\
		src[ 5] = SAMPLE3_TEX_RECT(tex, texCoord[ 5]).rgb;\n\
		src[ 6] = SAMPLE3_TEX_RECT(tex, texCoord[ 6]).rgb;\n\
		src[ 7] = SAMPLE3_TEX_RECT(tex, texCoord[ 7]).rgb;\n\
		src[ 8] = SAMPLE3_TEX_RECT(tex, texCoord[ 8]).rgb;\n\
		src[ 9] = SAMPLE3_TEX_RECT(tex, texCoord[ 9]).rgb;\n\
		src[10] = SAMPLE3_TEX_RECT(tex, texCoord[10]).rgb;\n\
		src[11] = SAMPLE3_TEX_RECT(tex, texCoord[11]).rgb;\n\
		src[12] = SAMPLE3_TEX_RECT(tex, texCoord[12]).rgb;\n\
		src[13] = SAMPLE3_TEX_RECT(tex, texCoord[13]).rgb;\n\
		src[14] = SAMPLE3_TEX_RECT(tex, texCoord[14]).rgb;\n\
		src[15] = SAMPLE3_TEX_RECT(tex, texCoord[15]).rgb;\n\
#endif\n\
		\n\
		float v[9];\n\
		v[0] = reduce(src[0]);\n\
		v[1] = reduce(src[1]);\n\
		v[2] = reduce(src[2]);\n\
		v[3] = reduce(src[3]);\n\
		v[4] = reduce(src[4]);\n\
		v[5] = reduce(src[5]);\n\
		v[6] = reduce(src[6]);\n\
		v[7] = reduce(src[7]);\n\
		v[8] = reduce(src[8]);\n\
		\n\
		ivec4 blendResult = ivec4(BLEND_NONE);\n\
		\n\
		// Preprocess corners\n\
		// Pixel Tap Mapping: --|--|--|--|--\n\
		//                    --|--|07|08|--\n\
		//                    --|05|00|01|10\n\
		//                    --|04|03|02|11\n\
		//                    --|--|14|13|--\n\
		\n\
		// Corner (1, 1)\n\
		if ( !((v[0] == v[1] && v[3] == v[2]) || (v[0] == v[3] && v[1] == v[2])) )\n\
		{\n\
			float dist_03_01 = DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + DistYCbCr(src[14], src[ 2]) + DistYCbCr(src[ 2], src[10]) + (4.0 * DistYCbCr(src[ 3], src[ 1]));\n\
			float dist_00_02 = DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src[ 3], src[13]) + DistYCbCr(src[ 7], src[ 1]) + DistYCbCr(src[ 1], src[11]) + (4.0 * DistYCbCr(src[ 0], src[ 2]));\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_03_01) < dist_00_02;\n\
			blendResult[2] = ((dist_03_01 < dist_00_02) && (v[0] != v[1]) && (v[0] != v[3])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		\n\
		// Pixel Tap Mapping: --|--|--|--|--\n\
		//                    --|06|07|--|--\n\
		//                    18|05|00|01|--\n\
		//                    17|04|03|02|--\n\
		//                    --|15|14|--|--\n\
		// Corner (0, 1)\n\
		if ( !((v[5] == v[0] && v[4] == v[3]) || (v[5] == v[4] && v[0] == v[3])) )\n\
		{\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
			float dist_04_00 = DistYCbCr(src[17], src[ 5]) + DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[15], src[ 3]) + DistYCbCr(src[ 3], src[ 1]) + (4.0 * DistYCbCr(src[ 4], src[ 0]));\n\
			float dist_05_03 = DistYCbCr(src[18], src[ 4]) + DistYCbCr(src[ 4], src[14]) + DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + (4.0 * DistYCbCr(src[ 5], src[ 3]));\n\
#else\n\
			vec3 src17 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(-2.0, 1.0)).rgb;\n\
			vec3 src18 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(-2.0, 0.0)).rgb;\n\
			float dist_04_00 = DistYCbCr(src17  , src[ 5]) + DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[15], src[ 3]) + DistYCbCr(src[ 3], src[ 1]) + (4.0 * DistYCbCr(src[ 4], src[ 0]));\n\
			float dist_05_03 = DistYCbCr(src18  , src[ 4]) + DistYCbCr(src[ 4], src[14]) + DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + (4.0 * DistYCbCr(src[ 5], src[ 3]));\n\
#endif\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_05_03) < dist_04_00;\n\
			blendResult[3] = ((dist_04_00 > dist_05_03) && (v[0] != v[5]) && (v[0] != v[3])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		// Pixel Tap Mapping: --|--|22|23|--\n\
		//                    --|06|07|08|09\n\
		//                    --|05|00|01|10\n\
		//                    --|--|03|02|--\n\
		//                    --|--|--|--|--\n\
		// Corner (1, 0)\n\
		if ( !((v[7] == v[8] && v[0] == v[1]) || (v[7] == v[0] && v[8] == v[1])) )\n\
		{\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
			float dist_00_08 = DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[ 7], src[23]) + DistYCbCr(src[ 3], src[ 1]) + DistYCbCr(src[ 1], src[ 9]) + (4.0 * DistYCbCr(src[ 0], src[ 8]));\n\
			float dist_07_01 = DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + DistYCbCr(src[22], src[ 8]) + DistYCbCr(src[ 8], src[10]) + (4.0 * DistYCbCr(src[ 7], src[ 1]));\n\
#else\n\
			vec3 src22 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(0.0, -2.0)).rgb;\n\
			vec3 src23 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(1.0, -2.0)).rgb;\n\
			float dist_00_08 = DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[ 7], src23  ) + DistYCbCr(src[ 3], src[ 1]) + DistYCbCr(src[ 1], src[ 9]) + (4.0 * DistYCbCr(src[ 0], src[ 8]));\n\
			float dist_07_01 = DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + DistYCbCr(src22  , src[ 8]) + DistYCbCr(src[ 8], src[10]) + (4.0 * DistYCbCr(src[ 7], src[ 1]));\n\
#endif\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_07_01) < dist_00_08;\n\
			blendResult[1] = ((dist_00_08 > dist_07_01) && (v[0] != v[7]) && (v[0] != v[1])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		// Pixel Tap Mapping: --|21|22|--|--\n\
		//                    19|06|07|08|--\n\
		//                    18|05|00|01|--\n\
		//                    --|04|03|--|--\n\
		//                    --|--|--|--|--\n\
		// Corner (0, 0)\n\
		if ( !((v[6] == v[7] && v[5] == v[0]) || (v[6] == v[5] && v[7] == v[0])) )\n\
		{\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
			float dist_05_07 = DistYCbCr(src[18], src[ 6]) + DistYCbCr(src[ 6], src[22]) + DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + (4.0 * DistYCbCr(src[ 5], src[ 7]));\n\
			float dist_06_00 = DistYCbCr(src[19], src[ 5]) + DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src[21], src[ 7]) + DistYCbCr(src[ 7], src[ 1]) + (4.0 * DistYCbCr(src[ 6], src[ 0]));\n\
#else\n\
			vec3 src18 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(-2.0,  0.0)).rgb;\n\
			vec3 src19 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(-2.0, -1.0)).rgb;\n\
			vec3 src21 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(-1.0, -2.0)).rgb;\n\
			vec3 src22 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2( 0.0, -2.0)).rgb;\n\
			float dist_05_07 = DistYCbCr(src18  , src[ 6]) + DistYCbCr(src[ 6], src22  ) + DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + (4.0 * DistYCbCr(src[ 5], src[ 7]));\n\
			float dist_06_00 = DistYCbCr(src19  , src[ 5]) + DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src21  , src[ 7]) + DistYCbCr(src[ 7], src[ 1]) + (4.0 * DistYCbCr(src[ 6], src[ 0]));\n\
#endif\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_05_07) < dist_06_00;\n\
			blendResult[0] = ((dist_05_07 < dist_06_00) && (v[0] != v[5]) && (v[0] != v[7])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		vec3 dst[4];\n\
		dst[0] = src[0];\n\
		dst[1] = src[0];\n\
		dst[2] = src[0];\n\
		dst[3] = src[0];\n\
		\n\
		// Scale pixel\n\
		if (IsBlendingNeeded(blendResult))\n\
		{\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
			vec4 dist_01_04 = vec4( DistYCbCr(src[1], src[4]), DistYCbCr(src[7], src[2]), DistYCbCr(src[5], src[8]), DistYCbCr(src[3], src[6]) );\n\
			vec4 dist_03_08 = vec4( DistYCbCr(src[3], src[8]), DistYCbCr(src[1], src[6]), DistYCbCr(src[7], src[4]), DistYCbCr(src[5], src[2]) );\n\
			bvec4 haveShallowLine = lessThanEqual(STEEP_DIRECTION_THRESHOLD * dist_01_04, dist_03_08);\n\
			bvec4 haveSteepLine = lessThanEqual(STEEP_DIRECTION_THRESHOLD * dist_03_08, dist_01_04);\n\
			bvec4 needBlend = notEqual( blendResult.zyxw, ivec4(BLEND_NONE) );\n\
			bvec4 doLineBlend = greaterThanEqual( blendResult.zyxw, ivec4(BLEND_DOMINANT) );\n\
			vec3 blendPix[4];\n\
			\n\
			haveShallowLine[0] = haveShallowLine[0] && (v[0] != v[4]) && (v[5] != v[4]);\n\
			haveSteepLine[0]   = haveSteepLine[0] && (v[0] != v[8]) && (v[7] != v[8]);\n\
			doLineBlend[0] = (  doLineBlend[0] ||\n\
							   !((blendResult[1] != BLEND_NONE && !IsPixEqual(src[0], src[4])) ||\n\
								 (blendResult[3] != BLEND_NONE && !IsPixEqual(src[0], src[8])) ||\n\
								 (IsPixEqual(src[4], src[3]) && IsPixEqual(src[3], src[2]) && IsPixEqual(src[2], src[1]) && IsPixEqual(src[1], src[8]) && !IsPixEqual(src[0], src[2])) ) );\n\
			blendPix[0] = ( DistYCbCr(src[0], src[1]) <= DistYCbCr(src[0], src[3]) ) ? src[1] : src[3];\n\
			\n\
			dst[1] = mix(dst[1], blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.25 : 0.00);\n\
			dst[2] = mix(dst[2], blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? ((haveShallowLine[0]) ? ((haveSteepLine[0]) ? 5.0/6.0 : 0.75) : ((haveSteepLine[0]) ? 0.75 : 0.50)) : 1.0 - (M_PI/4.0)) : 0.00);\n\
			dst[3] = mix(dst[3], blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.25 : 0.00);\n\
			\n\
			haveShallowLine[1] = haveShallowLine[1] && (v[0] != v[2]) && (v[3] != v[2]);\n\
			haveSteepLine[1]   = haveSteepLine[1] && (v[0] != v[6]) && (v[5] != v[6]);\n\
			doLineBlend[1] = (  doLineBlend[1] ||\n\
						  !((blendResult[0] != BLEND_NONE && !IsPixEqual(src[0], src[2])) ||\n\
							(blendResult[2] != BLEND_NONE && !IsPixEqual(src[0], src[6])) ||\n\
							(IsPixEqual(src[2], src[1]) && IsPixEqual(src[1], src[8]) && IsPixEqual(src[8], src[7]) && IsPixEqual(src[7], src[6]) && !IsPixEqual(src[0], src[8])) ) );\n\
			blendPix[1] = ( DistYCbCr(src[0], src[7]) <= DistYCbCr(src[0], src[1]) ) ? src[7] : src[1];\n\
			\n\
			dst[0] = mix(dst[0], blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.25 : 0.00);\n\
			dst[1] = mix(dst[1], blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? ((haveShallowLine[1]) ? ((haveSteepLine[1]) ? 5.0/6.0 : 0.75) : ((haveSteepLine[1]) ? 0.75 : 0.50)) : 1.0 - (M_PI/4.0)) : 0.00);\n\
			dst[2] = mix(dst[2], blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.25 : 0.00);\n\
			\n\
			haveShallowLine[2] = haveShallowLine[2] && (v[0] != v[8]) && (v[1] != v[8]);\n\
			haveSteepLine[2]   = haveSteepLine[2] && (v[0] != v[4]) && (v[3] != v[4]);\n\
			doLineBlend[2] = (  doLineBlend[2] ||\n\
						  !((blendResult[3] != BLEND_NONE && !IsPixEqual(src[0], src[8])) ||\n\
							(blendResult[1] != BLEND_NONE && !IsPixEqual(src[0], src[4])) ||\n\
							(IsPixEqual(src[8], src[7]) && IsPixEqual(src[7], src[6]) && IsPixEqual(src[6], src[5]) && IsPixEqual(src[5], src[4]) && !IsPixEqual(src[0], src[6])) ) );\n\
			blendPix[2] = ( DistYCbCr(src[0], src[5]) <= DistYCbCr(src[0], src[7]) ) ? src[5] : src[7];\n\
			\n\
			dst[3] = mix(dst[3], blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.25 : 0.00);\n\
			dst[0] = mix(dst[0], blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? ((haveShallowLine[2]) ? ((haveSteepLine[2]) ? 5.0/6.0 : 0.75) : ((haveSteepLine[2]) ? 0.75 : 0.50)) : 1.0 - (M_PI/4.0)) : 0.00);\n\
			dst[1] = mix(dst[1], blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.25 : 0.00);\n\
			\n\
			haveShallowLine[3] = haveShallowLine[3] && (v[0] != v[6]) && (v[7] != v[6]);\n\
			haveSteepLine[3]   = haveSteepLine[3] && (v[0] != v[2]) && (v[1] != v[2]);\n\
			doLineBlend[3] = (  doLineBlend[3] ||\n\
						  !((blendResult[2] != BLEND_NONE && !IsPixEqual(src[0], src[6])) ||\n\
							(blendResult[0] != BLEND_NONE && !IsPixEqual(src[0], src[2])) ||\n\
							(IsPixEqual(src[6], src[5]) && IsPixEqual(src[5], src[4]) && IsPixEqual(src[4], src[3]) && IsPixEqual(src[3], src[2]) && !IsPixEqual(src[0], src[4])) ) );\n\
			blendPix[3] = ( DistYCbCr(src[0], src[3]) <= DistYCbCr(src[0], src[5]) ) ? src[3] : src[5];\n\
			\n\
			dst[2] = mix(dst[2], blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.25 : 0.00);\n\
			dst[3] = mix(dst[3], blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? ((haveShallowLine[3]) ? ((haveSteepLine[3]) ? 5.0/6.0 : 0.75) : ((haveSteepLine[3]) ? 0.75 : 0.50)) : 1.0 - (M_PI/4.0)) : 0.00);\n\
			dst[0] = mix(dst[0], blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.25 : 0.00);\n\
#else\n\
			vec3 k[9];\n\
			vec3 tempDst3;\n\
			\n\
			k[0] = src[0];\n\
			k[1] = src[1];\n\
			k[2] = src[2];\n\
			k[3] = src[3];\n\
			k[4] = src[4];\n\
			k[5] = src[5];\n\
			k[6] = src[6];\n\
			k[7] = src[7];\n\
			k[8] = src[8];\n\
			ScalePixel(blendResult.xyzw, k, dst);\n\
			\n\
			k[1] = src[7];\n\
			k[2] = src[8];\n\
			k[3] = src[1];\n\
			k[4] = src[2];\n\
			k[5] = src[3];\n\
			k[6] = src[4];\n\
			k[7] = src[5];\n\
			k[8] = src[6];\n\
			tempDst3 = dst[3];\n\
			dst[3] = dst[2];\n\
			dst[2] = dst[1];\n\
			dst[1] = dst[0];\n\
			dst[0] = tempDst3;\n\
			ScalePixel(blendResult.wxyz, k, dst);\n\
			\n\
			k[1] = src[5];\n\
			k[2] = src[6];\n\
			k[3] = src[7];\n\
			k[4] = src[8];\n\
			k[5] = src[1];\n\
			k[6] = src[2];\n\
			k[7] = src[3];\n\
			k[8] = src[4];\n\
			tempDst3 = dst[3];\n\
			dst[3] = dst[2];\n\
			dst[2] = dst[1];\n\
			dst[1] = dst[0];\n\
			dst[0] = tempDst3;\n\
			ScalePixel(blendResult.zwxy, k, dst);\n\
			\n\
			k[1] = src[3];\n\
			k[2] = src[4];\n\
			k[3] = src[5];\n\
			k[4] = src[6];\n\
			k[5] = src[7];\n\
			k[6] = src[8];\n\
			k[7] = src[1];\n\
			k[8] = src[2];\n\
			tempDst3 = dst[3];\n\
			dst[3] = dst[2];\n\
			dst[2] = dst[1];\n\
			dst[1] = dst[0];\n\
			dst[0] = tempDst3;\n\
			ScalePixel(blendResult.yzwx, k, dst);\n\
			\n\
			// Rotate the destination pixels back to 0 degrees.\n\
			tempDst3 = dst[3];\n\
			dst[3] = dst[2];\n\
			dst[2] = dst[1];\n\
			dst[1] = dst[0];\n\
			dst[0] = tempDst3;\n\
#endif\n\
		}\n\
		\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		OUT_FRAG_COLOR.rgb =	mix( mix(dst[0], dst[1], f.x),\n\
								     mix(dst[3], dst[2], f.x), f.y );\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *Scaler3xBRZFragShader_110 = {"\
	#define BLEND_NONE 0\n\
	#define BLEND_NORMAL 1\n\
	#define BLEND_DOMINANT 2\n\
	#define LUMINANCE_WEIGHT 1.0\n\
	#define EQUAL_COLOR_TOLERANCE 30.0/255.0\n\
	#define STEEP_DIRECTION_THRESHOLD 2.2\n\
	#define DOMINANT_DIRECTION_THRESHOLD 3.6\n\
	#define M_PI 3.1415926535897932384626433832795\n\
	\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
	VARYING vec2 texCoord[25];\n\
#else\n\
	VARYING vec2 texCoord[16];\n\
#endif\n\
	uniform sampler2DRect tex;\n\
	\n\
	float reduce(const vec3 color)\n\
	{\n\
		return dot(color, vec3(65536.0, 256.0, 1.0));\n\
	}\n\
	\n\
	float DistYCbCr(const vec3 pixA, const vec3 pixB)\n\
	{\n\
		const vec3 w = vec3(0.2627, 0.6780, 0.0593);\n\
		const float scaleB = 0.5 / (1.0 - w.b);\n\
		const float scaleR = 0.5 / (1.0 - w.r);\n\
		vec3 diff = pixA - pixB;\n\
		float Y = dot(diff, w);\n\
		float Cb = scaleB * (diff.b - Y);\n\
		float Cr = scaleR * (diff.r - Y);\n\
		\n\
		return sqrt( ((LUMINANCE_WEIGHT*Y) * (LUMINANCE_WEIGHT*Y)) + (Cb * Cb) + (Cr * Cr) );\n\
	}\n\
	\n\
	bool IsPixEqual(const vec3 pixA, const vec3 pixB)\n\
	{\n\
		return (DistYCbCr(pixA, pixB) < EQUAL_COLOR_TOLERANCE);\n\
	}\n\
	\n\
	bool IsBlendingNeeded(const ivec4 blend)\n\
	{\n\
		return any(notEqual(blend, ivec4(BLEND_NONE)));\n\
	}\n\
	\n\
#if GPU_TIER < SHADERSUPPORT_MID_TIER\n\
	void ScalePixel(const ivec4 blend, const vec3 k[9], inout vec3 dst[9])\n\
	{\n\
		// This is the optimized version of xBRZ's blending logic. It's behavior\n\
		// should be identical to the original blending logic below.\n\
		float v0 = reduce(k[0]);\n\
		float v4 = reduce(k[4]);\n\
		float v5 = reduce(k[5]);\n\
		float v7 = reduce(k[7]);\n\
		float v8 = reduce(k[8]);\n\
		\n\
		float dist_01_04 = DistYCbCr(k[1], k[4]);\n\
		float dist_03_08 = DistYCbCr(k[3], k[8]);\n\
		bool haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_01_04 <= dist_03_08) && (v0 != v4) && (v5 != v4);\n\
		bool haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_03_08 <= dist_01_04) && (v0 != v8) && (v7 != v8);\n\
		bool needBlend = (blend[2] != BLEND_NONE);\n\
		bool doLineBlend = (  blend[2] >= BLEND_DOMINANT ||\n\
					       !((blend[1] != BLEND_NONE && !IsPixEqual(k[0], k[4])) ||\n\
					         (blend[3] != BLEND_NONE && !IsPixEqual(k[0], k[8])) ||\n\
					         (IsPixEqual(k[4], k[3]) && IsPixEqual(k[3], k[2]) && IsPixEqual(k[2], k[1]) && IsPixEqual(k[1], k[8]) && !IsPixEqual(k[0], k[2])) ) );\n\
		\n\
		vec3 blendPix = ( DistYCbCr(k[0], k[1]) <= DistYCbCr(k[0], k[3]) ) ? k[1] : k[3];\n\
		dst[1] = mix(dst[1], blendPix, (needBlend && doLineBlend) ? ((haveSteepLine) ? 0.750 : ((haveShallowLine) ? 0.250 : 0.125)) : 0.000);\n\
		dst[2] = mix(dst[2], blendPix, (needBlend) ? ((doLineBlend) ? ((!haveShallowLine && !haveSteepLine) ? 0.875 : 1.000) : 0.4545939598) : 0.000);\n\
		dst[3] = mix(dst[3], blendPix, (needBlend && doLineBlend) ? ((haveShallowLine) ? 0.750 : ((haveSteepLine) ? 0.250 : 0.125)) : 0.000);\n\
		dst[4] = mix(dst[4], blendPix, (needBlend && doLineBlend && haveShallowLine) ? 0.250 : 0.000);\n\
		dst[8] = mix(dst[8], blendPix, (needBlend && doLineBlend && haveSteepLine) ? 0.250 : 0.000);\n\
		\n\
		// Let's keep xBRZ's original blending logic around for reference.\n\
		/*\n\
		if (blend[2] == BLEND_NONE)\n\
		{\n\
			return;\n\
		}\n\
		\n\
		vec3 blendPix = ( DistYCbCr(k[0], k[1]) <= DistYCbCr(k[0], k[3]) ) ? k[1] : k[3];\n\
		\n\
		if ( DoLineBlend(blend, k) )\n\
		{\n\
			float v0 = reduce(k[0]);\n\
			float v4 = reduce(k[4]);\n\
			float v5 = reduce(k[5]);\n\
			float v7 = reduce(k[7]);\n\
			float v8 = reduce(k[8]);\n\
			\n\
			float dist_01_04 = DistYCbCr(k[1], k[4]);\n\
			float dist_03_08 = DistYCbCr(k[3], k[8]);\n\
			bool haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_01_04 <= dist_03_08) && (v0 != v4) && (v5 != v4);\n\
			bool haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_03_08 <= dist_01_04) && (v0 != v8) && (v7 != v8);\n\
			\n\
			if (haveShallowLine)\n\
			{\n\
				if (haveSteepLine)\n\
				{\n\
					// Blend line steep and shallow\n\
					dst[4] = mix(dst[4], blendPix, 0.25);\n\
					dst[8] = mix(dst[8], blendPix, 0.25);\n\
					dst[3] = mix(dst[3], blendPix, 0.75);\n\
					dst[1] = mix(dst[1], blendPix, 0.75);\n\
					dst[2] = mix(dst[2], blendPix, 1.00);\n\
				}\n\
				else\n\
				{\n\
					// Blend line shallow\n\
					dst[4] = mix(dst[4], blendPix, 0.25);\n\
					dst[1] = mix(dst[1], blendPix, 0.25);\n\
					dst[3] = mix(dst[3], blendPix, 0.75);\n\
					dst[2] = mix(dst[2], blendPix, 1.00);\n\
				}\n\
			}\n\
			else\n\
			{\n\
				if (haveSteepLine)\n\
				{\n\
					// Blend line steep\n\
					dst[8] = mix(dst[8], blendPix, 0.25);\n\
					dst[3] = mix(dst[3], blendPix, 0.25);\n\
					dst[1] = mix(dst[1], blendPix, 0.75);\n\
					dst[2] = mix(dst[2], blendPix, 1.00);\n\
				}\n\
				else\n\
				{\n\
					// Blend line diagonal\n\
					dst[1] = mix(dst[1], blendPix, 0.125);\n\
					dst[3] = mix(dst[3], blendPix, 0.125);\n\
					dst[2] = mix(dst[2], blendPix, 0.875);\n\
				}\n\
			}\n\
		}\n\
		else\n\
		{\n\
			// Blend corner\n\
			dst[2] = mix(dst[2], blendPix, 0.4545939598);\n\
		}\n\
		*/\n\
	}\n\
#endif\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  --|21|22|23|--\n\
	//                       19|06|07|08|09\n\
	//                       18|05|00|01|10\n\
	//                       17|04|03|02|11\n\
	//                       --|15|14|13|--\n\
	//\n\
	// Output Pixel Mapping:    06|07|08\n\
	//                          05|00|01\n\
	//                          04|03|02\n\
	\n\
	void main()\n\
	{\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
		vec3 src[25];\n\
		src[ 0] = SAMPLE3_TEX_RECT(tex, texCoord[ 0]);\n\
		src[ 1] = SAMPLE3_TEX_RECT(tex, texCoord[ 1]);\n\
		src[ 2] = SAMPLE3_TEX_RECT(tex, texCoord[ 2]);\n\
		src[ 3] = SAMPLE3_TEX_RECT(tex, texCoord[ 3]);\n\
		src[ 4] = SAMPLE3_TEX_RECT(tex, texCoord[ 4]);\n\
		src[ 5] = SAMPLE3_TEX_RECT(tex, texCoord[ 5]);\n\
		src[ 6] = SAMPLE3_TEX_RECT(tex, texCoord[ 6]);\n\
		src[ 7] = SAMPLE3_TEX_RECT(tex, texCoord[ 7]);\n\
		src[ 8] = SAMPLE3_TEX_RECT(tex, texCoord[ 8]);\n\
		src[ 9] = SAMPLE3_TEX_RECT(tex, texCoord[ 9]);\n\
		src[10] = SAMPLE3_TEX_RECT(tex, texCoord[10]);\n\
		src[11] = SAMPLE3_TEX_RECT(tex, texCoord[11]);\n\
		src[12] = SAMPLE3_TEX_RECT(tex, texCoord[12]);\n\
		src[13] = SAMPLE3_TEX_RECT(tex, texCoord[13]);\n\
		src[14] = SAMPLE3_TEX_RECT(tex, texCoord[14]);\n\
		src[15] = SAMPLE3_TEX_RECT(tex, texCoord[15]);\n\
		src[16] = SAMPLE3_TEX_RECT(tex, texCoord[16]);\n\
		src[17] = SAMPLE3_TEX_RECT(tex, texCoord[17]);\n\
		src[18] = SAMPLE3_TEX_RECT(tex, texCoord[18]);\n\
		src[19] = SAMPLE3_TEX_RECT(tex, texCoord[19]);\n\
		src[20] = SAMPLE3_TEX_RECT(tex, texCoord[20]);\n\
		src[21] = SAMPLE3_TEX_RECT(tex, texCoord[21]);\n\
		src[22] = SAMPLE3_TEX_RECT(tex, texCoord[22]);\n\
		src[23] = SAMPLE3_TEX_RECT(tex, texCoord[23]);\n\
		src[24] = SAMPLE3_TEX_RECT(tex, texCoord[24]);\n\
#else\n\
		vec3 src[16];\n\
		src[ 0] = SAMPLE3_TEX_RECT(tex, texCoord[ 0]);\n\
		src[ 1] = SAMPLE3_TEX_RECT(tex, texCoord[ 1]);\n\
		src[ 2] = SAMPLE3_TEX_RECT(tex, texCoord[ 2]);\n\
		src[ 3] = SAMPLE3_TEX_RECT(tex, texCoord[ 3]);\n\
		src[ 4] = SAMPLE3_TEX_RECT(tex, texCoord[ 4]);\n\
		src[ 5] = SAMPLE3_TEX_RECT(tex, texCoord[ 5]);\n\
		src[ 6] = SAMPLE3_TEX_RECT(tex, texCoord[ 6]);\n\
		src[ 7] = SAMPLE3_TEX_RECT(tex, texCoord[ 7]);\n\
		src[ 8] = SAMPLE3_TEX_RECT(tex, texCoord[ 8]);\n\
		src[ 9] = SAMPLE3_TEX_RECT(tex, texCoord[ 9]);\n\
		src[10] = SAMPLE3_TEX_RECT(tex, texCoord[10]);\n\
		src[11] = SAMPLE3_TEX_RECT(tex, texCoord[11]);\n\
		src[12] = SAMPLE3_TEX_RECT(tex, texCoord[12]);\n\
		src[13] = SAMPLE3_TEX_RECT(tex, texCoord[13]);\n\
		src[14] = SAMPLE3_TEX_RECT(tex, texCoord[14]);\n\
		src[15] = SAMPLE3_TEX_RECT(tex, texCoord[15]);\n\
#endif\n\
		\n\
		float v[9];\n\
		v[0] = reduce(src[0]);\n\
		v[1] = reduce(src[1]);\n\
		v[2] = reduce(src[2]);\n\
		v[3] = reduce(src[3]);\n\
		v[4] = reduce(src[4]);\n\
		v[5] = reduce(src[5]);\n\
		v[6] = reduce(src[6]);\n\
		v[7] = reduce(src[7]);\n\
		v[8] = reduce(src[8]);\n\
		\n\
		ivec4 blendResult = ivec4(BLEND_NONE);\n\
		\n\
		// Preprocess corners\n\
		// Pixel Tap Mapping: --|--|--|--|--\n\
		//                    --|--|07|08|--\n\
		//                    --|05|00|01|10\n\
		//                    --|04|03|02|11\n\
		//                    --|--|14|13|--\n\
		\n\
		// Corner (1, 1)\n\
		if ( !((v[0] == v[1] && v[3] == v[2]) || (v[0] == v[3] && v[1] == v[2])) )\n\
		{\n\
			float dist_03_01 = DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + DistYCbCr(src[14], src[ 2]) + DistYCbCr(src[ 2], src[10]) + (4.0 * DistYCbCr(src[ 3], src[ 1]));\n\
			float dist_00_02 = DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src[ 3], src[13]) + DistYCbCr(src[ 7], src[ 1]) + DistYCbCr(src[ 1], src[11]) + (4.0 * DistYCbCr(src[ 0], src[ 2]));\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_03_01) < dist_00_02;\n\
			blendResult[2] = ((dist_03_01 < dist_00_02) && (v[0] != v[1]) && (v[0] != v[3])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		\n\
		// Pixel Tap Mapping: --|--|--|--|--\n\
		//                    --|06|07|--|--\n\
		//                    18|05|00|01|--\n\
		//                    17|04|03|02|--\n\
		//                    --|15|14|--|--\n\
		// Corner (0, 1)\n\
		if ( !((v[5] == v[0] && v[4] == v[3]) || (v[5] == v[4] && v[0] == v[3])) )\n\
		{\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
			float dist_04_00 = DistYCbCr(src[17], src[ 5]) + DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[15], src[ 3]) + DistYCbCr(src[ 3], src[ 1]) + (4.0 * DistYCbCr(src[ 4], src[ 0]));\n\
			float dist_05_03 = DistYCbCr(src[18], src[ 4]) + DistYCbCr(src[ 4], src[14]) + DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + (4.0 * DistYCbCr(src[ 5], src[ 3]));\n\
#else\n\
			vec3 src17 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(-2.0, 1.0));\n\
			vec3 src18 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(-2.0, 0.0));\n\
			float dist_04_00 = DistYCbCr(src17  , src[ 5]) + DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[15], src[ 3]) + DistYCbCr(src[ 3], src[ 1]) + (4.0 * DistYCbCr(src[ 4], src[ 0]));\n\
			float dist_05_03 = DistYCbCr(src18  , src[ 4]) + DistYCbCr(src[ 4], src[14]) + DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + (4.0 * DistYCbCr(src[ 5], src[ 3]));\n\
#endif\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_05_03) < dist_04_00;\n\
			blendResult[3] = ((dist_04_00 > dist_05_03) && (v[0] != v[5]) && (v[0] != v[3])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		// Pixel Tap Mapping: --|--|22|23|--\n\
		//                    --|06|07|08|09\n\
		//                    --|05|00|01|10\n\
		//                    --|--|03|02|--\n\
		//                    --|--|--|--|--\n\
		// Corner (1, 0)\n\
		if ( !((v[7] == v[8] && v[0] == v[1]) || (v[7] == v[0] && v[8] == v[1])) )\n\
		{\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
			float dist_00_08 = DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[ 7], src[23]) + DistYCbCr(src[ 3], src[ 1]) + DistYCbCr(src[ 1], src[ 9]) + (4.0 * DistYCbCr(src[ 0], src[ 8]));\n\
			float dist_07_01 = DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + DistYCbCr(src[22], src[ 8]) + DistYCbCr(src[ 8], src[10]) + (4.0 * DistYCbCr(src[ 7], src[ 1]));\n\
#else\n\
			vec3 src22 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(0.0, -2.0));\n\
			vec3 src23 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(1.0, -2.0));\n\
			float dist_00_08 = DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[ 7], src23  ) + DistYCbCr(src[ 3], src[ 1]) + DistYCbCr(src[ 1], src[ 9]) + (4.0 * DistYCbCr(src[ 0], src[ 8]));\n\
			float dist_07_01 = DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + DistYCbCr(src22  , src[ 8]) + DistYCbCr(src[ 8], src[10]) + (4.0 * DistYCbCr(src[ 7], src[ 1]));\n\
#endif\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_07_01) < dist_00_08;\n\
			blendResult[1] = ((dist_00_08 > dist_07_01) && (v[0] != v[7]) && (v[0] != v[1])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		// Pixel Tap Mapping: --|21|22|--|--\n\
		//                    19|06|07|08|--\n\
		//                    18|05|00|01|--\n\
		//                    --|04|03|--|--\n\
		//                    --|--|--|--|--\n\
		// Corner (0, 0)\n\
		if ( !((v[6] == v[7] && v[5] == v[0]) || (v[6] == v[5] && v[7] == v[0])) )\n\
		{\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
			float dist_05_07 = DistYCbCr(src[18], src[ 6]) + DistYCbCr(src[ 6], src[22]) + DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + (4.0 * DistYCbCr(src[ 5], src[ 7]));\n\
			float dist_06_00 = DistYCbCr(src[19], src[ 5]) + DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src[21], src[ 7]) + DistYCbCr(src[ 7], src[ 1]) + (4.0 * DistYCbCr(src[ 6], src[ 0]));\n\
#else\n\
			vec3 src18 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(-2.0,  0.0));\n\
			vec3 src19 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(-2.0, -1.0));\n\
			vec3 src21 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2(-1.0, -2.0));\n\
			vec3 src22 = SAMPLE3_TEX_RECT(tex, texCoord[0] + vec2( 0.0, -2.0));\n\
			float dist_05_07 = DistYCbCr(src18  , src[ 6]) + DistYCbCr(src[ 6], src22  ) + DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + (4.0 * DistYCbCr(src[ 5], src[ 7]));\n\
			float dist_06_00 = DistYCbCr(src19  , src[ 5]) + DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src21  , src[ 7]) + DistYCbCr(src[ 7], src[ 1]) + (4.0 * DistYCbCr(src[ 6], src[ 0]));\n\
#endif\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_05_07) < dist_06_00;\n\
			blendResult[0] = ((dist_05_07 < dist_06_00) && (v[0] != v[5]) && (v[0] != v[7])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		vec3 dst[9];\n\
		dst[0] = src[0];\n\
		dst[1] = src[0];\n\
		dst[2] = src[0];\n\
		dst[3] = src[0];\n\
		dst[4] = src[0];\n\
		dst[5] = src[0];\n\
		dst[6] = src[0];\n\
		dst[7] = src[0];\n\
		dst[8] = src[0];\n\
		\n\
		// Scale pixel\n\
		if (IsBlendingNeeded(blendResult))\n\
		{\n\
#if GPU_TIER >= SHADERSUPPORT_MID_TIER\n\
			vec4 dist_01_04 = vec4( DistYCbCr(src[1], src[4]), DistYCbCr(src[7], src[2]), DistYCbCr(src[5], src[8]), DistYCbCr(src[3], src[6]) );\n\
			vec4 dist_03_08 = vec4( DistYCbCr(src[3], src[8]), DistYCbCr(src[1], src[6]), DistYCbCr(src[7], src[4]), DistYCbCr(src[5], src[2]) );\n\
			bvec4 haveShallowLine = lessThanEqual(STEEP_DIRECTION_THRESHOLD * dist_01_04, dist_03_08);\n\
			bvec4 haveSteepLine = lessThanEqual(STEEP_DIRECTION_THRESHOLD * dist_03_08, dist_01_04);\n\
			bvec4 needBlend = notEqual( blendResult.zyxw, ivec4(BLEND_NONE) );\n\
			bvec4 doLineBlend = greaterThanEqual( blendResult.zyxw, ivec4(BLEND_DOMINANT) );\n\
			vec3 blendPix[4];\n\
			\n\
			haveShallowLine[0] = haveShallowLine[0] && (v[0] != v[4]) && (v[5] != v[4]);\n\
			haveSteepLine[0]   = haveSteepLine[0] && (v[0] != v[8]) && (v[7] != v[8]);\n\
			doLineBlend[0] = (  doLineBlend[0] ||\n\
							   !((blendResult[1] != BLEND_NONE && !IsPixEqual(src[0], src[4])) ||\n\
								 (blendResult[3] != BLEND_NONE && !IsPixEqual(src[0], src[8])) ||\n\
								 (IsPixEqual(src[4], src[3]) && IsPixEqual(src[3], src[2]) && IsPixEqual(src[2], src[1]) && IsPixEqual(src[1], src[8]) && !IsPixEqual(src[0], src[2])) ) );\n\
			blendPix[0] = ( DistYCbCr(src[0], src[1]) <= DistYCbCr(src[0], src[3]) ) ? src[1] : src[3];\n\
			\n\
			dst[1] = mix(dst[1], blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((haveSteepLine[0]) ? 0.750 : ((haveShallowLine[0]) ? 0.250 : 0.125)) : 0.000);\n\
			dst[2] = mix(dst[2], blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? ((!haveShallowLine[0] && !haveSteepLine[0]) ? 0.875 : 1.000) : 0.4545939598) : 0.000);\n\
			dst[3] = mix(dst[3], blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((haveShallowLine[0]) ? 0.750 : ((haveSteepLine[0]) ? 0.250 : 0.125)) : 0.000);\n\
			dst[4] = mix(dst[4], blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.250 : 0.000);\n\
			dst[8] = mix(dst[8], blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.250 : 0.000);\n\
			\n\
			haveShallowLine[1] = haveShallowLine[1] && (v[0] != v[2]) && (v[3] != v[2]);\n\
			haveSteepLine[1]   = haveSteepLine[1] && (v[0] != v[6]) && (v[5] != v[6]);\n\
			doLineBlend[1] = (  doLineBlend[1] ||\n\
						  !((blendResult[0] != BLEND_NONE && !IsPixEqual(src[0], src[2])) ||\n\
							(blendResult[2] != BLEND_NONE && !IsPixEqual(src[0], src[6])) ||\n\
							(IsPixEqual(src[2], src[1]) && IsPixEqual(src[1], src[8]) && IsPixEqual(src[8], src[7]) && IsPixEqual(src[7], src[6]) && !IsPixEqual(src[0], src[8])) ) );\n\
			blendPix[1] = ( DistYCbCr(src[0], src[7]) <= DistYCbCr(src[0], src[1]) ) ? src[7] : src[1];\n\
			\n\
			dst[7] = mix(dst[7], blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((haveSteepLine[1]) ? 0.750 : ((haveShallowLine[1]) ? 0.250 : 0.125)) : 0.000);\n\
			dst[8] = mix(dst[8], blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? ((!haveShallowLine[1] && !haveSteepLine[1]) ? 0.875 : 1.000) : 0.4545939598) : 0.000);\n\
			dst[1] = mix(dst[1], blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((haveShallowLine[1]) ? 0.750 : ((haveSteepLine[1]) ? 0.250 : 0.125)) : 0.000);\n\
			dst[2] = mix(dst[2], blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.250 : 0.000);\n\
			dst[6] = mix(dst[6], blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.250 : 0.000);\n\
			\n\
			haveShallowLine[2] = haveShallowLine[2] && (v[0] != v[8]) && (v[1] != v[8]);\n\
			haveSteepLine[2]   = haveSteepLine[2] && (v[0] != v[4]) && (v[3] != v[4]);\n\
			doLineBlend[2] = (  doLineBlend[2] ||\n\
						  !((blendResult[3] != BLEND_NONE && !IsPixEqual(src[0], src[8])) ||\n\
							(blendResult[1] != BLEND_NONE && !IsPixEqual(src[0], src[4])) ||\n\
							(IsPixEqual(src[8], src[7]) && IsPixEqual(src[7], src[6]) && IsPixEqual(src[6], src[5]) && IsPixEqual(src[5], src[4]) && !IsPixEqual(src[0], src[6])) ) );\n\
			blendPix[2] = ( DistYCbCr(src[0], src[5]) <= DistYCbCr(src[0], src[7]) ) ? src[5] : src[7];\n\
			\n\
			dst[5] = mix(dst[5], blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((haveSteepLine[2]) ? 0.750 : ((haveShallowLine[2]) ? 0.250 : 0.125)) : 0.000);\n\
			dst[6] = mix(dst[6], blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? ((!haveShallowLine[2] && !haveSteepLine[2]) ? 0.875 : 1.000) : 0.4545939598) : 0.000);\n\
			dst[7] = mix(dst[7], blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((haveShallowLine[2]) ? 0.750 : ((haveSteepLine[2]) ? 0.250 : 0.125)) : 0.000);\n\
			dst[8] = mix(dst[8], blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.250 : 0.000);\n\
			dst[4] = mix(dst[4], blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.250 : 0.000);\n\
			\n\
			haveShallowLine[3] = haveShallowLine[3] && (v[0] != v[6]) && (v[7] != v[6]);\n\
			haveSteepLine[3]   = haveSteepLine[3] && (v[0] != v[2]) && (v[1] != v[2]);\n\
			doLineBlend[3] = (  doLineBlend[3] ||\n\
						  !((blendResult[2] != BLEND_NONE && !IsPixEqual(src[0], src[6])) ||\n\
							(blendResult[0] != BLEND_NONE && !IsPixEqual(src[0], src[2])) ||\n\
							(IsPixEqual(src[6], src[5]) && IsPixEqual(src[5], src[4]) && IsPixEqual(src[4], src[3]) && IsPixEqual(src[3], src[2]) && !IsPixEqual(src[0], src[4])) ) );\n\
			blendPix[3] = ( DistYCbCr(src[0], src[3]) <= DistYCbCr(src[0], src[5]) ) ? src[3] : src[5];\n\
			\n\
			dst[3] = mix(dst[3], blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((haveSteepLine[3]) ? 0.750 : ((haveShallowLine[3]) ? 0.250 : 0.125)) : 0.000);\n\
			dst[4] = mix(dst[4], blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? ((!haveShallowLine[3] && !haveSteepLine[3]) ? 0.875 : 1.000) : 0.4545939598) : 0.000);\n\
			dst[5] = mix(dst[5], blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((haveShallowLine[3]) ? 0.750 : ((haveSteepLine[3]) ? 0.250 : 0.125)) : 0.000);\n\
			dst[6] = mix(dst[6], blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.250 : 0.000);\n\
			dst[2] = mix(dst[2], blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.250 : 0.000);\n\
#else\n\
			vec3 k[9];\n\
			vec3 tempDst8;\n\
			vec3 tempDst7;\n\
			\n\
			k[8] = src[8];\n\
			k[7] = src[7];\n\
			k[6] = src[6];\n\
			k[5] = src[5];\n\
			k[4] = src[4];\n\
			k[3] = src[3];\n\
			k[2] = src[2];\n\
			k[1] = src[1];\n\
			k[0] = src[0];\n\
			ScalePixel(blendResult.xyzw, k, dst);\n\
			\n\
			k[8] = src[6];\n\
			k[7] = src[5];\n\
			k[6] = src[4];\n\
			k[5] = src[3];\n\
			k[4] = src[2];\n\
			k[3] = src[1];\n\
			k[2] = src[8];\n\
			k[1] = src[7];\n\
			tempDst8 = dst[8];\n\
			tempDst7 = dst[7];\n\
			dst[8] = dst[6];\n\
			dst[7] = dst[5];\n\
			dst[6] = dst[4];\n\
			dst[5] = dst[3];\n\
			dst[4] = dst[2];\n\
			dst[3] = dst[1];\n\
			dst[2] = tempDst8;\n\
			dst[1] = tempDst7;\n\
			ScalePixel(blendResult.wxyz, k, dst);\n\
			\n\
			k[8] = src[4];\n\
			k[7] = src[3];\n\
			k[6] = src[2];\n\
			k[5] = src[1];\n\
			k[4] = src[8];\n\
			k[3] = src[7];\n\
			k[2] = src[6];\n\
			k[1] = src[5];\n\
			tempDst8 = dst[8];\n\
			tempDst7 = dst[7];\n\
			dst[8] = dst[6];\n\
			dst[7] = dst[5];\n\
			dst[6] = dst[4];\n\
			dst[5] = dst[3];\n\
			dst[4] = dst[2];\n\
			dst[3] = dst[1];\n\
			dst[2] = tempDst8;\n\
			dst[1] = tempDst7;\n\
			ScalePixel(blendResult.zwxy, k, dst);\n\
			\n\
			k[8] = src[2];\n\
			k[7] = src[1];\n\
			k[6] = src[8];\n\
			k[5] = src[7];\n\
			k[4] = src[6];\n\
			k[3] = src[5];\n\
			k[2] = src[4];\n\
			k[1] = src[3];\n\
			tempDst8 = dst[8];\n\
			tempDst7 = dst[7];\n\
			dst[8] = dst[6];\n\
			dst[7] = dst[5];\n\
			dst[6] = dst[4];\n\
			dst[5] = dst[3];\n\
			dst[4] = dst[2];\n\
			dst[3] = dst[1];\n\
			dst[2] = tempDst8;\n\
			dst[1] = tempDst7;\n\
			ScalePixel(blendResult.yzwx, k, dst);\n\
			\n\
			// Rotate the destination pixels back to 0 degrees.\n\
			tempDst8 = dst[8];\n\
			tempDst7 = dst[7];\n\
			dst[8] = dst[6];\n\
			dst[7] = dst[5];\n\
			dst[6] = dst[4];\n\
			dst[5] = dst[3];\n\
			dst[4] = dst[2];\n\
			dst[3] = dst[1];\n\
			dst[2] = tempDst8;\n\
			dst[1] = tempDst7;\n\
#endif\n\
		}\n\
		\n\
		vec2 f = fract(texCoord[0]);\n\
		OUT_FRAG_COLOR.rgb =	mix( mix(     dst[6], mix(dst[7], dst[8], step(0.6, f.x)), step(0.3, f.x)),\n\
							         mix( mix(dst[5], mix(dst[0], dst[1], step(0.6, f.x)), step(0.3, f.x)),\n\
							              mix(dst[4], mix(dst[3], dst[2], step(0.6, f.x)), step(0.3, f.x)), step(0.6, f.y)),\n\
								step(0.3, f.y) );\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *Scaler4xBRZFragShader_110 = {"\
	#define BLEND_NONE 0\n\
	#define BLEND_NORMAL 1\n\
	#define BLEND_DOMINANT 2\n\
	#define LUMINANCE_WEIGHT 1.0\n\
	#define EQUAL_COLOR_TOLERANCE 30.0/255.0\n\
	#define STEEP_DIRECTION_THRESHOLD 2.2\n\
	#define DOMINANT_DIRECTION_THRESHOLD 3.6\n\
	#define M_PI 3.1415926535897932384626433832795\n\
	\n\
	// Let's not even bother trying to support GPUs below Mid-tier.\n\
	// The xBRZ pixel scalers are already pretty hefty as-is, and\n\
	// this shader, having to calculate 16 pixel locations, is the\n\
	// heftiest of all of them. Trust me -- older GPUs just can't\n\
	// handle this one.\n\
	\n\
	VARYING vec2 texCoord[25];\n\
	uniform sampler2DRect tex;\n\
	\n\
	float reduce(const vec3 color)\n\
	{\n\
		return dot(color, vec3(65536.0, 256.0, 1.0));\n\
	}\n\
	\n\
	float DistYCbCr(const vec3 pixA, const vec3 pixB)\n\
	{\n\
		const vec3 w = vec3(0.2627, 0.6780, 0.0593);\n\
		const float scaleB = 0.5 / (1.0 - w.b);\n\
		const float scaleR = 0.5 / (1.0 - w.r);\n\
		vec3 diff = pixA - pixB;\n\
		float Y = dot(diff, w);\n\
		float Cb = scaleB * (diff.b - Y);\n\
		float Cr = scaleR * (diff.r - Y);\n\
		\n\
		return sqrt( ((LUMINANCE_WEIGHT*Y) * (LUMINANCE_WEIGHT*Y)) + (Cb * Cb) + (Cr * Cr) );\n\
	}\n\
	\n\
	bool IsPixEqual(const vec3 pixA, const vec3 pixB)\n\
	{\n\
		return (DistYCbCr(pixA, pixB) < EQUAL_COLOR_TOLERANCE);\n\
	}\n\
	\n\
	bool IsBlendingNeeded(const ivec4 blend)\n\
	{\n\
		return any(notEqual(blend, ivec4(BLEND_NONE)));\n\
	}\n\
	\n\
	// Let's keep xBRZ's original blending logic around for reference.\n\
	/*\n\
	void ScalePixel(const ivec4 blend, const vec3 k[9], inout vec3 dst[16])\n\
	{\n\
		// This is the optimized version of xBRZ's blending logic. It's behavior\n\
		// should be identical to the original blending logic below.\n\
		float v0 = reduce(k[0]);\n\
		float v4 = reduce(k[4]);\n\
		float v5 = reduce(k[5]);\n\
		float v7 = reduce(k[7]);\n\
		float v8 = reduce(k[8]);\n\
		\n\
		float dist_01_04 = DistYCbCr(k[1], k[4]);\n\
		float dist_03_08 = DistYCbCr(k[3], k[8]);\n\
		bool haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_01_04 <= dist_03_08) && (v0 != v4) && (v5 != v4);\n\
		bool haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_03_08 <= dist_01_04) && (v0 != v8) && (v7 != v8);\n\
		bool needBlend = (blend[2] != BLEND_NONE);\n\
		bool doLineBlend = (  blend[2] >= BLEND_DOMINANT ||\n\
					       !((blend[1] != BLEND_NONE && !IsPixEqual(k[0], k[4])) ||\n\
					         (blend[3] != BLEND_NONE && !IsPixEqual(k[0], k[8])) ||\n\
					         (IsPixEqual(k[4], k[3]) && IsPixEqual(k[3], k[2]) && IsPixEqual(k[2], k[1]) && IsPixEqual(k[1], k[8]) && !IsPixEqual(k[0], k[2])) ) );\n\
		\n\
		vec3 blendPix = ( DistYCbCr(k[0], k[1]) <= DistYCbCr(k[0], k[3]) ) ? k[1] : k[3];\n\
		dst[ 2] = mix(dst[ 2], blendPix, (needBlend && doLineBlend) ? ((haveShallowLine) ? ((haveSteepLine) ? 1.0/3.0 : 0.25) : ((haveSteepLine) ? 0.25 : 0.00)) : 0.00);\n\
		dst[ 9] = mix(dst[ 9], blendPix, (needBlend && doLineBlend && haveSteepLine) ? 0.25 : 0.00);\n\
		dst[10] = mix(dst[10], blendPix, (needBlend && doLineBlend && haveSteepLine) ? 0.75 : 0.00);\n\
		dst[11] = mix(dst[11], blendPix, (needBlend) ? ((doLineBlend) ? ((haveSteepLine) ? 1.00 : ((haveShallowLine) ? 0.75 : 0.50)) : 0.08677704501) : 0.00);\n\
		dst[12] = mix(dst[12], blendPix, (needBlend) ? ((doLineBlend) ? 1.00 : 0.6848532563) : 0.00);\n\
		dst[13] = mix(dst[13], blendPix, (needBlend) ? ((doLineBlend) ? ((haveShallowLine) ? 1.00 : ((haveSteepLine) ? 0.75 : 0.50)) : 0.08677704501) : 0.00);\n\
		dst[14] = mix(dst[14], blendPix, (needBlend && doLineBlend && haveShallowLine) ? 0.75 : 0.00);\n\
		dst[15] = mix(dst[15], blendPix, (needBlend && doLineBlend && haveShallowLine) ? 0.25 : 0.00);\n\
		\n\
		if (blend[2] == BLEND_NONE)\n\
		{\n\
			return;\n\
		}\n\
		\n\
		vec3 blendPix = ( DistYCbCr(k[0], k[1]) <= DistYCbCr(k[0], k[3]) ) ? k[1] : k[3];\n\
		\n\
		if ( DoLineBlend(blend, k) )\n\
		{\n\
			float v0 = reduce(k[0]);\n\
			float v4 = reduce(k[4]);\n\
			float v5 = reduce(k[5]);\n\
			float v7 = reduce(k[7]);\n\
			float v8 = reduce(k[8]);\n\
			\n\
			float dist_01_04 = DistYCbCr(k[1], k[4]);\n\
			float dist_03_08 = DistYCbCr(k[3], k[8]);\n\
			bool haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_01_04 <= dist_03_08) && (v0 != v4) && (v5 != v4);\n\
			bool haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_03_08 <= dist_01_04) && (v0 != v8) && (v7 != v8);\n\
			\n\
			if (haveShallowLine)\n\
			{\n\
				if (haveSteepLine)\n\
				{\n\
					// Blend line steep and shallow\n\
					dst[14] = mix(dst[14], blendPix, 0.75);\n\
					dst[10] = mix(dst[10], blendPix, 0.75);\n\
					dst[15] = mix(dst[15], blendPix, 0.25);\n\
					dst[ 9] = mix(dst[ 9], blendPix, 0.25);\n\
					dst[ 2] = mix(dst[ 2], blendPix, 1.0/3.0);\n\
					dst[12] = mix(dst[12], blendPix, 1.00);\n\
					dst[13] = mix(dst[13], blendPix, 1.00);\n\
					dst[11] = mix(dst[11], blendPix, 1.00);\n\
				}\n\
				else\n\
				{\n\
					// Blend line shallow\n\
					dst[15] = mix(dst[15], blendPix, 0.25);\n\
					dst[ 2] = mix(dst[ 2], blendPix, 0.25);\n\
					dst[14] = mix(dst[14], blendPix, 0.75);\n\
					dst[11] = mix(dst[11], blendPix, 0.75);\n\
					dst[13] = mix(dst[13], blendPix, 1.00);\n\
					dst[12] = mix(dst[12], blendPix, 1.00);\n\
				}\n\
			}\n\
			else\n\
			{\n\
				if (haveSteepLine)\n\
				{\n\
					// Blend line steep\n\
					dst[ 9] = mix(dst[ 9], blendPix, 0.25);\n\
					dst[ 2] = mix(dst[ 2], blendPix, 0.25);\n\
					dst[10] = mix(dst[10], blendPix, 0.75);\n\
					dst[13] = mix(dst[13], blendPix, 0.75);\n\
					dst[11] = mix(dst[11], blendPix, 1.00);\n\
					dst[12] = mix(dst[12], blendPix, 1.00);\n\
				}\n\
				else\n\
				{\n\
					// Blend line diagonal\n\
					dst[13] = mix(dst[13], blendPix, 0.50);\n\
					dst[11] = mix(dst[11], blendPix, 0.50);\n\
					dst[12] = mix(dst[12], blendPix, 1.00);\n\
				}\n\
			}\n\
		}\n\
		else\n\
		{\n\
			// Blend corner\n\
			dst[12] = mix(dst[12], blendPix, 0.6848532563);\n\
			dst[13] = mix(dst[13], blendPix, 0.08677704501);\n\
			dst[11] = mix(dst[11], blendPix, 0.08677704501);\n\
		}\n\
	}\n\
	*/\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  --|21|22|23|--\n\
	//                       19|06|07|08|09\n\
	//                       18|05|00|01|10\n\
	//                       17|04|03|02|11\n\
	//                       --|15|14|13|--\n\
	//\n\
	// Output Pixel Mapping:  00|01|02|03\n\
	//                        04|05|06|07\n\
	//                        08|09|10|11\n\
	//                        12|13|14|15\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[25];\n\
		src[ 0] = SAMPLE3_TEX_RECT(tex, texCoord[ 0]).rgb;\n\
		src[ 1] = SAMPLE3_TEX_RECT(tex, texCoord[ 1]).rgb;\n\
		src[ 2] = SAMPLE3_TEX_RECT(tex, texCoord[ 2]).rgb;\n\
		src[ 3] = SAMPLE3_TEX_RECT(tex, texCoord[ 3]).rgb;\n\
		src[ 4] = SAMPLE3_TEX_RECT(tex, texCoord[ 4]).rgb;\n\
		src[ 5] = SAMPLE3_TEX_RECT(tex, texCoord[ 5]).rgb;\n\
		src[ 6] = SAMPLE3_TEX_RECT(tex, texCoord[ 6]).rgb;\n\
		src[ 7] = SAMPLE3_TEX_RECT(tex, texCoord[ 7]).rgb;\n\
		src[ 8] = SAMPLE3_TEX_RECT(tex, texCoord[ 8]).rgb;\n\
		src[ 9] = SAMPLE3_TEX_RECT(tex, texCoord[ 9]).rgb;\n\
		src[10] = SAMPLE3_TEX_RECT(tex, texCoord[10]).rgb;\n\
		src[11] = SAMPLE3_TEX_RECT(tex, texCoord[11]).rgb;\n\
		src[12] = SAMPLE3_TEX_RECT(tex, texCoord[12]).rgb;\n\
		src[13] = SAMPLE3_TEX_RECT(tex, texCoord[13]).rgb;\n\
		src[14] = SAMPLE3_TEX_RECT(tex, texCoord[14]).rgb;\n\
		src[15] = SAMPLE3_TEX_RECT(tex, texCoord[15]).rgb;\n\
		src[16] = SAMPLE3_TEX_RECT(tex, texCoord[16]).rgb;\n\
		src[17] = SAMPLE3_TEX_RECT(tex, texCoord[17]).rgb;\n\
		src[18] = SAMPLE3_TEX_RECT(tex, texCoord[18]).rgb;\n\
		src[19] = SAMPLE3_TEX_RECT(tex, texCoord[19]).rgb;\n\
		src[20] = SAMPLE3_TEX_RECT(tex, texCoord[20]).rgb;\n\
		src[21] = SAMPLE3_TEX_RECT(tex, texCoord[21]).rgb;\n\
		src[22] = SAMPLE3_TEX_RECT(tex, texCoord[22]).rgb;\n\
		src[23] = SAMPLE3_TEX_RECT(tex, texCoord[23]).rgb;\n\
		src[24] = SAMPLE3_TEX_RECT(tex, texCoord[24]).rgb;\n\
		\n\
		float v[9];\n\
		v[0] = reduce(src[0]);\n\
		v[1] = reduce(src[1]);\n\
		v[2] = reduce(src[2]);\n\
		v[3] = reduce(src[3]);\n\
		v[4] = reduce(src[4]);\n\
		v[5] = reduce(src[5]);\n\
		v[6] = reduce(src[6]);\n\
		v[7] = reduce(src[7]);\n\
		v[8] = reduce(src[8]);\n\
		\n\
		ivec4 blendResult = ivec4(BLEND_NONE);\n\
		\n\
		// Preprocess corners\n\
		// Pixel Tap Mapping: --|--|--|--|--\n\
		//                    --|--|07|08|--\n\
		//                    --|05|00|01|10\n\
		//                    --|04|03|02|11\n\
		//                    --|--|14|13|--\n\
		\n\
		// Corner (1, 1)\n\
		if ( !((v[0] == v[1] && v[3] == v[2]) || (v[0] == v[3] && v[1] == v[2])) )\n\
		{\n\
			float dist_03_01 = DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + DistYCbCr(src[14], src[ 2]) + DistYCbCr(src[ 2], src[10]) + (4.0 * DistYCbCr(src[ 3], src[ 1]));\n\
			float dist_00_02 = DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src[ 3], src[13]) + DistYCbCr(src[ 7], src[ 1]) + DistYCbCr(src[ 1], src[11]) + (4.0 * DistYCbCr(src[ 0], src[ 2]));\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_03_01) < dist_00_02;\n\
			blendResult[2] = ((dist_03_01 < dist_00_02) && (v[0] != v[1]) && (v[0] != v[3])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		\n\
		// Pixel Tap Mapping: --|--|--|--|--\n\
		//                    --|06|07|--|--\n\
		//                    18|05|00|01|--\n\
		//                    17|04|03|02|--\n\
		//                    --|15|14|--|--\n\
		// Corner (0, 1)\n\
		if ( !((v[5] == v[0] && v[4] == v[3]) || (v[5] == v[4] && v[0] == v[3])) )\n\
		{\n\
			float dist_04_00 = DistYCbCr(src[17], src[ 5]) + DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[15], src[ 3]) + DistYCbCr(src[ 3], src[ 1]) + (4.0 * DistYCbCr(src[ 4], src[ 0]));\n\
			float dist_05_03 = DistYCbCr(src[18], src[ 4]) + DistYCbCr(src[ 4], src[14]) + DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + (4.0 * DistYCbCr(src[ 5], src[ 3]));\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_05_03) < dist_04_00;\n\
			blendResult[3] = ((dist_04_00 > dist_05_03) && (v[0] != v[5]) && (v[0] != v[3])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		// Pixel Tap Mapping: --|--|22|23|--\n\
		//                    --|06|07|08|09\n\
		//                    --|05|00|01|10\n\
		//                    --|--|03|02|--\n\
		//                    --|--|--|--|--\n\
		// Corner (1, 0)\n\
		if ( !((v[7] == v[8] && v[0] == v[1]) || (v[7] == v[0] && v[8] == v[1])) )\n\
		{\n\
			float dist_00_08 = DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[ 7], src[23]) + DistYCbCr(src[ 3], src[ 1]) + DistYCbCr(src[ 1], src[ 9]) + (4.0 * DistYCbCr(src[ 0], src[ 8]));\n\
			float dist_07_01 = DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + DistYCbCr(src[22], src[ 8]) + DistYCbCr(src[ 8], src[10]) + (4.0 * DistYCbCr(src[ 7], src[ 1]));\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_07_01) < dist_00_08;\n\
			blendResult[1] = ((dist_00_08 > dist_07_01) && (v[0] != v[7]) && (v[0] != v[1])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		// Pixel Tap Mapping: --|21|22|--|--\n\
		//                    19|06|07|08|--\n\
		//                    18|05|00|01|--\n\
		//                    --|04|03|--|--\n\
		//                    --|--|--|--|--\n\
		// Corner (0, 0)\n\
		if ( !((v[6] == v[7] && v[5] == v[0]) || (v[6] == v[5] && v[7] == v[0])) )\n\
		{\n\
			float dist_05_07 = DistYCbCr(src[18], src[ 6]) + DistYCbCr(src[ 6], src[22]) + DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + (4.0 * DistYCbCr(src[ 5], src[ 7]));\n\
			float dist_06_00 = DistYCbCr(src[19], src[ 5]) + DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src[21], src[ 7]) + DistYCbCr(src[ 7], src[ 1]) + (4.0 * DistYCbCr(src[ 6], src[ 0]));\n\
			bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_05_07) < dist_06_00;\n\
			blendResult[0] = ((dist_05_07 < dist_06_00) && (v[0] != v[5]) && (v[0] != v[7])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
		}\n\
		\n\
		vec3 newFragColor = src[0];\n\
		\n\
		// Scale pixel\n\
		if (IsBlendingNeeded(blendResult))\n\
		{\n\
			vec4 dist_01_04 = vec4( DistYCbCr(src[1], src[4]), DistYCbCr(src[7], src[2]), DistYCbCr(src[5], src[8]), DistYCbCr(src[3], src[6]) );\n\
			vec4 dist_03_08 = vec4( DistYCbCr(src[3], src[8]), DistYCbCr(src[1], src[6]), DistYCbCr(src[7], src[4]), DistYCbCr(src[5], src[2]) );\n\
			bvec4 haveShallowLine = lessThanEqual(STEEP_DIRECTION_THRESHOLD * dist_01_04, dist_03_08);\n\
			bvec4 haveSteepLine = lessThanEqual(STEEP_DIRECTION_THRESHOLD * dist_03_08, dist_01_04);\n\
			bvec4 needBlend = notEqual( blendResult.zyxw, ivec4(BLEND_NONE) );\n\
			bvec4 doLineBlend = greaterThanEqual( blendResult.zyxw, ivec4(BLEND_DOMINANT) );\n\
			vec3 blendPix[4];\n\
			\n\
			haveShallowLine[0] = haveShallowLine[0] && (v[0] != v[4]) && (v[5] != v[4]);\n\
			haveSteepLine[0]   = haveSteepLine[0] && (v[0] != v[8]) && (v[7] != v[8]);\n\
			doLineBlend[0] = (  doLineBlend[0] ||\n\
							   !((blendResult[1] != BLEND_NONE && !IsPixEqual(src[0], src[4])) ||\n\
								 (blendResult[3] != BLEND_NONE && !IsPixEqual(src[0], src[8])) ||\n\
								 (IsPixEqual(src[4], src[3]) && IsPixEqual(src[3], src[2]) && IsPixEqual(src[2], src[1]) && IsPixEqual(src[1], src[8]) && !IsPixEqual(src[0], src[2])) ) );\n\
			blendPix[0] = ( DistYCbCr(src[0], src[1]) <= DistYCbCr(src[0], src[3]) ) ? src[1] : src[3];\n\
			\n\
			haveShallowLine[1] = haveShallowLine[1] && (v[0] != v[2]) && (v[3] != v[2]);\n\
			haveSteepLine[1]   = haveSteepLine[1] && (v[0] != v[6]) && (v[5] != v[6]);\n\
			doLineBlend[1] = (  doLineBlend[1] ||\n\
						  !((blendResult[0] != BLEND_NONE && !IsPixEqual(src[0], src[2])) ||\n\
							(blendResult[2] != BLEND_NONE && !IsPixEqual(src[0], src[6])) ||\n\
							(IsPixEqual(src[2], src[1]) && IsPixEqual(src[1], src[8]) && IsPixEqual(src[8], src[7]) && IsPixEqual(src[7], src[6]) && !IsPixEqual(src[0], src[8])) ) );\n\
			blendPix[1] = ( DistYCbCr(src[0], src[7]) <= DistYCbCr(src[0], src[1]) ) ? src[7] : src[1];\n\
			\n\
			haveShallowLine[2] = haveShallowLine[2] && (v[0] != v[8]) && (v[1] != v[8]);\n\
			haveSteepLine[2]   = haveSteepLine[2] && (v[0] != v[4]) && (v[3] != v[4]);\n\
			doLineBlend[2] = (  doLineBlend[2] ||\n\
						  !((blendResult[3] != BLEND_NONE && !IsPixEqual(src[0], src[8])) ||\n\
							(blendResult[1] != BLEND_NONE && !IsPixEqual(src[0], src[4])) ||\n\
							(IsPixEqual(src[8], src[7]) && IsPixEqual(src[7], src[6]) && IsPixEqual(src[6], src[5]) && IsPixEqual(src[5], src[4]) && !IsPixEqual(src[0], src[6])) ) );\n\
			blendPix[2] = ( DistYCbCr(src[0], src[5]) <= DistYCbCr(src[0], src[7]) ) ? src[5] : src[7];\n\
			\n\
			haveShallowLine[3] = haveShallowLine[3] && (v[0] != v[6]) && (v[7] != v[6]);\n\
			haveSteepLine[3]   = haveSteepLine[3] && (v[0] != v[2]) && (v[1] != v[2]);\n\
			doLineBlend[3] = (  doLineBlend[3] ||\n\
						  !((blendResult[2] != BLEND_NONE && !IsPixEqual(src[0], src[6])) ||\n\
							(blendResult[0] != BLEND_NONE && !IsPixEqual(src[0], src[2])) ||\n\
							(IsPixEqual(src[6], src[5]) && IsPixEqual(src[5], src[4]) && IsPixEqual(src[4], src[3]) && IsPixEqual(src[3], src[2]) && !IsPixEqual(src[0], src[4])) ) );\n\
			blendPix[3] = ( DistYCbCr(src[0], src[3]) <= DistYCbCr(src[0], src[5]) ) ? src[3] : src[5];\n\
			\n\
			int i = int( dot(floor(fract(texCoord[0]) * 4.05), vec2(1.05, 4.05)) );\n\
			\n\
			if (i == 0)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.25 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? 1.00 : 0.6848532563) : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.25 : 0.00);\n\
			}\n\
			else if (i == 1)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.75 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? ((haveShallowLine[2]) ? 1.00 : ((haveSteepLine[2]) ? 0.75 : 0.50)) : 0.08677704501) : 0.00);\n\
			}\n\
			else if (i == 2)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? ((haveSteepLine[1]) ? 1.00 : ((haveShallowLine[1]) ? 0.75 : 0.50)) : 0.08677704501) : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.75 : 0.00);\n\
			}\n\
			else if (i == 3)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.25 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? 1.00 : 0.6848532563) : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.25 : 0.00);\n\
			}\n\
			else if (i == 4)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? ((haveSteepLine[2]) ? 1.00 : ((haveShallowLine[2]) ? 0.75 : 0.50)) : 0.08677704501) : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.75 : 0.00);\n\
			}\n\
			else if (i == 5)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((haveShallowLine[2]) ? ((haveSteepLine[2]) ? 1.0/3.0 : 0.25) : ((haveSteepLine[2]) ? 0.25 : 0.00)) : 0.00);\n\
			}\n\
			else if (i == 6)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((haveShallowLine[1]) ? ((haveSteepLine[1]) ? 1.0/3.0 : 0.25) : ((haveSteepLine[1]) ? 0.25 : 0.00)) : 0.00);\n\
			}\n\
			else if (i == 7)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.75 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? ((haveShallowLine[1]) ? 1.00 : ((haveSteepLine[1]) ? 0.75 : 0.50)) : 0.08677704501) : 0.00);\n\
			}\n\
			else if (i == 8)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.75 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? ((haveShallowLine[3]) ? 1.00 : ((haveSteepLine[3]) ? 0.75 : 0.50)) : 0.08677704501) : 0.00);\n\
			}\n\
			else if (i == 9)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((haveShallowLine[3]) ? ((haveSteepLine[3]) ? 1.0/3.0 : 0.25) : ((haveSteepLine[3]) ? 0.25 : 0.00)) : 0.00);\n\
			}\n\
			else if (i == 10)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((haveShallowLine[0]) ? ((haveSteepLine[0]) ? 1.0/3.0 : 0.25) : ((haveSteepLine[0]) ? 0.25 : 0.00)) : 0.00);\n\
			}\n\
			else if (i == 11)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? ((haveSteepLine[0]) ? 1.00 : ((haveShallowLine[0]) ? 0.75 : 0.50)) : 0.08677704501) : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.75 : 0.00);\n\
			}\n\
			else if (i == 12)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.25 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.25 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? 1.00 : 0.6848532563) : 0.00);\n\
			}\n\
			else if (i == 13)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.75 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? ((haveSteepLine[3]) ? 1.00 : ((haveShallowLine[3]) ? 0.75 : 0.50)) : 0.08677704501) : 0.00);\n\
			}\n\
			else if (i == 14)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? ((haveShallowLine[0]) ? 1.00 : ((haveSteepLine[0]) ? 0.75 : 0.50)) : 0.08677704501) : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.75 : 0.00);\n\
			}\n\
			else if (i == 15)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? 1.00 : 0.6848532563) : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.25 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.25 : 0.00);\n\
			}\n\
		}\n\
		\n\
		OUT_FRAG_COLOR = vec4(newFragColor, 1.0);\n\
	}\n\
"};

static const char *Scaler5xBRZFragShader_110 = {"\
	#define BLEND_NONE 0\n\
	#define BLEND_NORMAL 1\n\
	#define BLEND_DOMINANT 2\n\
	#define LUMINANCE_WEIGHT 1.0\n\
	#define EQUAL_COLOR_TOLERANCE 30.0/255.0\n\
	#define STEEP_DIRECTION_THRESHOLD 2.2\n\
	#define DOMINANT_DIRECTION_THRESHOLD 3.6\n\
	#define M_PI 3.1415926535897932384626433832795\n\
	\n\
	// Let's not even bother trying to support GPUs below Mid-tier.\n\
	// The xBRZ pixel scalers are already pretty hefty as-is, and\n\
	// this shader, having to calculate 25 pixel locations, is the\n\
	// heftiest of all of them. Trust me -- older GPUs just can't\n\
	// handle this one.\n\
	\n\
	VARYING vec2 texCoord[25];\n\
	uniform sampler2DRect tex;\n\
	\n\
	float reduce(const vec3 color)\n\
	{\n\
		return dot(color, vec3(65536.0, 256.0, 1.0));\n\
	}\n\
	\n\
	float DistYCbCr(const vec3 pixA, const vec3 pixB)\n\
	{\n\
		const vec3 w = vec3(0.2627, 0.6780, 0.0593);\n\
		const float scaleB = 0.5 / (1.0 - w.b);\n\
		const float scaleR = 0.5 / (1.0 - w.r);\n\
		vec3 diff = pixA - pixB;\n\
		float Y = dot(diff, w);\n\
		float Cb = scaleB * (diff.b - Y);\n\
		float Cr = scaleR * (diff.r - Y);\n\
		\n\
		return sqrt( ((LUMINANCE_WEIGHT*Y) * (LUMINANCE_WEIGHT*Y)) + (Cb * Cb) + (Cr * Cr) );\n\
	}\n\
	\n\
	bool IsPixEqual(const vec3 pixA, const vec3 pixB)\n\
	{\n\
		return (DistYCbCr(pixA, pixB) < EQUAL_COLOR_TOLERANCE);\n\
	}\n\
	\n\
	bool IsBlendingNeeded(const ivec4 blend)\n\
	{\n\
		return any(notEqual(blend, ivec4(BLEND_NONE)));\n\
	}\n\
	\n\
	/*\n\
	// Let's keep xBRZ's original blending logic around for reference.\n\
	void ScalePixel(const ivec4 blend, const vec3 k[9], inout vec3 dst[25])\n\
	{\n\
		if (blend[2] == BLEND_NONE)\n\
		{\n\
			return;\n\
		}\n\
		\n\
		vec3 blendPix = ( DistYCbCr(k[0], k[1]) <= DistYCbCr(k[0], k[3]) ) ? k[1] : k[3];\n\
		\n\
		if ( DoLineBlend(blend, k) )\n\
		{\n\
			float v0 = reduce(k[0]);\n\
			float v4 = reduce(k[4]);\n\
			float v5 = reduce(k[5]);\n\
			float v7 = reduce(k[7]);\n\
			float v8 = reduce(k[8]);\n\
			\n\
			float dist_01_04 = DistYCbCr(k[1], k[4]);\n\
			float dist_03_08 = DistYCbCr(k[3], k[8]);\n\
			bool haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_01_04 <= dist_03_08) && (v0 != v4) && (v5 != v4);\n\
			bool haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_03_08 <= dist_01_04) && (v0 != v8) && (v7 != v8);\n\
			\n\
			if (haveShallowLine)\n\
			{\n\
				if (haveSteepLine)\n\
				{\n\
					// Blend line steep and shallow\n\
					dst[24] = mix(dst[24], blendPix, 0.25);\n\
					dst[ 1] = mix(dst[ 1], blendPix, 0.25);\n\
					dst[ 9] = mix(dst[ 9], blendPix, 0.75);\n\
					dst[16] = mix(dst[16], blendPix, 0.25);\n\
					dst[ 3] = mix(dst[ 3], blendPix, 0.25);\n\
					dst[15] = mix(dst[15], blendPix, 0.75);\n\
					dst[ 2] = mix(dst[ 2], blendPix, 2.0/3.0);\n\
					dst[10] = mix(dst[10], blendPix, 1.00);\n\
					dst[11] = mix(dst[11], blendPix, 1.00);\n\
					dst[12] = mix(dst[12], blendPix, 1.00);\n\
					dst[14] = mix(dst[14], blendPix, 1.00);\n\
					dst[13] = mix(dst[13], blendPix, 1.00);\n\
				}\n\
				else\n\
				{\n\
					// Blend line shallow\n\
					dst[16] = mix(dst[16], blendPix, 0.25);\n\
					dst[ 3] = mix(dst[ 3], blendPix, 0.25);\n\
					dst[10] = mix(dst[10], blendPix, 0.25);\n\
					dst[15] = mix(dst[15], blendPix, 0.75);\n\
					dst[ 2] = mix(dst[ 2], blendPix, 0.75);\n\
					dst[14] = mix(dst[14], blendPix, 1.00);\n\
					dst[13] = mix(dst[13], blendPix, 1.00);\n\
					dst[12] = mix(dst[12], blendPix, 1.00);\n\
					dst[11] = mix(dst[11], blendPix, 1.00);\n\
				}\n\
			}\n\
			else\n\
			{\n\
				if (haveSteepLine)\n\
				{\n\
					// Blend line steep\n\
					dst[24] = mix(dst[24], blendPix, 0.25);\n\
					dst[ 1] = mix(dst[ 1], blendPix, 0.25);\n\
					dst[14] = mix(dst[14], blendPix, 0.25);\n\
					dst[ 9] = mix(dst[ 9], blendPix, 0.75);\n\
					dst[ 2] = mix(dst[ 2], blendPix, 0.75);\n\
					dst[10] = mix(dst[10], blendPix, 1.00);\n\
					dst[11] = mix(dst[11], blendPix, 1.00);\n\
					dst[12] = mix(dst[12], blendPix, 1.00);\n\
					dst[13] = mix(dst[13], blendPix, 1.00);\n\
				}\n\
				else\n\
				{\n\
					// Blend line diagonal\n\
					dst[14] = mix(dst[14], blendPix, 0.125);\n\
					dst[ 2] = mix(dst[ 2], blendPix, 0.125);\n\
					dst[10] = mix(dst[10], blendPix, 0.125);\n\
					dst[13] = mix(dst[13], blendPix, 0.875);\n\
					dst[11] = mix(dst[11], blendPix, 0.875);\n\
					dst[12] = mix(dst[12], blendPix, 1.000);\n\
				}\n\
			}\n\
		}\n\
		else\n\
		{\n\
			// Blend corner\n\
			dst[12] = mix(dst[12], blendPix, 0.8631434088);\n\
			dst[11] = mix(dst[11], blendPix, 0.2306749731);\n\
			dst[13] = mix(dst[13], blendPix, 0.2306749731);\n\
		}\n\
	}\n\
	*/\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  --|21|22|23|--\n\
	//                       19|06|07|08|09\n\
	//                       18|05|00|01|10\n\
	//                       17|04|03|02|11\n\
	//                       --|15|14|13|--\n\
	//\n\
	// Output Pixel Mapping: 00|01|02|03|04\n\
	//                       05|06|07|08|09\n\
	//                       10|11|12|13|14\n\
	//                       15|16|17|18|19\n\
	//                       20|21|22|23|24\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[25];\n\
		src[ 0] = SAMPLE3_TEX_RECT(tex, texCoord[ 0]).rgb;\n\
		src[ 1] = SAMPLE3_TEX_RECT(tex, texCoord[ 1]).rgb;\n\
		src[ 2] = SAMPLE3_TEX_RECT(tex, texCoord[ 2]).rgb;\n\
		src[ 3] = SAMPLE3_TEX_RECT(tex, texCoord[ 3]).rgb;\n\
		src[ 4] = SAMPLE3_TEX_RECT(tex, texCoord[ 4]).rgb;\n\
		src[ 5] = SAMPLE3_TEX_RECT(tex, texCoord[ 5]).rgb;\n\
		src[ 6] = SAMPLE3_TEX_RECT(tex, texCoord[ 6]).rgb;\n\
		src[ 7] = SAMPLE3_TEX_RECT(tex, texCoord[ 7]).rgb;\n\
		src[ 8] = SAMPLE3_TEX_RECT(tex, texCoord[ 8]).rgb;\n\
		src[ 9] = SAMPLE3_TEX_RECT(tex, texCoord[ 9]).rgb;\n\
		src[10] = SAMPLE3_TEX_RECT(tex, texCoord[10]).rgb;\n\
		src[11] = SAMPLE3_TEX_RECT(tex, texCoord[11]).rgb;\n\
		src[12] = SAMPLE3_TEX_RECT(tex, texCoord[12]).rgb;\n\
		src[13] = SAMPLE3_TEX_RECT(tex, texCoord[13]).rgb;\n\
		src[14] = SAMPLE3_TEX_RECT(tex, texCoord[14]).rgb;\n\
		src[15] = SAMPLE3_TEX_RECT(tex, texCoord[15]).rgb;\n\
		src[16] = SAMPLE3_TEX_RECT(tex, texCoord[16]).rgb;\n\
		src[17] = SAMPLE3_TEX_RECT(tex, texCoord[17]).rgb;\n\
		src[18] = SAMPLE3_TEX_RECT(tex, texCoord[18]).rgb;\n\
		src[19] = SAMPLE3_TEX_RECT(tex, texCoord[19]).rgb;\n\
		src[20] = SAMPLE3_TEX_RECT(tex, texCoord[20]).rgb;\n\
		src[21] = SAMPLE3_TEX_RECT(tex, texCoord[21]).rgb;\n\
		src[22] = SAMPLE3_TEX_RECT(tex, texCoord[22]).rgb;\n\
		src[23] = SAMPLE3_TEX_RECT(tex, texCoord[23]).rgb;\n\
		src[24] = SAMPLE3_TEX_RECT(tex, texCoord[24]).rgb;\n\
		\n\
		vec3 newFragColor = src[0];\n\
		int i = int( dot(floor(fract(texCoord[0]) * 5.05), vec2(1.05, 5.05)) );\n\
		\n\
		if (i != 12)\n\
		{\n\
			float v[9];\n\
			v[0] = reduce(src[0]);\n\
			v[1] = reduce(src[1]);\n\
			v[2] = reduce(src[2]);\n\
			v[3] = reduce(src[3]);\n\
			v[4] = reduce(src[4]);\n\
			v[5] = reduce(src[5]);\n\
			v[6] = reduce(src[6]);\n\
			v[7] = reduce(src[7]);\n\
			v[8] = reduce(src[8]);\n\
			\n\
			ivec4 blendResult = ivec4(BLEND_NONE);\n\
			\n\
			// Preprocess corners\n\
			// Pixel Tap Mapping: --|--|--|--|--\n\
			//                    --|--|07|08|--\n\
			//                    --|05|00|01|10\n\
			//                    --|04|03|02|11\n\
			//                    --|--|14|13|--\n\
			\n\
			// Corner (1, 1)\n\
			if ( !((v[0] == v[1] && v[3] == v[2]) || (v[0] == v[3] && v[1] == v[2])) )\n\
			{\n\
				float dist_03_01 = DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + DistYCbCr(src[14], src[ 2]) + DistYCbCr(src[ 2], src[10]) + (4.0 * DistYCbCr(src[ 3], src[ 1]));\n\
				float dist_00_02 = DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src[ 3], src[13]) + DistYCbCr(src[ 7], src[ 1]) + DistYCbCr(src[ 1], src[11]) + (4.0 * DistYCbCr(src[ 0], src[ 2]));\n\
				bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_03_01) < dist_00_02;\n\
				blendResult[2] = ((dist_03_01 < dist_00_02) && (v[0] != v[1]) && (v[0] != v[3])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
			}\n\
			\n\
			\n\
			// Pixel Tap Mapping: --|--|--|--|--\n\
			//                    --|06|07|--|--\n\
			//                    18|05|00|01|--\n\
			//                    17|04|03|02|--\n\
			//                    --|15|14|--|--\n\
			// Corner (0, 1)\n\
			if ( !((v[5] == v[0] && v[4] == v[3]) || (v[5] == v[4] && v[0] == v[3])) )\n\
			{\n\
				float dist_04_00 = DistYCbCr(src[17], src[ 5]) + DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[15], src[ 3]) + DistYCbCr(src[ 3], src[ 1]) + (4.0 * DistYCbCr(src[ 4], src[ 0]));\n\
				float dist_05_03 = DistYCbCr(src[18], src[ 4]) + DistYCbCr(src[ 4], src[14]) + DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + (4.0 * DistYCbCr(src[ 5], src[ 3]));\n\
				bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_05_03) < dist_04_00;\n\
				blendResult[3] = ((dist_04_00 > dist_05_03) && (v[0] != v[5]) && (v[0] != v[3])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
			}\n\
			\n\
			// Pixel Tap Mapping: --|--|22|23|--\n\
			//                    --|06|07|08|09\n\
			//                    --|05|00|01|10\n\
			//                    --|--|03|02|--\n\
			//                    --|--|--|--|--\n\
			// Corner (1, 0)\n\
			if ( !((v[7] == v[8] && v[0] == v[1]) || (v[7] == v[0] && v[8] == v[1])) )\n\
			{\n\
				float dist_00_08 = DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[ 7], src[23]) + DistYCbCr(src[ 3], src[ 1]) + DistYCbCr(src[ 1], src[ 9]) + (4.0 * DistYCbCr(src[ 0], src[ 8]));\n\
				float dist_07_01 = DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + DistYCbCr(src[22], src[ 8]) + DistYCbCr(src[ 8], src[10]) + (4.0 * DistYCbCr(src[ 7], src[ 1]));\n\
				bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_07_01) < dist_00_08;\n\
				blendResult[1] = ((dist_00_08 > dist_07_01) && (v[0] != v[7]) && (v[0] != v[1])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
			}\n\
			\n\
			// Pixel Tap Mapping: --|21|22|--|--\n\
			//                    19|06|07|08|--\n\
			//                    18|05|00|01|--\n\
			//                    --|04|03|--|--\n\
			//                    --|--|--|--|--\n\
			// Corner (0, 0)\n\
			if ( !((v[6] == v[7] && v[5] == v[0]) || (v[6] == v[5] && v[7] == v[0])) )\n\
			{\n\
				float dist_05_07 = DistYCbCr(src[18], src[ 6]) + DistYCbCr(src[ 6], src[22]) + DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + (4.0 * DistYCbCr(src[ 5], src[ 7]));\n\
				float dist_06_00 = DistYCbCr(src[19], src[ 5]) + DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src[21], src[ 7]) + DistYCbCr(src[ 7], src[ 1]) + (4.0 * DistYCbCr(src[ 6], src[ 0]));\n\
				bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_05_07) < dist_06_00;\n\
				blendResult[0] = ((dist_05_07 < dist_06_00) && (v[0] != v[5]) && (v[0] != v[7])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
			}\n\
			\n\
			// Scale pixel\n\
			if (IsBlendingNeeded(blendResult))\n\
			{\n\
				vec4 dist_01_04 = vec4( DistYCbCr(src[1], src[4]), DistYCbCr(src[7], src[2]), DistYCbCr(src[5], src[8]), DistYCbCr(src[3], src[6]) );\n\
				vec4 dist_03_08 = vec4( DistYCbCr(src[3], src[8]), DistYCbCr(src[1], src[6]), DistYCbCr(src[7], src[4]), DistYCbCr(src[5], src[2]) );\n\
				bvec4 haveShallowLine = lessThanEqual(STEEP_DIRECTION_THRESHOLD * dist_01_04, dist_03_08);\n\
				bvec4 haveSteepLine = lessThanEqual(STEEP_DIRECTION_THRESHOLD * dist_03_08, dist_01_04);\n\
				bvec4 needBlend = notEqual( blendResult.zyxw, ivec4(BLEND_NONE) );\n\
				bvec4 doLineBlend = greaterThanEqual( blendResult.zyxw, ivec4(BLEND_DOMINANT) );\n\
				vec3 blendPix[4];\n\
				\n\
				haveShallowLine[0] = haveShallowLine[0] && (v[0] != v[4]) && (v[5] != v[4]);\n\
				haveSteepLine[0]   = haveSteepLine[0] && (v[0] != v[8]) && (v[7] != v[8]);\n\
				doLineBlend[0] = (  doLineBlend[0] ||\n\
								   !((blendResult[1] != BLEND_NONE && !IsPixEqual(src[0], src[4])) ||\n\
									 (blendResult[3] != BLEND_NONE && !IsPixEqual(src[0], src[8])) ||\n\
									 (IsPixEqual(src[4], src[3]) && IsPixEqual(src[3], src[2]) && IsPixEqual(src[2], src[1]) && IsPixEqual(src[1], src[8]) && !IsPixEqual(src[0], src[2])) ) );\n\
				blendPix[0] = ( DistYCbCr(src[0], src[1]) <= DistYCbCr(src[0], src[3]) ) ? src[1] : src[3];\n\
				\n\
				haveShallowLine[1] = haveShallowLine[1] && (v[0] != v[2]) && (v[3] != v[2]);\n\
				haveSteepLine[1]   = haveSteepLine[1] && (v[0] != v[6]) && (v[5] != v[6]);\n\
				doLineBlend[1] = (  doLineBlend[1] ||\n\
							  !((blendResult[0] != BLEND_NONE && !IsPixEqual(src[0], src[2])) ||\n\
								(blendResult[2] != BLEND_NONE && !IsPixEqual(src[0], src[6])) ||\n\
								(IsPixEqual(src[2], src[1]) && IsPixEqual(src[1], src[8]) && IsPixEqual(src[8], src[7]) && IsPixEqual(src[7], src[6]) && !IsPixEqual(src[0], src[8])) ) );\n\
				blendPix[1] = ( DistYCbCr(src[0], src[7]) <= DistYCbCr(src[0], src[1]) ) ? src[7] : src[1];\n\
				\n\
				haveShallowLine[2] = haveShallowLine[2] && (v[0] != v[8]) && (v[1] != v[8]);\n\
				haveSteepLine[2]   = haveSteepLine[2] && (v[0] != v[4]) && (v[3] != v[4]);\n\
				doLineBlend[2] = (  doLineBlend[2] ||\n\
							  !((blendResult[3] != BLEND_NONE && !IsPixEqual(src[0], src[8])) ||\n\
								(blendResult[1] != BLEND_NONE && !IsPixEqual(src[0], src[4])) ||\n\
								(IsPixEqual(src[8], src[7]) && IsPixEqual(src[7], src[6]) && IsPixEqual(src[6], src[5]) && IsPixEqual(src[5], src[4]) && !IsPixEqual(src[0], src[6])) ) );\n\
				blendPix[2] = ( DistYCbCr(src[0], src[5]) <= DistYCbCr(src[0], src[7]) ) ? src[5] : src[7];\n\
				\n\
				haveShallowLine[3] = haveShallowLine[3] && (v[0] != v[6]) && (v[7] != v[6]);\n\
				haveSteepLine[3]   = haveSteepLine[3] && (v[0] != v[2]) && (v[1] != v[2]);\n\
				doLineBlend[3] = (  doLineBlend[3] ||\n\
							  !((blendResult[2] != BLEND_NONE && !IsPixEqual(src[0], src[6])) ||\n\
								(blendResult[0] != BLEND_NONE && !IsPixEqual(src[0], src[2])) ||\n\
								(IsPixEqual(src[6], src[5]) && IsPixEqual(src[5], src[4]) && IsPixEqual(src[4], src[3]) && IsPixEqual(src[3], src[2]) && !IsPixEqual(src[0], src[4])) ) );\n\
				blendPix[3] = ( DistYCbCr(src[0], src[3]) <= DistYCbCr(src[0], src[5]) ) ? src[3] : src[5];\n\
				\n\
				if (i == 0)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? 1.000 : 0.8631434088) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 1)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.750 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? ((!haveShallowLine[2] && !haveSteepLine[2]) ? 0.875 : 1.000) : 0.2306749731) : 0.000);\n\
				}\n\
				else if (i == 2)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((haveSteepLine[1]) ? 1.000 : ((haveShallowLine[1]) ? 0.250 : 0.125)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((haveShallowLine[2]) ? 1.000 : ((haveSteepLine[2]) ? 0.250 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 3)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? ((!haveShallowLine[1] && !haveSteepLine[1]) ? 0.875 : 1.000) : 0.2306749731) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.750 : 0.000);\n\
				}\n\
				else if (i == 4)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? 1.000 : 0.8631434088) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 5)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? ((!haveShallowLine[2] && !haveSteepLine[2]) ? 0.875 : 1.000) : 0.2306749731) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.750 : 0.000);\n\
				}\n\
				else if (i == 6)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((haveShallowLine[2]) ? ((haveSteepLine[2]) ? 2.0/3.0 : 0.750) : ((haveSteepLine[2]) ? 0.750 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 7)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 8)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((haveShallowLine[1]) ? ((haveSteepLine[1]) ? 2.0/3.0 : 0.750) : ((haveSteepLine[1]) ? 0.750 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 9)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.750 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? ((!haveShallowLine[1] && !haveSteepLine[1]) ? 0.875 : 1.000) : 0.2306749731) : 0.000);\n\
				}\n\
				else if (i == 10)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((haveSteepLine[2]) ? 1.000 : ((haveShallowLine[2]) ? 0.250 : 0.125)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((haveShallowLine[3]) ? 1.000 : ((haveSteepLine[3]) ? 0.250 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 11)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 13)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 14)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((haveSteepLine[0]) ? 1.000 : ((haveShallowLine[0]) ? 0.250 : 0.125)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((haveShallowLine[1]) ? 1.000 : ((haveSteepLine[1]) ? 0.250 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 15)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.750 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? ((!haveShallowLine[3] && !haveSteepLine[3]) ? 0.875 : 1.000) : 0.2306749731) : 0.000);\n\
				}\n\
				else if (i == 16)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((haveShallowLine[3]) ? ((haveSteepLine[3]) ? 2.0/3.0 : 0.750) : ((haveSteepLine[3]) ? 0.750 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 17)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 18)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((haveShallowLine[0]) ? ((haveSteepLine[0]) ? 2.0/3.0 : 0.750) : ((haveSteepLine[0]) ? 0.750 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 19)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? ((!haveShallowLine[0] && !haveSteepLine[0]) ? 0.875 : 1.000) : 0.2306749731) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.750 : 0.000);\n\
				}\n\
				else if (i == 20)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? 1.000 : 0.8631434088) : 0.000);\n\
				}\n\
				else if (i == 21)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.750 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? ((!haveShallowLine[3] && !haveSteepLine[3]) ? 0.875 : 1.000) : 0.2306749731) : 0.000);\n\
				}\n\
				else if (i == 22)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((haveShallowLine[0]) ? 1.000 : ((haveSteepLine[0]) ? 0.250 : 0.125)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((haveSteepLine[3]) ? 1.000 : ((haveShallowLine[3]) ? 0.250 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 23)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? ((!haveShallowLine[0] && !haveSteepLine[0]) ? 0.875 : 1.000) : 0.2306749731) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.750 : 0.000);\n\
				}\n\
				else if (i == 24)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? 1.000 : 0.8631434088) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.250 : 0.000);\n\
				}\n\
			}\n\
		}\n\
		\n\
		OUT_FRAG_COLOR = vec4(newFragColor, 1.0);\n\
	}\n\
"};

static const char *Scaler6xBRZFragShader_110 = {"\
	#define BLEND_NONE 0\n\
	#define BLEND_NORMAL 1\n\
	#define BLEND_DOMINANT 2\n\
	#define LUMINANCE_WEIGHT 1.0\n\
	#define EQUAL_COLOR_TOLERANCE 30.0/255.0\n\
	#define STEEP_DIRECTION_THRESHOLD 2.2\n\
	#define DOMINANT_DIRECTION_THRESHOLD 3.6\n\
	#define M_PI 3.1415926535897932384626433832795\n\
	\n\
	// Let's not even bother trying to support GPUs below Mid-tier.\n\
	// The xBRZ pixel scalers are already pretty hefty as-is, and\n\
	// this shader, having to calculate 36 pixel locations, is the\n\
	// heftiest of all of them. Trust me -- older GPUs just can't\n\
	// handle this one.\n\
	\n\
	VARYING vec2 texCoord[25];\n\
	uniform sampler2DRect tex;\n\
	\n\
	float reduce(const vec3 color)\n\
	{\n\
		return dot(color, vec3(65536.0, 256.0, 1.0));\n\
	}\n\
	\n\
	float DistYCbCr(const vec3 pixA, const vec3 pixB)\n\
	{\n\
		const vec3 w = vec3(0.2627, 0.6780, 0.0593);\n\
		const float scaleB = 0.5 / (1.0 - w.b);\n\
		const float scaleR = 0.5 / (1.0 - w.r);\n\
		vec3 diff = pixA - pixB;\n\
		float Y = dot(diff, w);\n\
		float Cb = scaleB * (diff.b - Y);\n\
		float Cr = scaleR * (diff.r - Y);\n\
		\n\
		return sqrt( ((LUMINANCE_WEIGHT*Y) * (LUMINANCE_WEIGHT*Y)) + (Cb * Cb) + (Cr * Cr) );\n\
	}\n\
	\n\
	bool IsPixEqual(const vec3 pixA, const vec3 pixB)\n\
	{\n\
		return (DistYCbCr(pixA, pixB) < EQUAL_COLOR_TOLERANCE);\n\
	}\n\
	\n\
	bool IsBlendingNeeded(const ivec4 blend)\n\
	{\n\
		return any(notEqual(blend, ivec4(BLEND_NONE)));\n\
	}\n\
	\n\
	/*\n\
	// Let's keep xBRZ's original blending logic around for reference.\n\
	void ScalePixel(const ivec4 blend, const vec3 k[9], inout vec3 dst[25])\n\
	{\n\
		if (blend[2] == BLEND_NONE)\n\
		{\n\
			return;\n\
		}\n\
		\n\
		vec3 blendPix = ( DistYCbCr(k[0], k[1]) <= DistYCbCr(k[0], k[3]) ) ? k[1] : k[3];\n\
		\n\
		if ( DoLineBlend(blend, k) )\n\
		{\n\
			float v0 = reduce(k[0]);\n\
			float v4 = reduce(k[4]);\n\
			float v5 = reduce(k[5]);\n\
			float v7 = reduce(k[7]);\n\
			float v8 = reduce(k[8]);\n\
			\n\
			float dist_01_04 = DistYCbCr(k[1], k[4]);\n\
			float dist_03_08 = DistYCbCr(k[3], k[8]);\n\
			bool haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_01_04 <= dist_03_08) && (v0 != v4) && (v5 != v4);\n\
			bool haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_03_08 <= dist_01_04) && (v0 != v8) && (v7 != v8);\n\
			\n\
			if (haveShallowLine)\n\
			{\n\
				if (haveSteepLine)\n\
				{\n\
					// Blend line steep and shallow\n\
					dst[25] = mix(dst[25], blendPix, 0.25);\n\
					dst[10] = mix(dst[10], blendPix, 0.25);\n\
					dst[26] = mix(dst[26], blendPix, 0.75);\n\
					dst[11] = mix(dst[11], blendPix, 0.75);\n\
					dst[35] = mix(dst[35], blendPix, 0.25);\n\
					dst[14] = mix(dst[14], blendPix, 0.25);\n\
					dst[34] = mix(dst[34], blendPix, 0.75);\n\
					dst[13] = mix(dst[13], blendPix, 0.75);\n\
					dst[27] = mix(dst[27], blendPix, 1.00);\n\
					dst[28] = mix(dst[28], blendPix, 1.00);\n\
					dst[29] = mix(dst[29], blendPix, 1.00);\n\
					dst[30] = mix(dst[30], blendPix, 1.00);\n\
					dst[12] = mix(dst[12], blendPix, 1.00);\n\
					dst[31] = mix(dst[31], blendPix, 1.00);\n\
					dst[33] = mix(dst[33], blendPix, 1.00);\n\
					dst[32] = mix(dst[32], blendPix, 1.00);\n\
				}\n\
				else\n\
				{\n\
					// Blend line shallow\n\
					dst[35] = mix(dst[35], blendPix, 0.25);\n\
					dst[14] = mix(dst[14], blendPix, 0.25);\n\
					dst[11] = mix(dst[11], blendPix, 0.25);\n\
					dst[34] = mix(dst[34], blendPix, 0.75);\n\
					dst[13] = mix(dst[13], blendPix, 0.75);\n\
					dst[28] = mix(dst[28], blendPix, 0.75);\n\
					dst[33] = mix(dst[33], blendPix, 1.00);\n\
					dst[32] = mix(dst[32], blendPix, 1.00);\n\
					dst[31] = mix(dst[31], blendPix, 1.00);\n\
					dst[30] = mix(dst[30], blendPix, 1.00);\n\
					dst[12] = mix(dst[12], blendPix, 1.00);\n\
					dst[29] = mix(dst[29], blendPix, 1.00);\n\
				}\n\
			}\n\
			else\n\
			{\n\
				if (haveSteepLine)\n\
				{\n\
					// Blend line steep\n\
					dst[25] = mix(dst[25], blendPix, 0.25);\n\
					dst[10] = mix(dst[10], blendPix, 0.25);\n\
					dst[13] = mix(dst[13], blendPix, 0.25);\n\
					dst[26] = mix(dst[26], blendPix, 0.75);\n\
					dst[11] = mix(dst[11], blendPix, 0.75);\n\
					dst[32] = mix(dst[32], blendPix, 0.75);\n\
					dst[27] = mix(dst[27], blendPix, 1.00);\n\
					dst[28] = mix(dst[28], blendPix, 1.00);\n\
					dst[29] = mix(dst[29], blendPix, 1.00);\n\
					dst[30] = mix(dst[30], blendPix, 1.00);\n\
					dst[12] = mix(dst[12], blendPix, 1.00);\n\
					dst[31] = mix(dst[31], blendPix, 1.00);\n\
				}\n\
				else\n\
				{\n\
					// Blend line diagonal\n\
					dst[32] = mix(dst[32], blendPix, 0.50);\n\
					dst[12] = mix(dst[12], blendPix, 0.50);\n\
					dst[28] = mix(dst[28], blendPix, 0.50);\n\
					dst[29] = mix(dst[29], blendPix, 1.00);\n\
					dst[30] = mix(dst[30], blendPix, 1.00);\n\
					dst[31] = mix(dst[31], blendPix, 1.00);\n\
				}\n\
			}\n\
		}\n\
		else\n\
		{\n\
			// Blend corner\n\
			dst[30] = mix(dst[30], blendPix, 0.9711013910);\n\
			dst[29] = mix(dst[29], blendPix, 0.4236372243);\n\
			dst[31] = mix(dst[31], blendPix, 0.4236372243);\n\
			dst[32] = mix(dst[32], blendPix, 0.05652034508);\n\
			dst[28] = mix(dst[28], blendPix, 0.05652034508);\n\
		}\n\
	}\n\
	*/\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:    --|21|22|23|--\n\
	//                         19|06|07|08|09\n\
	//                         18|05|00|01|10\n\
	//                         17|04|03|02|11\n\
	//                         --|15|14|13|--\n\
	//\n\
	// Output Pixel Mapping: 00|01|02|03|04|05\n\
	//                       06|07|08|09|10|11\n\
	//                       12|13|14|15|16|17\n\
	//                       18|19|20|21|22|23\n\
	//                       24|25|26|27|28|29\n\
	//                       30|31|32|33|34|35\n\
	\n\
	void main()\n\
	{\n\
		vec3 src[25];\n\
		src[ 0] = SAMPLE3_TEX_RECT(tex, texCoord[ 0]).rgb;\n\
		src[ 1] = SAMPLE3_TEX_RECT(tex, texCoord[ 1]).rgb;\n\
		src[ 2] = SAMPLE3_TEX_RECT(tex, texCoord[ 2]).rgb;\n\
		src[ 3] = SAMPLE3_TEX_RECT(tex, texCoord[ 3]).rgb;\n\
		src[ 4] = SAMPLE3_TEX_RECT(tex, texCoord[ 4]).rgb;\n\
		src[ 5] = SAMPLE3_TEX_RECT(tex, texCoord[ 5]).rgb;\n\
		src[ 6] = SAMPLE3_TEX_RECT(tex, texCoord[ 6]).rgb;\n\
		src[ 7] = SAMPLE3_TEX_RECT(tex, texCoord[ 7]).rgb;\n\
		src[ 8] = SAMPLE3_TEX_RECT(tex, texCoord[ 8]).rgb;\n\
		src[ 9] = SAMPLE3_TEX_RECT(tex, texCoord[ 9]).rgb;\n\
		src[10] = SAMPLE3_TEX_RECT(tex, texCoord[10]).rgb;\n\
		src[11] = SAMPLE3_TEX_RECT(tex, texCoord[11]).rgb;\n\
		src[12] = SAMPLE3_TEX_RECT(tex, texCoord[12]).rgb;\n\
		src[13] = SAMPLE3_TEX_RECT(tex, texCoord[13]).rgb;\n\
		src[14] = SAMPLE3_TEX_RECT(tex, texCoord[14]).rgb;\n\
		src[15] = SAMPLE3_TEX_RECT(tex, texCoord[15]).rgb;\n\
		src[16] = SAMPLE3_TEX_RECT(tex, texCoord[16]).rgb;\n\
		src[17] = SAMPLE3_TEX_RECT(tex, texCoord[17]).rgb;\n\
		src[18] = SAMPLE3_TEX_RECT(tex, texCoord[18]).rgb;\n\
		src[19] = SAMPLE3_TEX_RECT(tex, texCoord[19]).rgb;\n\
		src[20] = SAMPLE3_TEX_RECT(tex, texCoord[20]).rgb;\n\
		src[21] = SAMPLE3_TEX_RECT(tex, texCoord[21]).rgb;\n\
		src[22] = SAMPLE3_TEX_RECT(tex, texCoord[22]).rgb;\n\
		src[23] = SAMPLE3_TEX_RECT(tex, texCoord[23]).rgb;\n\
		src[24] = SAMPLE3_TEX_RECT(tex, texCoord[24]).rgb;\n\
		\n\
		vec3 newFragColor = src[0];\n\
		int i = int( dot(floor(fract(texCoord[0]) * 6.05), vec2(1.05, 6.05)) );\n\
		\n\
		if ( (i != 14) && (i != 15) && (i != 20) && (i != 21) )\n\
		{\n\
			float v[9];\n\
			v[0] = reduce(src[0]);\n\
			v[1] = reduce(src[1]);\n\
			v[2] = reduce(src[2]);\n\
			v[3] = reduce(src[3]);\n\
			v[4] = reduce(src[4]);\n\
			v[5] = reduce(src[5]);\n\
			v[6] = reduce(src[6]);\n\
			v[7] = reduce(src[7]);\n\
			v[8] = reduce(src[8]);\n\
			\n\
			ivec4 blendResult = ivec4(BLEND_NONE);\n\
			\n\
			// Preprocess corners\n\
			// Pixel Tap Mapping: --|--|--|--|--\n\
			//                    --|--|07|08|--\n\
			//                    --|05|00|01|10\n\
			//                    --|04|03|02|11\n\
			//                    --|--|14|13|--\n\
			\n\
			// Corner (1, 1)\n\
			if ( !((v[0] == v[1] && v[3] == v[2]) || (v[0] == v[3] && v[1] == v[2])) )\n\
			{\n\
				float dist_03_01 = DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + DistYCbCr(src[14], src[ 2]) + DistYCbCr(src[ 2], src[10]) + (4.0 * DistYCbCr(src[ 3], src[ 1]));\n\
				float dist_00_02 = DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src[ 3], src[13]) + DistYCbCr(src[ 7], src[ 1]) + DistYCbCr(src[ 1], src[11]) + (4.0 * DistYCbCr(src[ 0], src[ 2]));\n\
				bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_03_01) < dist_00_02;\n\
				blendResult[2] = ((dist_03_01 < dist_00_02) && (v[0] != v[1]) && (v[0] != v[3])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
			}\n\
			\n\
			\n\
			// Pixel Tap Mapping: --|--|--|--|--\n\
			//                    --|06|07|--|--\n\
			//                    18|05|00|01|--\n\
			//                    17|04|03|02|--\n\
			//                    --|15|14|--|--\n\
			// Corner (0, 1)\n\
			if ( !((v[5] == v[0] && v[4] == v[3]) || (v[5] == v[4] && v[0] == v[3])) )\n\
			{\n\
				float dist_04_00 = DistYCbCr(src[17], src[ 5]) + DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[15], src[ 3]) + DistYCbCr(src[ 3], src[ 1]) + (4.0 * DistYCbCr(src[ 4], src[ 0]));\n\
				float dist_05_03 = DistYCbCr(src[18], src[ 4]) + DistYCbCr(src[ 4], src[14]) + DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + (4.0 * DistYCbCr(src[ 5], src[ 3]));\n\
				bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_05_03) < dist_04_00;\n\
				blendResult[3] = ((dist_04_00 > dist_05_03) && (v[0] != v[5]) && (v[0] != v[3])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
			}\n\
			\n\
			// Pixel Tap Mapping: --|--|22|23|--\n\
			//                    --|06|07|08|09\n\
			//                    --|05|00|01|10\n\
			//                    --|--|03|02|--\n\
			//                    --|--|--|--|--\n\
			// Corner (1, 0)\n\
			if ( !((v[7] == v[8] && v[0] == v[1]) || (v[7] == v[0] && v[8] == v[1])) )\n\
			{\n\
				float dist_00_08 = DistYCbCr(src[ 5], src[ 7]) + DistYCbCr(src[ 7], src[23]) + DistYCbCr(src[ 3], src[ 1]) + DistYCbCr(src[ 1], src[ 9]) + (4.0 * DistYCbCr(src[ 0], src[ 8]));\n\
				float dist_07_01 = DistYCbCr(src[ 6], src[ 0]) + DistYCbCr(src[ 0], src[ 2]) + DistYCbCr(src[22], src[ 8]) + DistYCbCr(src[ 8], src[10]) + (4.0 * DistYCbCr(src[ 7], src[ 1]));\n\
				bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_07_01) < dist_00_08;\n\
				blendResult[1] = ((dist_00_08 > dist_07_01) && (v[0] != v[7]) && (v[0] != v[1])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
			}\n\
			\n\
			// Pixel Tap Mapping: --|21|22|--|--\n\
			//                    19|06|07|08|--\n\
			//                    18|05|00|01|--\n\
			//                    --|04|03|--|--\n\
			//                    --|--|--|--|--\n\
			// Corner (0, 0)\n\
			if ( !((v[6] == v[7] && v[5] == v[0]) || (v[6] == v[5] && v[7] == v[0])) )\n\
			{\n\
				float dist_05_07 = DistYCbCr(src[18], src[ 6]) + DistYCbCr(src[ 6], src[22]) + DistYCbCr(src[ 4], src[ 0]) + DistYCbCr(src[ 0], src[ 8]) + (4.0 * DistYCbCr(src[ 5], src[ 7]));\n\
				float dist_06_00 = DistYCbCr(src[19], src[ 5]) + DistYCbCr(src[ 5], src[ 3]) + DistYCbCr(src[21], src[ 7]) + DistYCbCr(src[ 7], src[ 1]) + (4.0 * DistYCbCr(src[ 6], src[ 0]));\n\
				bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_05_07) < dist_06_00;\n\
				blendResult[0] = ((dist_05_07 < dist_06_00) && (v[0] != v[5]) && (v[0] != v[7])) ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;\n\
			}\n\
			\n\
			// Scale pixel\n\
			if (IsBlendingNeeded(blendResult))\n\
			{\n\
				vec4 dist_01_04 = vec4( DistYCbCr(src[1], src[4]), DistYCbCr(src[7], src[2]), DistYCbCr(src[5], src[8]), DistYCbCr(src[3], src[6]) );\n\
				vec4 dist_03_08 = vec4( DistYCbCr(src[3], src[8]), DistYCbCr(src[1], src[6]), DistYCbCr(src[7], src[4]), DistYCbCr(src[5], src[2]) );\n\
				bvec4 haveShallowLine = lessThanEqual(STEEP_DIRECTION_THRESHOLD * dist_01_04, dist_03_08);\n\
				bvec4 haveSteepLine = lessThanEqual(STEEP_DIRECTION_THRESHOLD * dist_03_08, dist_01_04);\n\
				bvec4 needBlend = notEqual( blendResult.zyxw, ivec4(BLEND_NONE) );\n\
				bvec4 doLineBlend = greaterThanEqual( blendResult.zyxw, ivec4(BLEND_DOMINANT) );\n\
				vec3 blendPix[4];\n\
				\n\
				haveShallowLine[0] = haveShallowLine[0] && (v[0] != v[4]) && (v[5] != v[4]);\n\
				haveSteepLine[0]   = haveSteepLine[0] && (v[0] != v[8]) && (v[7] != v[8]);\n\
				doLineBlend[0] = (  doLineBlend[0] ||\n\
								   !((blendResult[1] != BLEND_NONE && !IsPixEqual(src[0], src[4])) ||\n\
									 (blendResult[3] != BLEND_NONE && !IsPixEqual(src[0], src[8])) ||\n\
									 (IsPixEqual(src[4], src[3]) && IsPixEqual(src[3], src[2]) && IsPixEqual(src[2], src[1]) && IsPixEqual(src[1], src[8]) && !IsPixEqual(src[0], src[2])) ) );\n\
				blendPix[0] = ( DistYCbCr(src[0], src[1]) <= DistYCbCr(src[0], src[3]) ) ? src[1] : src[3];\n\
				\n\
				haveShallowLine[1] = haveShallowLine[1] && (v[0] != v[2]) && (v[3] != v[2]);\n\
				haveSteepLine[1]   = haveSteepLine[1] && (v[0] != v[6]) && (v[5] != v[6]);\n\
				doLineBlend[1] = (  doLineBlend[1] ||\n\
							  !((blendResult[0] != BLEND_NONE && !IsPixEqual(src[0], src[2])) ||\n\
								(blendResult[2] != BLEND_NONE && !IsPixEqual(src[0], src[6])) ||\n\
								(IsPixEqual(src[2], src[1]) && IsPixEqual(src[1], src[8]) && IsPixEqual(src[8], src[7]) && IsPixEqual(src[7], src[6]) && !IsPixEqual(src[0], src[8])) ) );\n\
				blendPix[1] = ( DistYCbCr(src[0], src[7]) <= DistYCbCr(src[0], src[1]) ) ? src[7] : src[1];\n\
				\n\
				haveShallowLine[2] = haveShallowLine[2] && (v[0] != v[8]) && (v[1] != v[8]);\n\
				haveSteepLine[2]   = haveSteepLine[2] && (v[0] != v[4]) && (v[3] != v[4]);\n\
				doLineBlend[2] = (  doLineBlend[2] ||\n\
							  !((blendResult[3] != BLEND_NONE && !IsPixEqual(src[0], src[8])) ||\n\
								(blendResult[1] != BLEND_NONE && !IsPixEqual(src[0], src[4])) ||\n\
								(IsPixEqual(src[8], src[7]) && IsPixEqual(src[7], src[6]) && IsPixEqual(src[6], src[5]) && IsPixEqual(src[5], src[4]) && !IsPixEqual(src[0], src[6])) ) );\n\
				blendPix[2] = ( DistYCbCr(src[0], src[5]) <= DistYCbCr(src[0], src[7]) ) ? src[5] : src[7];\n\
				\n\
				haveShallowLine[3] = haveShallowLine[3] && (v[0] != v[6]) && (v[7] != v[6]);\n\
				haveSteepLine[3]   = haveSteepLine[3] && (v[0] != v[2]) && (v[1] != v[2]);\n\
				doLineBlend[3] = (  doLineBlend[3] ||\n\
							  !((blendResult[2] != BLEND_NONE && !IsPixEqual(src[0], src[6])) ||\n\
								(blendResult[0] != BLEND_NONE && !IsPixEqual(src[0], src[2])) ||\n\
								(IsPixEqual(src[6], src[5]) && IsPixEqual(src[5], src[4]) && IsPixEqual(src[4], src[3]) && IsPixEqual(src[3], src[2]) && !IsPixEqual(src[0], src[4])) ) );\n\
				blendPix[3] = ( DistYCbCr(src[0], src[3]) <= DistYCbCr(src[0], src[5]) ) ? src[3] : src[5];\n\
				\n\
				if (i == 0)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? 1.000 : 0.9711013910) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 1)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.750 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? 1.000 : 0.4236372243) : 0.000);\n\
				}\n\
				else if (i == 2)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 1.000 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? ((haveShallowLine[2]) ? 1.000 : ((haveSteepLine[2]) ? 0.750 : 0.500)) : 0.05652034508) : 0.000);\n\
				}\n\
				else if (i == 3)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? ((haveSteepLine[1]) ? 1.000 : ((haveShallowLine[1]) ? 0.750 : 0.500)) : 0.05652034508) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 1.000 : 0.000);\n\
				}\n\
				else if (i == 4)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? 1.000 : 0.4236372243) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.750 : 0.000);\n\
				}\n\
				else if (i == 5)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? 1.000 : 0.9711013910) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 6)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? 1.000 : 0.4236372243) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.750 : 0.000);\n\
				}\n\
				else if (i == 7)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((!haveShallowLine[2] && !haveSteepLine[2]) ? 0.500 : 1.000) : 0.000);\n\
				}\n\
				else if (i == 8)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((haveShallowLine[2]) ? 0.750 : ((haveSteepLine[2]) ? 0.250 : 0.000)) : 0.000);\n\
				}\n\
				else if (i == 9)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((haveSteepLine[1]) ? 0.750 : ((haveShallowLine[1]) ? 0.250 : 0.000)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 10)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((!haveShallowLine[1] && !haveSteepLine[1]) ? 0.500 : 1.000) : 0.000);\n\
				}\n\
				else if (i == 11)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.750 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? 1.000 : 0.4236372243) : 0.000);\n\
				}\n\
				else if (i == 12)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? ((haveSteepLine[2]) ? 1.000 : ((haveShallowLine[2]) ? 0.750 : 0.500)) : 0.05652034508) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 1.000 : 0.000);\n\
				}\n\
				else if (i == 13)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((haveSteepLine[2]) ? 0.750 : ((haveShallowLine[2]) ? 0.250 : 0.000)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 16)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((haveShallowLine[1]) ? 0.750 : ((haveSteepLine[1]) ? 0.250 : 0.000)) : 0.000);\n\
				}\n\
				else if (i == 17)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 1.000 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? ((haveShallowLine[1]) ? 1.000 : ((haveSteepLine[1]) ? 0.750 : 0.500)) : 0.05652034508) : 0.000);\n\
				}\n\
				else if (i == 18)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 1.000 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? ((haveShallowLine[3]) ? 1.000 : ((haveSteepLine[3]) ? 0.750 : 0.500)) : 0.05652034508) : 0.000);\n\
				}\n\
				else if (i == 19)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((haveShallowLine[3]) ? 0.750 : ((haveSteepLine[3]) ? 0.250 : 0.000)) : 0.000);\n\
				}\n\
				else if (i == 22)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((haveSteepLine[0]) ? 0.750 : ((haveShallowLine[0]) ? 0.250 : 0.000)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 23)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? ((haveSteepLine[0]) ? 1.000 : ((haveShallowLine[0]) ? 0.750 : 0.500)) : 0.05652034508) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 1.000 : 0.000);\n\
				}\n\
				else if (i == 24)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.750 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? 1.000 : 0.4236372243) : 0.000);\n\
				}\n\
				else if (i == 25)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((!haveShallowLine[3] && !haveSteepLine[3]) ? 0.500 : 1.000) : 0.000);\n\
				}\n\
				else if (i == 26)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((haveSteepLine[3]) ? 0.750 : ((haveShallowLine[3]) ? 0.250 : 0.000)) : 0.000);\n\
				}\n\
				else if (i == 27)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((haveShallowLine[0]) ? 0.750 : ((haveSteepLine[0]) ? 0.250 : 0.000)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 28)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((!haveShallowLine[0] && !haveSteepLine[0]) ? 0.500 : 1.000) : 0.000);\n\
				}\n\
				else if (i == 29)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? 1.000 : 0.4236372243) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.750 : 0.000);\n\
				}\n\
				else if (i == 30)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? 1.000 : 0.9711013910) : 0.000);\n\
				}\n\
				else if (i == 31)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.750 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? 1.000 : 0.4236372243) : 0.000);\n\
				}\n\
				else if (i == 32)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 1.000 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? ((haveSteepLine[3]) ? 1.000 : ((haveShallowLine[3]) ? 0.750 : 0.500)) : 0.05652034508) : 0.000);\n\
				}\n\
				else if (i == 33)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? ((haveShallowLine[0]) ? 1.000 : ((haveSteepLine[0]) ? 0.750 : 0.500)) : 0.05652034508) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 1.000 : 0.000);\n\
				}\n\
				else if (i == 34)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? 1.000 : 0.4236372243) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.750 : 0.000);\n\
				}\n\
				else if (i == 35)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? 1.000 : 0.9711013910) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.250 : 0.000);\n\
				}\n\
			}\n\
		}\n\
		\n\
		OUT_FRAG_COLOR = vec4(newFragColor, 1.0);\n\
	}\n\
"};

enum OGLVertexAttributeID
{
	OGLVertexAttributeID_Position = 0,
	OGLVertexAttributeID_TexCoord0 = 8
};

typedef struct
{
	uint8_t p0;
	uint8_t p1;
	uint8_t p2;
	uint8_t w0;
	uint8_t w1;
	uint8_t w2;
} LUTValues;

static LUTValues *_LQ2xLUT = NULL;
static LUTValues *_HQ2xLUT = NULL;
static LUTValues *_HQ3xLUT = NULL;
static LUTValues *_HQ4xLUT = NULL;

static const GLint filterVtxBuffer[8] = {-1, -1, 1, -1, -1, 1, 1, 1};

void GetGLVersionOGL(GLint *outMajor, GLint *outMinor, GLint *outRevision)
{
	const char *oglVersionString = (const char *)glGetString(GL_VERSION);
	if (oglVersionString == NULL)
	{
		return;
	}
	
	size_t versionStringLength = 0;
	
	// First, check for the dot in the revision string. There should be at
	// least one present.
	const char *versionStrEnd = strstr(oglVersionString, ".");
	if (versionStrEnd == NULL)
	{
		return;
	}
	
	// Next, check for the space before the vendor-specific info (if present).
	versionStrEnd = strstr(oglVersionString, " ");
	if (versionStrEnd == NULL)
	{
		// If a space was not found, then the vendor-specific info is not present,
		// and therefore the entire string must be the version number.
		versionStringLength = strlen(oglVersionString);
	}
	else
	{
		// If a space was found, then the vendor-specific info is present,
		// and therefore the version number is everything before the space.
		versionStringLength = versionStrEnd - oglVersionString;
	}
	
	// Copy the version substring and parse it.
	char *versionSubstring = (char *)malloc(versionStringLength * sizeof(char));
	strncpy(versionSubstring, oglVersionString, versionStringLength);
	
	unsigned int major = 0;
	unsigned int minor = 0;
	unsigned int revision = 0;
	
	sscanf(versionSubstring, "%u.%u.%u", &major, &minor, &revision);
	
	free(versionSubstring);
	versionSubstring = NULL;
	
	if (outMajor != NULL)
	{
		*outMajor = major;
	}
	
	if (outMinor != NULL)
	{
		*outMinor = minor;
	}
	
	if (outRevision != NULL)
	{
		*outRevision = revision;
	}
}

void glBindVertexArray_LegacyAPPLE(GLuint vaoID)
{
	glBindVertexArrayAPPLE(vaoID);
}

void glDeleteVertexArrays_LegacyAPPLE(GLsizei n, const GLuint *vaoIDs)
{
	glDeleteVertexArraysAPPLE(n, vaoIDs);
}

void glGenVertexArrays_LegacyAPPLE(GLsizei n, GLuint *vaoIDs)
{
	glGenVertexArraysAPPLE(n, vaoIDs);
}

void (*glBindVertexArrayDESMUME)(GLuint id) = &glBindVertexArray_LegacyAPPLE;
void (*glDeleteVertexArraysDESMUME)(GLsizei n, const GLuint *ids) = &glDeleteVertexArrays_LegacyAPPLE;
void (*glGenVertexArraysDESMUME)(GLsizei n, GLuint *ids) = &glGenVertexArrays_LegacyAPPLE;

static LUTValues PackLUTValues(const uint8_t p0, const uint8_t p1, const uint8_t p2, const uint8_t w0, const uint8_t w1, const uint8_t w2)
{
	const uint8_t wR = 256 / (w0 + w1 + w2);
	return (LUTValues) {
		(uint8_t)(p0*31),
		(uint8_t)(p1*31),
		(uint8_t)(p2*31),
		(uint8_t)((w1 == 0 && w2 == 0) ? 255 : w0*wR),
		(uint8_t)(w1*wR),
		(uint8_t)(w2*wR)
	};
}

static void InitHQnxLUTs()
{
	static bool lutValuesInited = false;
	
	if (lutValuesInited)
	{
		return;
	}
	
	_LQ2xLUT = (LUTValues *)malloc(256*(2*2)*16 * sizeof(LUTValues));
	_HQ2xLUT = (LUTValues *)malloc(256*(2*2)*16 * sizeof(LUTValues));
	_HQ3xLUT = (LUTValues *)malloc(256*(3*3)*16 * sizeof(LUTValues) + 2);
	_HQ4xLUT = (LUTValues *)malloc(256*(4*4)*16 * sizeof(LUTValues) + 4); // The bytes fix a mysterious crash that intermittently occurs. Don't know why this works... it just does.
	
#define MUR (compare & 0x01) // top-right
#define MDR (compare & 0x02) // bottom-right
#define MDL (compare & 0x04) // bottom-left
#define MUL (compare & 0x08) // top-left
#define IC(p0)			PackLUTValues(p0, p0, p0,  1, 0, 0)
#define I11(p0,p1)		PackLUTValues(p0, p1, p0,  1, 1, 0)
#define I211(p0,p1,p2)	PackLUTValues(p0, p1, p2,  2, 1, 1)
#define I31(p0,p1)		PackLUTValues(p0, p1, p0,  3, 1, 0)
#define I332(p0,p1,p2)	PackLUTValues(p0, p1, p2,  3, 3, 2)
#define I431(p0,p1,p2)	PackLUTValues(p0, p1, p2,  4, 3, 1)
#define I521(p0,p1,p2)	PackLUTValues(p0, p1, p2,  5, 2, 1)
#define I53(p0,p1)		PackLUTValues(p0, p1, p0,  5, 3, 0)
#define I611(p0,p1,p2)	PackLUTValues(p0, p1, p2,  6, 1, 1)
#define I71(p0,p1)		PackLUTValues(p0, p1, p0,  7, 1, 0)
#define I772(p0,p1,p2)	PackLUTValues(p0, p1, p2,  7, 7, 2)
#define I97(p0,p1)		PackLUTValues(p0, p1, p0,  9, 7, 0)
#define I1411(p0,p1,p2)	PackLUTValues(p0, p1, p2, 14, 1, 1)
#define I151(p0,p1)		PackLUTValues(p0, p1, p0, 15, 1, 0)
	
#define P0 _LQ2xLUT[pattern+(256*0)+(1024*compare)]
#define P1 _LQ2xLUT[pattern+(256*1)+(1024*compare)]
#define P2 _LQ2xLUT[pattern+(256*2)+(1024*compare)]
#define P3 _LQ2xLUT[pattern+(256*3)+(1024*compare)]
	for (size_t compare = 0; compare < 16; compare++)
	{
		for (size_t pattern = 0; pattern < 256; pattern++)
		{
			switch (pattern)
			{
				#include "../../filter/lq2x.h"
			}
		}
	}
#undef P0
#undef P1
#undef P2
#undef P3

#define P0 _HQ2xLUT[pattern+(256*0)+(1024*compare)]
#define P1 _HQ2xLUT[pattern+(256*1)+(1024*compare)]
#define P2 _HQ2xLUT[pattern+(256*2)+(1024*compare)]
#define P3 _HQ2xLUT[pattern+(256*3)+(1024*compare)]
	for (size_t compare = 0; compare < 16; compare++)
	{
		for (size_t pattern = 0; pattern < 256; pattern++)
		{
			switch (pattern)
			{
				#include "../../filter/hq2x.h"
			}
		}
	}
#undef P0
#undef P1
#undef P2
#undef P3

#define P(a, b)						_HQ3xLUT[pattern+(256*((b*3)+a))+(2304*compare)]
#define I1(p0)						PackLUTValues(p0, p0, p0,  1,  0,  0)
#define I2(i0, i1, p0, p1)			PackLUTValues(p0, p1, p0, i0, i1,  0)
#define I3(i0, i1, i2, p0, p1, p2)	PackLUTValues(p0, p1, p2, i0, i1, i2)
	for (size_t compare = 0; compare < 16; compare++)
	{
		for (size_t pattern = 0; pattern < 256; pattern++)
		{
			switch (pattern)
			{
				#include "../../filter/hq3x.dat"
			}
		}
	}
#undef P
#undef I1
#undef I2
#undef I3

#define P(a, b)						_HQ4xLUT[pattern+(256*((b*4)+a))+(4096*compare)]
#define I1(p0)						PackLUTValues(p0, p0, p0,  1,  0,  0)
#define I2(i0, i1, p0, p1)			PackLUTValues(p0, p1, p0, i0, i1,  0)
#define I3(i0, i1, i2, p0, p1, p2)	PackLUTValues(p0, p1, p2, i0, i1, i2)
	for (size_t compare = 0; compare < 16; compare++)
	{
		for (size_t pattern = 0; pattern < 256; pattern++)
		{
			switch (pattern)
			{
				#include "../../filter/hq4x.dat"
			}
		}
	}
#undef P
#undef I1
#undef I2
#undef I3
	
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef IC
#undef I11
#undef I211
#undef I31
#undef I332
#undef I431
#undef I521
#undef I53
#undef I611
#undef I71
#undef I772
#undef I97
#undef I1411
#undef I151
	
	lutValuesInited = true;
}

#pragma mark -

OGLContextInfo::OGLContextInfo()
{
	GetGLVersionOGL(&_versionMajor, &_versionMinor, &_versionRevision);
	_shaderSupport = ShaderSupport_Unsupported;
	_useShader150 = false;
	
	_isVBOSupported = false;
	_isVAOSupported = false;
	_isPBOSupported = false;
	_isFBOSupported = false;
}

ShaderSupportTier OGLContextInfo::GetShaderSupport()
{
	return this->_shaderSupport;
}

bool OGLContextInfo::IsUsingShader150()
{
	return this->_useShader150;
}

bool OGLContextInfo::IsVBOSupported()
{
	return this->_isVBOSupported;
}

bool OGLContextInfo::IsVAOSupported()
{
	return this->_isVAOSupported;
}

bool OGLContextInfo::IsPBOSupported()
{
	return this->_isPBOSupported;
}

bool OGLContextInfo::IsShaderSupported()
{
	return (this->_shaderSupport != ShaderSupport_Unsupported);
}

bool OGLContextInfo::IsFBOSupported()
{
	return this->_isFBOSupported;
}

OGLContextInfo_Legacy::OGLContextInfo_Legacy()
{
	_shaderSupport = ShaderSupport_Unsupported;
	_useShader150 = false;
	
	// Check the OpenGL capabilities for this renderer
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSetOGL(&oglExtensionSet);
	
	_isVBOSupported = this->IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_buffer_object");
	
	bool isShaderSupported	= this->IsExtensionPresent(oglExtensionSet, "GL_ARB_shader_objects") &&
							  this->IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_shader") &&
							  this->IsExtensionPresent(oglExtensionSet, "GL_ARB_fragment_shader") &&
							  this->IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_program");
	if (isShaderSupported)
	{
		if ( _versionMajor < 3 ||
			(_versionMajor == 3 && _versionMinor < 2) )
		{
			if (_versionMajor < 2)
			{
				_shaderSupport = ShaderSupport_Unsupported;
			}
			else
			{
				GLint maxVaryingFloats = 0;
				glGetIntegerv(GL_MAX_VARYING_FLOATS_ARB, &maxVaryingFloats);
				
				if (_versionMajor == 2 && _versionMinor == 0)
				{
					if (maxVaryingFloats < 32)
					{
						_shaderSupport = ShaderSupport_BottomTier;
					}
					else
					{
						_shaderSupport = ShaderSupport_LowTier;
					}
				}
				else
				{
					if (maxVaryingFloats < 32)
					{
						_shaderSupport = ShaderSupport_BottomTier;
					}
					else if (maxVaryingFloats < 60)
					{
						_shaderSupport = ShaderSupport_LowTier;
					}
					else if (maxVaryingFloats < 84)
					{
						_shaderSupport = ShaderSupport_MidTier;
					}
					else if (maxVaryingFloats < 108)
					{
						_shaderSupport = ShaderSupport_HighTier;
					}
					else if (maxVaryingFloats >= 108)
					{
						_shaderSupport = ShaderSupport_TopTier;
					}
				}
			}
		}
		else
		{
			_useShader150 = true;
			_shaderSupport = ShaderSupport_MidTier;
			
			if (_versionMajor == 4)
			{
				if (_versionMinor <= 1)
				{
					_shaderSupport = ShaderSupport_HighTier;
				}
				else
				{
					_shaderSupport = ShaderSupport_TopTier;
				}
			}
		}
	}
	
	_isVAOSupported =   isShaderSupported &&
	                   _isVBOSupported &&
	                  (this->IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_array_object") ||
	                   this->IsExtensionPresent(oglExtensionSet, "GL_APPLE_vertex_array_object"));
	
	_isPBOSupported	= this->IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_buffer_object") &&
					 (this->IsExtensionPresent(oglExtensionSet, "GL_ARB_pixel_buffer_object") ||
					  this->IsExtensionPresent(oglExtensionSet, "GL_EXT_pixel_buffer_object"));
	
	_isFBOSupported	= this->IsExtensionPresent(oglExtensionSet, "GL_EXT_framebuffer_object");
}

void OGLContextInfo_Legacy::GetExtensionSetOGL(std::set<std::string> *oglExtensionSet)
{
	std::string oglExtensionString = std::string((const char *)glGetString(GL_EXTENSIONS));
	
	size_t extStringStartLoc = 0;
	size_t delimiterLoc = oglExtensionString.find_first_of(' ', extStringStartLoc);
	while (delimiterLoc != std::string::npos)
	{
		std::string extensionName = oglExtensionString.substr(extStringStartLoc, delimiterLoc - extStringStartLoc);
		oglExtensionSet->insert(extensionName);
		
		extStringStartLoc = delimiterLoc + 1;
		delimiterLoc = oglExtensionString.find_first_of(' ', extStringStartLoc);
	}
	
	if (extStringStartLoc - oglExtensionString.length() > 0)
	{
		std::string extensionName = oglExtensionString.substr(extStringStartLoc, oglExtensionString.length() - extStringStartLoc);
		oglExtensionSet->insert(extensionName);
	}
}

bool OGLContextInfo_Legacy::IsExtensionPresent(const std::set<std::string> &oglExtensionSet, const std::string &extensionName) const
{
	if (oglExtensionSet.size() == 0)
	{
		return false;
	}
	
	return (oglExtensionSet.find(extensionName) != oglExtensionSet.end());
}

#pragma mark -

OGLShaderProgram::OGLShaderProgram()
{
	_vertexID = 0;
	_fragmentID = 0;

	_programID = glCreateProgram();
	if (_programID == 0)
	{
		printf("OpenGL Error - Failed to create shader program.\n");
	}
}

OGLShaderProgram::~OGLShaderProgram()
{
	glDetachShader(this->_programID, this->_vertexID);
	glDetachShader(this->_programID, this->_fragmentID);
	glDeleteProgram(this->_programID);
	glDeleteShader(this->_vertexID);
	glDeleteShader(this->_fragmentID);
}

GLuint OGLShaderProgram::LoadShaderOGL(GLenum shaderType, const char *shaderProgram, bool useShader150)
{
	GLint shaderStatus = GL_TRUE;
	
	GLuint shaderID = glCreateShader(shaderType);
	if (shaderID == 0)
	{
		printf("OpenGL Error - Failed to create %s.\n",
			   (shaderType == GL_VERTEX_SHADER) ? "GL_VERTEX_SHADER" : ((shaderType == GL_FRAGMENT_SHADER) ? "GL_FRAGMENT_SHADER" : "OTHER SHADER TYPE"));
		return shaderID;
	}
	
	std::string shaderSupportStr;
	// Convert _shaderSupport to std::string.
	switch (_shaderSupport)
	{
		case ShaderSupport_BottomTier:
			shaderSupportStr = "SHADERSUPPORT_BOTTOM_TIER";
			break;
			
		case ShaderSupport_LowTier:
			shaderSupportStr = "SHADERSUPPORT_LOW_TIER";
			break;
			
		case ShaderSupport_MidTier:
			shaderSupportStr = "SHADERSUPPORT_MID_TIER";
			break;
			
		case ShaderSupport_HighTier:
			shaderSupportStr = "SHADERSUPPORT_HIGH_TIER";
			break;
			
		case ShaderSupport_TopTier:
			shaderSupportStr = "SHADERSUPPORT_TOP_TIER";
			break;
			
		case ShaderSupport_FutureTier:
			shaderSupportStr = "SHADERSUPPORT_FUTURE_TIER";
			break;
			
		default:
			shaderSupportStr = "SHADERSUPPORT_UNSUPPORTED";
			break;
	}
	
	std::string programSource;
	if (useShader150)
	{
		programSource += "#version 150\n";
		
		if (shaderType == GL_VERTEX_SHADER)
		{
			programSource += "#define ATTRIBUTE in\n";
			programSource += "#define VARYING out\n";
		}
		else if (shaderType == GL_FRAGMENT_SHADER)
		{
			programSource += "#define VARYING in\n";
			programSource += "#define SAMPLE3_TEX_RECT(t,c) texture(t,c).rgb\n";
			programSource += "#define SAMPLE3_TEX_1D(t,c)   texture(t,c).rgb\n";
			programSource += "#define SAMPLE3_TEX_2D(t,c)   texture(t,c).rgb\n";
			programSource += "#define SAMPLE3_TEX_3D(t,c)   texture(t,c).rgb\n";
			programSource += "#define SAMPLE4_TEX_1D(t,c)   texture(t,c)\n";
			programSource += "#define SAMPLE4_TEX_2D(t,c)   texture(t,c)\n";
			programSource += "#define SAMPLE4_TEX_3D(t,c)   texture(t,c)\n";
			programSource += "#define OUT_FRAG_COLOR outFragColor\n\n\n";
			programSource += "out vec4 outFragColor;\n\n";
		}
	}
	else
	{
		programSource += "#version 110\n";
		
		if (shaderType == GL_VERTEX_SHADER)
		{
			programSource += "#define ATTRIBUTE attribute\n";
			programSource += "#define VARYING varying\n\n";
		}
		else if (shaderType == GL_FRAGMENT_SHADER)
		{
			programSource += "#extension GL_ARB_texture_rectangle : require\n";
			programSource += "#define VARYING varying\n";
			programSource += "#define SAMPLE3_TEX_RECT(t,c) texture2DRect(t,c).rgb\n";
			programSource += "#define SAMPLE3_TEX_1D(t,c)   texture1D(t,c).rgb\n";
			programSource += "#define SAMPLE3_TEX_2D(t,c)   texture2D(t,c).rgb\n";
			programSource += "#define SAMPLE3_TEX_3D(t,c)   texture3D(t,c).rgb\n";
			programSource += "#define SAMPLE4_TEX_1D(t,c)   texture1D(t,c)\n";
			programSource += "#define SAMPLE4_TEX_2D(t,c)   texture2D(t,c)\n";
			programSource += "#define SAMPLE4_TEX_3D(t,c)   texture3D(t,c)\n";
			programSource += "#define OUT_FRAG_COLOR gl_FragColor\n\n";
		}
	}
	
	programSource += "#define SHADERSUPPORT_UNSUPPORTED		0\n";
	programSource += "#define SHADERSUPPORT_BOTTOM_TIER		1\n";
	programSource += "#define SHADERSUPPORT_LOW_TIER		2\n";
	programSource += "#define SHADERSUPPORT_MID_TIER		3\n";
	programSource += "#define SHADERSUPPORT_HIGH_TIER		4\n";
	programSource += "#define SHADERSUPPORT_TOP_TIER		5\n";
	programSource += "#define SHADERSUPPORT_FUTURE_TIER		6\n";
	programSource += "#define GPU_TIER " + shaderSupportStr + "\n\n";
	programSource += shaderProgram;
	
	const char *programSourceChar = programSource.c_str();
	glShaderSource(shaderID, 1, (const GLchar **)&programSourceChar, NULL);
	glCompileShader(shaderID);
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &shaderStatus);
	if (shaderStatus == GL_FALSE)
	{
		static const size_t logBytes = 16384; // 16KB should be more than enough
		GLchar *logBuf = (GLchar *)calloc(logBytes, sizeof(GLchar));
		GLsizei logSize = 0;
		glGetShaderInfoLog(shaderID, logBytes * sizeof(GLchar), &logSize, logBuf);
		
		printf("OpenGL Error - Failed to compile %s.\n%s\n",
			   (shaderType == GL_VERTEX_SHADER) ? "GL_VERTEX_SHADER" : ((shaderType == GL_FRAGMENT_SHADER) ? "GL_FRAGMENT_SHADER" : "OTHER SHADER TYPE"),
			   (char *)logBuf);
		
		glDeleteShader(shaderID);
		return shaderID;
	}
	
	return shaderID;
}

ShaderSupportTier OGLShaderProgram::GetShaderSupport()
{
	return this->_shaderSupport;
}

void OGLShaderProgram::SetShaderSupport(const ShaderSupportTier theTier)
{
	this->_shaderSupport = theTier;
}

GLuint OGLShaderProgram::GetVertexShaderID()
{
	return this->_vertexID;
}

void OGLShaderProgram::SetVertexShaderOGL(const char *shaderProgram, bool useShader150)
{
	if (this->_vertexID != 0)
	{
		glDetachShader(this->_programID, this->_vertexID);
		glDeleteShader(this->_vertexID);
	}
	
	this->_vertexID = this->LoadShaderOGL(GL_VERTEX_SHADER, shaderProgram, useShader150);
	
	if (this->_vertexID != 0)
	{
		glAttachShader(this->_programID, this->_vertexID);
		glBindAttribLocation(this->_programID, OGLVertexAttributeID_Position, "inPosition");
		glBindAttribLocation(this->_programID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	}
	
	if (this->_vertexID != 0 && this->_fragmentID != 0)
	{
		this->LinkOGL();
	}
}

GLuint OGLShaderProgram::GetFragmentShaderID()
{
	return this->_fragmentID;
}

void OGLShaderProgram::SetFragmentShaderOGL(const char *shaderProgram, bool useShader150)
{
	if (this->_fragmentID != 0)
	{
		glDetachShader(this->_programID, this->_fragmentID);
		glDeleteShader(this->_fragmentID);
	}
	
	this->_fragmentID = this->LoadShaderOGL(GL_FRAGMENT_SHADER, shaderProgram, useShader150);
	
	if (this->_fragmentID != 0)
	{
		glAttachShader(this->_programID, this->_fragmentID);
		if (useShader150)
		{
			glBindFragDataLocationEXT(this->_programID, 0, "outFragColor");
		}
	}
	
	if (this->_vertexID != 0 && this->_fragmentID != 0)
	{
		this->LinkOGL();
	}
}

void OGLShaderProgram::SetVertexAndFragmentShaderOGL(const char *vertShaderProgram, const char *fragShaderProgram, bool useShader150)
{
	if (this->_vertexID != 0)
	{
		glDetachShader(this->_programID, this->_vertexID);
		glDeleteShader(this->_vertexID);
	}
	
	if (this->_fragmentID != 0)
	{
		glDetachShader(this->_programID, this->_fragmentID);
		glDeleteShader(this->_fragmentID);
	}
	
	this->_vertexID = this->LoadShaderOGL(GL_VERTEX_SHADER, vertShaderProgram, useShader150);
	this->_fragmentID = this->LoadShaderOGL(GL_FRAGMENT_SHADER, fragShaderProgram, useShader150);
	
	if (this->_vertexID != 0)
	{
		glAttachShader(this->_programID, this->_vertexID);
		glBindAttribLocation(this->_programID, OGLVertexAttributeID_Position, "inPosition");
		glBindAttribLocation(this->_programID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	}
	
	if (this->_fragmentID != 0)
	{
		glAttachShader(this->_programID, this->_fragmentID);
		if (useShader150)
		{
			glBindFragDataLocationEXT(this->_programID, 0, "outFragColor");
		}
	}
	
	if (this->_vertexID != 0 && this->_fragmentID != 0)
	{
		this->LinkOGL();
	}
}

GLuint OGLShaderProgram::GetProgramID()
{
	return this->_programID;
}

bool OGLShaderProgram::LinkOGL()
{
	bool result = false;
	GLint shaderStatus = GL_FALSE;
	
	glLinkProgram(this->_programID);
	glGetProgramiv(this->_programID, GL_LINK_STATUS, &shaderStatus);
	if (shaderStatus == GL_FALSE)
	{
		printf("OpenGL Error - Failed to link shader program.\n");
		return result;
	}
	
	glValidateProgram(this->_programID);
	
	result = true;
	return result;
}

#pragma mark -

OGLVideoOutput::OGLVideoOutput()
{
	_contextInfo = NULL;
	_needUpdateViewport = true;
	_layerList = new std::vector<OGLVideoLayer *>;
	_layerList->reserve(8);
}

OGLVideoOutput::~OGLVideoOutput()
{
	if (this->_layerList != NULL)
	{
		for (size_t i = 0; i < _layerList->size(); i++)
		{
			delete (*this->_layerList)[i];
		}
		
		delete this->_layerList;
		this->_layerList = NULL;
	}
}

void OGLVideoOutput::_UpdateNormalSize()
{
	this->GetDisplayLayer()->SetNeedsUpdateVertices();
}

void OGLVideoOutput::_UpdateOrder()
{
	this->GetDisplayLayer()->SetNeedsUpdateVertices();
}

void OGLVideoOutput::_UpdateRotation()
{
	this->GetDisplayLayer()->SetNeedsUpdateRotationScale();
}

void OGLVideoOutput::_UpdateClientSize()
{
	this->_viewportWidth  = (GLsizei)(this->_renderProperty.clientWidth  + 0.0001);
	this->_viewportHeight = (GLsizei)(this->_renderProperty.clientHeight + 0.0001);
	this->_needUpdateViewport = true;
	
	this->GetHUDLayer()->SetNeedsUpdateVertices();
}

void OGLVideoOutput::_UpdateViewScale()
{
	this->ClientDisplayView::_UpdateViewScale();
	this->GetDisplayLayer()->SetNeedsUpdateRotationScale();
}

void OGLVideoOutput::_UpdateViewport()
{
	glViewport(0, 0, this->_viewportWidth, this->_viewportHeight);
	
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		OGLVideoLayer *theLayer = (*_layerList)[i];
		
		if (theLayer->IsVisible())
		{
			theLayer->UpdateViewportOGL();
		}
	}
	
	this->_needUpdateViewport = false;
}

void OGLVideoOutput::_LoadNativeDisplayByID(const NDSDisplayID displayID)
{
	this->GetDisplayLayer()->LoadNativeDisplayByID_OGL(displayID);
}

void OGLVideoOutput::_LoadCustomDisplayByID(const NDSDisplayID displayID)
{
	this->GetDisplayLayer()->LoadCustomDisplayByID_OGL(displayID);
}

OGLContextInfo* OGLVideoOutput::GetContextInfo()
{
	return this->_contextInfo;
}

void OGLVideoOutput::Init()
{
	this->_canFilterOnGPU = ( this->_contextInfo->IsShaderSupported() && this->_contextInfo->IsFBOSupported() );
	this->_filtersPreferGPU = this->_canFilterOnGPU;
	this->_willFilterOnGPU = false;
	
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		delete (*_layerList)[i];
	}
	
	this->_layerList->clear();
	this->_layerList->push_back(new OGLDisplayLayer(this));
	this->_layerList->push_back(new OGLHUDLayer(this));
	
	// Render State Setup (common to both shaders and fixed-function pipeline)
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);
	glDisable(GL_STENCIL_TEST);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	// Set up fixed-function pipeline render states.
	if (!this->_contextInfo->IsShaderSupported())
	{
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_LIGHTING);
		glDisable(GL_FOG);
		glEnable(GL_TEXTURE_RECTANGLE_ARB);
	}
	
	// Set up clear attributes
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void OGLVideoOutput::SetOutputFilter(const OutputFilterTypeID filterID)
{
	this->_outputFilter = this->GetDisplayLayer()->SetOutputFilterOGL(filterID);
	this->GetDisplayLayer()->SetNeedsUpdateRotationScale();
	this->_needUpdateViewport = true;
}

void OGLVideoOutput::SetPixelScaler(const VideoFilterTypeID filterID)
{
	std::string cpuTypeIDString = std::string( VideoFilter::GetTypeStringByID(filterID) );
	const VideoFilterTypeID newFilterID = (cpuTypeIDString != std::string(VIDEOFILTERTYPE_UNKNOWN_STRING)) ? filterID : VideoFilterTypeID_None;
	
	this->GetDisplayLayer()->SetCPUPixelScalerOGL(newFilterID);
	
	this->_pixelScaler = newFilterID;
	this->_willFilterOnGPU = (this->GetFiltersPreferGPU()) ? this->GetDisplayLayer()->SetGPUPixelScalerOGL(newFilterID) : false;
}

void OGLVideoOutput::CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo)
{
	this->GetHUDLayer()->CopyHUDFont(fontFace, glyphSize, glyphTileSize, glyphInfo);
}

void OGLVideoOutput::SetHUDVisibility(const bool visibleState)
{
	this->ClientDisplay3DView::SetHUDVisibility(visibleState);
	this->GetHUDLayer()->SetVisibility(visibleState);
}

void OGLVideoOutput::SetFiltersPreferGPU(const bool preferGPU)
{
	this->_filtersPreferGPU = preferGPU;
	this->_willFilterOnGPU = (preferGPU) ? this->GetDisplayLayer()->SetGPUPixelScalerOGL(this->_pixelScaler) : false;
	
	this->GetDisplayLayer()->SetFiltersPreferGPUOGL();
}

void OGLVideoOutput::SetVideoBuffers(const uint32_t colorFormat,
									 const void *videoBufferHead,
									 const void *nativeBuffer0,
									 const void *nativeBuffer1,
									 const void *customBuffer0, const size_t customWidth0, const size_t customHeight0,
									 const void *customBuffer1, const size_t customWidth1, const size_t customHeight1)
{
	this->GetDisplayLayer()->SetVideoBuffers(colorFormat, videoBufferHead,
											 nativeBuffer0, nativeBuffer1,
											 customBuffer0, customWidth0, customHeight0,
											 customBuffer1, customWidth1, customHeight1);
}

GLsizei OGLVideoOutput::GetViewportWidth()
{
	return this->_viewportWidth;
}

GLsizei OGLVideoOutput::GetViewportHeight()
{
	return this->_viewportHeight;
}

OGLDisplayLayer* OGLVideoOutput::GetDisplayLayer()
{
	return (OGLDisplayLayer *)this->_layerList->at(0);
}

OGLHUDLayer* OGLVideoOutput::GetHUDLayer()
{
	return (OGLHUDLayer *)this->_layerList->at(1);
}

void OGLVideoOutput::ProcessDisplays()
{
	OGLDisplayLayer *displayLayer = this->GetDisplayLayer();
	if (displayLayer->IsVisible())
	{
		displayLayer->ProcessOGL();
	}
}

void OGLVideoOutput::FrameProcessHUD()
{	
	if (this->GetHUDVisibility())
	{
		this->GetHUDLayer()->ProcessOGL();
	}
}

void OGLVideoOutput::FrameRender()
{
	if (this->_needUpdateViewport)
	{
		this->_UpdateViewport();
	}
	
	glClear(GL_COLOR_BUFFER_BIT);
	
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		OGLVideoLayer *theLayer = (*_layerList)[i];
		
		if (theLayer->IsVisible())
		{
			theLayer->RenderOGL();
		}
	}
}

void OGLVideoOutput::FrameFinish()
{
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		OGLVideoLayer *theLayer = (*_layerList)[i];
		
		if (theLayer->IsVisible())
		{
			theLayer->FinishOGL();
		}
	}
}

#pragma mark -

OGLFilter::OGLFilter()
{
	OGLFilterInit(1, 1, 1);
}

OGLFilter::OGLFilter(GLsizei srcWidth, GLsizei srcHeight, GLfloat scale = 1)
{
	OGLFilterInit(srcWidth, srcHeight, scale);
}

OGLFilter::~OGLFilter()
{
	if (_isVAOPresent)
	{
		glDeleteVertexArraysDESMUME(1, &this->_vaoID);
		_isVAOPresent = false;
	}
	
	glDeleteFramebuffersEXT(1, &this->_fboID);
	glDeleteTextures(1, &this->_texDstID);
	glDeleteBuffers(1, &this->_vboVtxID);
	glDeleteBuffers(1, &this->_vboTexCoordID);
}

void OGLFilter::GetSupport(int vfTypeID, bool *outSupportCPU, bool *outSupportShader)
{
	const char *cpuTypeIDString = VideoFilter::GetTypeStringByID((VideoFilterTypeID)vfTypeID);
	*outSupportCPU		= (strstr(cpuTypeIDString, VIDEOFILTERTYPE_UNKNOWN_STRING) == NULL);
	*outSupportShader	= (vfTypeID == VideoFilterTypeID_Nearest1_5X) ||
						  (vfTypeID == VideoFilterTypeID_Nearest2X) ||
						  (vfTypeID == VideoFilterTypeID_Scanline) ||
						  (vfTypeID == VideoFilterTypeID_EPX) ||
						  (vfTypeID == VideoFilterTypeID_EPXPlus) ||
						  (vfTypeID == VideoFilterTypeID_2xSaI) ||
						  (vfTypeID == VideoFilterTypeID_Super2xSaI) ||
						  (vfTypeID == VideoFilterTypeID_SuperEagle);
}

void OGLFilter::OGLFilterInit(GLsizei srcWidth, GLsizei srcHeight, GLfloat scale)
{
	_program = new OGLShaderProgram;
	
	_scale = scale;
	_srcWidth = srcWidth;
	_srcHeight = srcHeight;
	_dstWidth = _srcWidth * _scale;
	_dstHeight = _srcHeight * _scale;
	
	_texCoordBuffer[0] = 0;
	_texCoordBuffer[1] = 0;
	_texCoordBuffer[2] = _srcWidth;
	_texCoordBuffer[3] = 0;
	_texCoordBuffer[4] = 0;
	_texCoordBuffer[5] = _srcHeight;
	_texCoordBuffer[6] = _srcWidth;
	_texCoordBuffer[7] = _srcHeight;
	
	glGenTextures(1, &_texDstID);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texDstID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, _dstWidth, _dstHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	glGenFramebuffersEXT(1, &_fboID);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fboID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, _texDstID, 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	
	glGenBuffers(1, &_vboVtxID);
	glGenBuffers(1, &_vboTexCoordID);
	
	glBindBuffer(GL_ARRAY_BUFFER, _vboVtxID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(filterVtxBuffer), filterVtxBuffer, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, _vboTexCoordID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(_texCoordBuffer), _texCoordBuffer, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glGenVertexArraysDESMUME(1, &_vaoID);
	glBindVertexArrayDESMUME(_vaoID);
	
	glBindBuffer(GL_ARRAY_BUFFER, _vboVtxID);
	glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_INT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, _vboTexCoordID);
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_INT, GL_FALSE, 0, 0);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	
	glBindVertexArrayDESMUME(0);
	_isVAOPresent = true;
}

OGLShaderProgram* OGLFilter::GetProgram()
{
	return this->_program;
}

GLuint OGLFilter::GetDstTexID()
{
	return this->_texDstID;
}

GLsizei OGLFilter::GetSrcWidth()
{
	return this->_srcWidth;
}

GLsizei OGLFilter::GetSrcHeight()
{
	return this->_srcHeight;
}

GLsizei OGLFilter::GetDstWidth()
{
	return this->_dstWidth;
}

GLsizei OGLFilter::GetDstHeight()
{
	return this->_dstHeight;
}

void OGLFilter::SetSrcSizeOGL(GLsizei w, GLsizei h)
{
	this->_srcWidth = w;
	this->_srcHeight = h;
	this->_dstWidth = this->_srcWidth * this->_scale;
	this->_dstHeight = this->_srcHeight * this->_scale;
	
	this->_texCoordBuffer[2] = w;
	this->_texCoordBuffer[4] = w;
	this->_texCoordBuffer[5] = h;
	this->_texCoordBuffer[7] = h;
	
	glBindBuffer(GL_ARRAY_BUFFER, this->_vboTexCoordID);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(this->_texCoordBuffer) , this->_texCoordBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	uint32_t *tempDstBuffer = (uint32_t *)calloc(this->_dstWidth * this->_dstHeight, sizeof(uint32_t));
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDstID);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, this->_dstWidth, this->_dstHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, tempDstBuffer);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	free(tempDstBuffer);
}

GLfloat OGLFilter::GetScale()
{
	return this->_scale;
}

void OGLFilter::SetScaleOGL(GLfloat scale)
{
	this->_scale = scale;
	this->_dstWidth = this->_srcWidth * this->_scale;
	this->_dstHeight = this->_srcHeight * this->_scale;
	
	uint32_t *tempDstBuffer = (uint32_t *)calloc(this->_dstWidth * this->_dstHeight, sizeof(uint32_t));
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDstID);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, this->_dstWidth, this->_dstHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, tempDstBuffer);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	free(tempDstBuffer);
}

GLuint OGLFilter::RunFilterOGL(GLuint srcTexID)
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, this->_fboID);
	glBindVertexArrayDESMUME(this->_vaoID);
	glUseProgram(this->_program->GetProgramID());
	glViewport(0, 0, this->_dstWidth, this->_dstHeight);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTexID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	glBindVertexArrayDESMUME(0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	
	return this->GetDstTexID();
}

void OGLFilter::DownloadDstBufferOGL(uint32_t *dstBuffer, size_t lineOffset, size_t readLineCount)
{
	if (dstBuffer == NULL)
	{
		return;
	}
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, this->_fboID);
	glReadPixels(0, lineOffset, this->_dstWidth, readLineCount, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, dstBuffer);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

#pragma mark -

OGLFilterDeposterize::OGLFilterDeposterize(GLsizei srcWidth, GLsizei srcHeight, ShaderSupportTier theTier, bool useShader150)
{
	SetSrcSizeOGL(srcWidth, srcHeight);
	
	glGenTextures(1, &_texIntermediateID);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texIntermediateID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, _dstWidth, _dstHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	_program->SetShaderSupport(theTier);
	_program->SetVertexShaderOGL(Sample3x3_VertShader_110, useShader150);
	_program->SetFragmentShaderOGL(FilterDeposterizeFragShader_110, useShader150);
}

OGLFilterDeposterize::~OGLFilterDeposterize()
{
	glDeleteTextures(1, &this->_texIntermediateID);
}

void OGLFilterDeposterize::SetSrcSizeOGL(GLsizei w, GLsizei h)
{
	this->_srcWidth = w;
	this->_srcHeight = h;
	this->_dstWidth = this->_srcWidth * this->_scale;
	this->_dstHeight = this->_srcHeight * this->_scale;
	
	this->_texCoordBuffer[2] = w;
	this->_texCoordBuffer[5] = h;
	this->_texCoordBuffer[6] = w;
	this->_texCoordBuffer[7] = h;
	
	glBindBuffer(GL_ARRAY_BUFFER, this->_vboTexCoordID);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(this->_texCoordBuffer), this->_texCoordBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	uint32_t *tempDstBuffer = (uint32_t *)calloc(this->_dstWidth * this->_dstHeight, sizeof(uint32_t));
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDstID);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, this->_dstWidth, this->_dstHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, tempDstBuffer);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texIntermediateID);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, this->_dstWidth, this->_dstHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, tempDstBuffer);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	free(tempDstBuffer);
}

GLuint OGLFilterDeposterize::RunFilterOGL(GLuint srcTexID)
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, this->_fboID);
	glBindVertexArrayDESMUME(this->_vaoID);
	glUseProgram(this->_program->GetProgramID());
	glViewport(0, 0, this->_dstWidth, this->_dstHeight);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTexID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, this->_texIntermediateID, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texIntermediateID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, this->_texDstID, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	glBindVertexArrayDESMUME(0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	
	return this->GetDstTexID();
}

#pragma mark -

OGLImage::OGLImage(OGLContextInfo *contextInfo, GLsizei imageWidth, GLsizei imageHeight, GLsizei viewportWidth, GLsizei viewportHeight)
{
	_needUploadVertices = true;
	_useDeposterize = false;
	
	_normalWidth = imageWidth;
	_normalHeight = imageHeight;
	_viewportWidth = viewportWidth;
	_viewportHeight = viewportHeight;
	
	_vf = new VideoFilter(_normalWidth, _normalHeight, VideoFilterTypeID_None, 0);
	
	_vfMasterDstBuffer = (uint32_t *)calloc(_vf->GetDstWidth() * _vf->GetDstHeight(), sizeof(uint32_t));
	_vf->SetDstBufferPtr(_vfMasterDstBuffer);
	
	_displayTexFilter = GL_NEAREST;
	glViewport(0, 0, _viewportWidth, _viewportHeight);
	
	UpdateVertices();
	UpdateTexCoords(_vf->GetDstWidth(), _vf->GetDstHeight());
	
	// Set up textures
	glGenTextures(1, &_texCPUFilterDstID);
	glGenTextures(1, &_texVideoInputDataID);
	_texVideoSourceID = _texVideoInputDataID;
	_texVideoPixelScalerID = _texVideoInputDataID;
	_texVideoOutputID = _texVideoInputDataID;
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texCPUFilterDstID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texVideoInputDataID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, _vf->GetSrcWidth(), _vf->GetSrcHeight(), 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, _vf->GetSrcBufferPtr());
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	// Set up VBOs
	glGenBuffersARB(1, &_vboVertexID);
	glGenBuffersARB(1, &_vboTexCoordID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(_vtxBuffer), _vtxBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(_texCoordBuffer), _texCoordBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	// Set up VAO
	glGenVertexArraysDESMUME(1, &_vaoMainStatesID);
	glBindVertexArrayDESMUME(_vaoMainStatesID);
	
	if (contextInfo->IsShaderSupported())
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_INT, GL_FALSE, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	else
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
		glVertexPointer(2, GL_INT, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
		glTexCoordPointer(2, GL_FLOAT, 0, 0);
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	
	glBindVertexArrayDESMUME(0);
	_isVAOPresent = true;
	
	_pixelScaler = VideoFilterTypeID_None;
	_useShader150 = contextInfo->IsUsingShader150();
	_shaderSupport = contextInfo->GetShaderSupport();
	
	_canUseShaderOutput = contextInfo->IsShaderSupported();
	if (_canUseShaderOutput)
	{
		_finalOutputProgram = new OGLShaderProgram;
		_finalOutputProgram->SetShaderSupport(_shaderSupport);
		_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, _useShader150);
		
		const GLuint finalOutputProgramID = _finalOutputProgram->GetProgramID();
		glUseProgram(finalOutputProgramID);
		_uniformFinalOutputAngleDegrees = glGetUniformLocation(finalOutputProgramID, "angleDegrees");
		_uniformFinalOutputScalar = glGetUniformLocation(finalOutputProgramID, "scalar");
		_uniformFinalOutputViewSize = glGetUniformLocation(finalOutputProgramID, "viewSize");
		glUseProgram(0);
	}
	else
	{
		_finalOutputProgram = NULL;
	}
	
	_canUseShaderBasedFilters = (contextInfo->IsShaderSupported() && contextInfo->IsFBOSupported());
	if (_canUseShaderBasedFilters)
	{
		_filterDeposterize = new OGLFilterDeposterize(_vf->GetSrcWidth(), _vf->GetSrcHeight(), _shaderSupport, _useShader150);
		
		_shaderFilter = new OGLFilter(_vf->GetSrcWidth(), _vf->GetSrcHeight(), 1);
		OGLShaderProgram *shaderFilterProgram = _shaderFilter->GetProgram();
		shaderFilterProgram->SetShaderSupport(_shaderSupport);
		shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, _useShader150);
		
		UploadHQnxLUTs();
	}
	else
	{
		_filterDeposterize = NULL;
		_shaderFilter = NULL;
	}
	
	_useShaderBasedPixelScaler = false;
	_filtersPreferGPU = true;
	_outputFilter = OutputFilterTypeID_Bilinear;
}

OGLImage::~OGLImage()
{
	if (_isVAOPresent)
	{
		glDeleteVertexArraysDESMUME(1, &this->_vaoMainStatesID);
		_isVAOPresent = false;
	}
	
	glDeleteBuffersARB(1, &this->_vboVertexID);
	glDeleteBuffersARB(1, &this->_vboTexCoordID);
	glDeleteTextures(1, &this->_texCPUFilterDstID);
	glDeleteTextures(1, &this->_texVideoInputDataID);
	
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_3D, 0);
	glDeleteTextures(1, &this->_texLQ2xLUT);
	glDeleteTextures(1, &this->_texHQ2xLUT);
	glDeleteTextures(1, &this->_texHQ3xLUT);
	glDeleteTextures(1, &this->_texHQ4xLUT);
	glActiveTexture(GL_TEXTURE0);
	
	if (_canUseShaderOutput)
	{
		glUseProgram(0);
		delete this->_finalOutputProgram;
	}
	
	if (_canUseShaderBasedFilters)
	{
		delete this->_filterDeposterize;
		delete this->_shaderFilter;
	}
	
	delete this->_vf;
	free(_vfMasterDstBuffer);
}

void OGLImage::UploadHQnxLUTs()
{
	InitHQnxLUTs();
	
	glGenTextures(1, &_texLQ2xLUT);
	glGenTextures(1, &_texHQ2xLUT);
	glGenTextures(1, &_texHQ3xLUT);
	glGenTextures(1, &_texHQ4xLUT);
	glActiveTexture(GL_TEXTURE0 + 1);
	
	glBindTexture(GL_TEXTURE_3D, _texLQ2xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 256*2, 4, 16, 0, GL_BGR, GL_UNSIGNED_BYTE, _LQ2xLUT);
	
	glBindTexture(GL_TEXTURE_3D, _texHQ2xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 256*2, 4, 16, 0, GL_BGR, GL_UNSIGNED_BYTE, _HQ2xLUT);
	
	glBindTexture(GL_TEXTURE_3D, _texHQ3xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 256*2, 9, 16, 0, GL_BGR, GL_UNSIGNED_BYTE, _HQ3xLUT);
	
	glBindTexture(GL_TEXTURE_3D, _texHQ4xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 256*2, 16, 16, 0, GL_BGR, GL_UNSIGNED_BYTE, _HQ4xLUT);
	
	glBindTexture(GL_TEXTURE_3D, 0);
	glActiveTexture(GL_TEXTURE0);
}

bool OGLImage::GetFiltersPreferGPU()
{
	return this->_filtersPreferGPU;
}

void OGLImage::SetFiltersPreferGPUOGL(bool preferGPU)
{
	this->_filtersPreferGPU = preferGPU;
	this->_useShaderBasedPixelScaler = (preferGPU) ? this->SetGPUPixelScalerOGL(this->_pixelScaler) : false;
}

bool OGLImage::GetSourceDeposterize()
{
	return this->_useDeposterize;
}

void OGLImage::SetSourceDeposterize(bool useDeposterize)
{
	this->_useDeposterize = (this->_canUseShaderBasedFilters) ? useDeposterize : false;
}

void OGLImage::UpdateVertices()
{
	const GLint w = this->_normalWidth;
	const GLint h = this->_normalHeight;
	
	_vtxBuffer[0] = -w/2;	_vtxBuffer[1] =  h/2;
	_vtxBuffer[2] =  w/2;	_vtxBuffer[3] =  h/2;
	_vtxBuffer[4] = -w/2;	_vtxBuffer[5] = -h/2;
	_vtxBuffer[6] =  w/2;	_vtxBuffer[7] = -h/2;
	
	this->_needUploadVertices = true;
}

void OGLImage::UpdateTexCoords(GLfloat s, GLfloat t)
{
	_texCoordBuffer[0] = 0.0f;	_texCoordBuffer[1] = 0.0f;
	_texCoordBuffer[2] =    s;	_texCoordBuffer[3] = 0.0f;
	_texCoordBuffer[4] = 0.0f;	_texCoordBuffer[5] =    t;
	_texCoordBuffer[6] =    s;	_texCoordBuffer[7] =    t;
}

bool OGLImage::CanUseShaderBasedFilters()
{
	return this->_canUseShaderBasedFilters;
}

void OGLImage::GetNormalSize(double &w, double &h) const
{
	w = this->_normalWidth;
	h = this->_normalHeight;
}

void OGLImage::UploadVerticesOGL()
{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(this->_vtxBuffer), this->_vtxBuffer);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	this->_needUploadVertices = false;
}

void OGLImage::UploadTexCoordsOGL()
{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(this->_texCoordBuffer), this->_texCoordBuffer);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void OGLImage::UploadTransformationOGL()
{
	const double w = this->_viewportWidth;
	const double h = this->_viewportHeight;
	const GLdouble s = ClientDisplayView::GetMaxScalarWithinBounds(this->_normalWidth, this->_normalHeight, w, h);
	
	if (this->_canUseShaderOutput)
	{
		glUniform2f(this->_uniformFinalOutputViewSize, w, h);
		glUniform1f(this->_uniformFinalOutputAngleDegrees, 0.0f);
		glUniform1f(this->_uniformFinalOutputScalar, s);
	}
	else
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-w/2.0, -w/2.0 + w, -h/2.0, -h/2.0 + h, -1.0, 1.0);
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
		glScalef(s, s, 1.0f);
	}
	
	glViewport(0, 0, this->_viewportWidth, this->_viewportHeight);
}

int OGLImage::GetOutputFilter()
{
	return this->_outputFilter;
}

void OGLImage::SetOutputFilterOGL(const int filterID)
{
	this->_displayTexFilter = GL_NEAREST;
	
	if (this->_canUseShaderBasedFilters)
	{
		this->_outputFilter = filterID;
		
		switch (filterID)
		{
			case OutputFilterTypeID_NearestNeighbor:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, _useShader150);
				break;
				
			case OutputFilterTypeID_Bilinear:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, _useShader150);
				this->_displayTexFilter = GL_LINEAR;
				break;
				
			case OutputFilterTypeID_BicubicBSpline:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterBicubicBSplineFragShader_110, _useShader150);
				break;
				
			case OutputFilterTypeID_BicubicMitchell:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterBicubicMitchellNetravaliFragShader_110, _useShader150);
				break;
				
			case OutputFilterTypeID_Lanczos2:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterLanczos2FragShader_110, _useShader150);
				break;
				
			case OutputFilterTypeID_Lanczos3:
			{
				if (this->_shaderSupport >= ShaderSupport_HighTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample6x6Output_VertShader_110, FilterLanczos3FragShader_110, _useShader150);
				}
				else if (this->_shaderSupport >= ShaderSupport_MidTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample5x5Output_VertShader_110, FilterLanczos3FragShader_110, _useShader150);
				}
				else
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterLanczos3FragShader_110, _useShader150);
				}
				break;
			}
				
			default:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, _useShader150);
				this->_outputFilter = OutputFilterTypeID_NearestNeighbor;
				break;
		}
	}
	else
	{
		if (filterID == OutputFilterTypeID_Bilinear)
		{
			this->_displayTexFilter = GL_LINEAR;
			this->_outputFilter = filterID;
		}
		else
		{
			this->_outputFilter = OutputFilterTypeID_NearestNeighbor;
		}
	}
}

int OGLImage::GetPixelScaler()
{
	return (int)this->_pixelScaler;
}

void OGLImage::SetPixelScalerOGL(const int filterID)
{
	std::string cpuTypeIDString = std::string( VideoFilter::GetTypeStringByID((VideoFilterTypeID)filterID) );
	const VideoFilterTypeID newFilterID = (cpuTypeIDString != std::string(VIDEOFILTERTYPE_UNKNOWN_STRING)) ? (VideoFilterTypeID)filterID : VideoFilterTypeID_None;
	
	this->SetCPUPixelScalerOGL(newFilterID);
	this->_useShaderBasedPixelScaler = (this->GetFiltersPreferGPU()) ? this->SetGPUPixelScalerOGL(newFilterID) : false;
	this->_pixelScaler = newFilterID;
}

bool OGLImage::SetGPUPixelScalerOGL(const VideoFilterTypeID filterID)
{
	bool willUseShaderBasedPixelScaler = true;
	
	if (!this->_canUseShaderBasedFilters || filterID == VideoFilterTypeID_None)
	{
		willUseShaderBasedPixelScaler = false;
		return willUseShaderBasedPixelScaler;
	}
	
	OGLShaderProgram *shaderFilterProgram = _shaderFilter->GetProgram();
	VideoFilterAttributes vfAttr = VideoFilter::GetAttributesByID((VideoFilterTypeID)filterID);
	GLfloat vfScale = (GLfloat)vfAttr.scaleMultiply / (GLfloat)vfAttr.scaleDivide;
	
	switch (filterID)
	{
		case VideoFilterTypeID_Nearest1_5X:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, _useShader150);
			break;
			
		case VideoFilterTypeID_Nearest2X:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, _useShader150);
			break;
			
		case VideoFilterTypeID_Scanline:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, Scalar2xScanlineFragShader_110, _useShader150);
			break;
			
		case VideoFilterTypeID_EPX:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, Scalar2xEPXFragShader_110, _useShader150);
			break;
			
		case VideoFilterTypeID_EPXPlus:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, Scalar2xEPXPlusFragShader_110, _useShader150);
			break;
			
		case VideoFilterTypeID_2xSaI:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, Scalar2xSaIFragShader_110, _useShader150);
			break;
			
		case VideoFilterTypeID_Super2xSaI:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, ScalarSuper2xSaIFragShader_110, _useShader150);
			break;
			
		case VideoFilterTypeID_SuperEagle:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, ScalarSuperEagle2xFragShader_110, _useShader150);
			break;
			
		case VideoFilterTypeID_LQ2X:
		{
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_3D, this->_texLQ2xLUT);
			glActiveTexture(GL_TEXTURE0);
			
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerLQ2xFragShader_110, _useShader150);
			
			glUseProgram(shaderFilterProgram->GetProgramID());
			GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
			glUniform1i(uniformTexSampler, 0);
			
			uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
			glUniform1i(uniformTexSampler, 1);
			glUseProgram(0);
			break;
		}
			
		case VideoFilterTypeID_LQ2XS:
		{
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_3D, this->_texLQ2xLUT);
			glActiveTexture(GL_TEXTURE0);
			
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerLQ2xSFragShader_110, _useShader150);
			
			glUseProgram(shaderFilterProgram->GetProgramID());
			GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
			glUniform1i(uniformTexSampler, 0);
			
			uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
			glUniform1i(uniformTexSampler, 1);
			glUseProgram(0);
			break;
		}
			
		case VideoFilterTypeID_HQ2X:
		{
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_3D, this->_texHQ2xLUT);
			glActiveTexture(GL_TEXTURE0);
			
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ2xFragShader_110, _useShader150);
			
			glUseProgram(shaderFilterProgram->GetProgramID());
			GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
			glUniform1i(uniformTexSampler, 0);
			
			uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
			glUniform1i(uniformTexSampler, 1);
			glUseProgram(0);
			break;
		}
			
		case VideoFilterTypeID_HQ2XS:
		{
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_3D, this->_texHQ2xLUT);
			glActiveTexture(GL_TEXTURE0);
			
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ2xSFragShader_110, _useShader150);
			
			glUseProgram(shaderFilterProgram->GetProgramID());
			GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
			glUniform1i(uniformTexSampler, 0);
			
			uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
			glUniform1i(uniformTexSampler, 1);
			glUseProgram(0);
			break;
		}
			
		case VideoFilterTypeID_HQ3X:
		{
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_3D, this->_texHQ3xLUT);
			glActiveTexture(GL_TEXTURE0);
			
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ3xFragShader_110, _useShader150);
			
			glUseProgram(shaderFilterProgram->GetProgramID());
			GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
			glUniform1i(uniformTexSampler, 0);
			
			uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
			glUniform1i(uniformTexSampler, 1);
			glUseProgram(0);
			break;
		}
			
		case VideoFilterTypeID_HQ3XS:
		{
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_3D, this->_texHQ3xLUT);
			glActiveTexture(GL_TEXTURE0);
			
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ3xSFragShader_110, _useShader150);
			
			glUseProgram(shaderFilterProgram->GetProgramID());
			GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
			glUniform1i(uniformTexSampler, 0);
			
			uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
			glUniform1i(uniformTexSampler, 1);
			glUseProgram(0);
			break;
		}
			
		case VideoFilterTypeID_HQ4X:
		{
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_3D, this->_texHQ4xLUT);
			glActiveTexture(GL_TEXTURE0);
			
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ4xFragShader_110, _useShader150);
			
			glUseProgram(shaderFilterProgram->GetProgramID());
			GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
			glUniform1i(uniformTexSampler, 0);
			
			uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
			glUniform1i(uniformTexSampler, 1);
			glUseProgram(0);
			break;
		}
			
		case VideoFilterTypeID_HQ4XS:
		{
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_3D, this->_texHQ4xLUT);
			glActiveTexture(GL_TEXTURE0);
			
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ4xSFragShader_110, _useShader150);
			
			glUseProgram(shaderFilterProgram->GetProgramID());
			GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
			glUniform1i(uniformTexSampler, 0);
			
			uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
			glUniform1i(uniformTexSampler, 1);
			glUseProgram(0);
			break;
		}
			
		case VideoFilterTypeID_2xBRZ:
		{
			if (this->_shaderSupport >= ShaderSupport_MidTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler2xBRZFragShader_110, _useShader150);
			}
			else if (this->_shaderSupport >= ShaderSupport_LowTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, Scaler2xBRZFragShader_110, _useShader150);
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		case VideoFilterTypeID_3xBRZ:
		{
			if (this->_shaderSupport >= ShaderSupport_MidTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler3xBRZFragShader_110, _useShader150);
			}
			else if (this->_shaderSupport >= ShaderSupport_LowTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, Scaler3xBRZFragShader_110, _useShader150);
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		case VideoFilterTypeID_4xBRZ:
		{
			if (this->_shaderSupport >= ShaderSupport_MidTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler4xBRZFragShader_110, _useShader150);
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
			
		case VideoFilterTypeID_5xBRZ:
		{
			if (this->_shaderSupport >= ShaderSupport_MidTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler5xBRZFragShader_110, _useShader150);
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		case VideoFilterTypeID_6xBRZ:
		{
			if (this->_shaderSupport >= ShaderSupport_MidTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler6xBRZFragShader_110, _useShader150);
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		default:
			willUseShaderBasedPixelScaler = false;
			break;
	}
	
	if (willUseShaderBasedPixelScaler)
	{
		_shaderFilter->SetScaleOGL(vfScale);
	}
	
	return willUseShaderBasedPixelScaler;
}

void OGLImage::SetCPUPixelScalerOGL(const VideoFilterTypeID filterID)
{
	bool needResizeTexture = false;
	const VideoFilterAttributes newFilterAttr = VideoFilter::GetAttributesByID(filterID);
	const GLsizei oldDstBufferWidth = this->_vf->GetDstWidth();
	const GLsizei oldDstBufferHeight = this->_vf->GetDstHeight();
	const GLsizei newDstBufferWidth = this->_vf->GetSrcWidth() * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	const GLsizei newDstBufferHeight = this->_vf->GetSrcHeight() * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	
	if (oldDstBufferWidth != newDstBufferWidth || oldDstBufferHeight != newDstBufferHeight)
	{
		needResizeTexture = true;
	}
	
	if (needResizeTexture)
	{
		uint32_t *oldMasterBuffer = _vfMasterDstBuffer;
		uint32_t *newMasterBuffer = (uint32_t *)calloc(newDstBufferWidth * newDstBufferHeight, sizeof(uint32_t));
		this->_vf->SetDstBufferPtr(newMasterBuffer);
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, newDstBufferWidth, newDstBufferHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, newMasterBuffer);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
		
		_vfMasterDstBuffer = newMasterBuffer;
		free(oldMasterBuffer);
	}
	
	this->_vf->ChangeFilterByID(filterID);
}

void OGLImage::LoadFrameOGL(const uint32_t *frameData, GLint x, GLint y, GLsizei w, GLsizei h)
{
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataID);
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, x, y, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, frameData);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	memcpy(this->_vf->GetSrcBufferPtr(), frameData, w * h * sizeof(uint32_t));
}

void OGLImage::ProcessOGL()
{
	VideoFilter *currentFilter = this->_vf;
	const bool isUsingCPUPixelScaler = (this->_pixelScaler != VideoFilterTypeID_None) && !this->_useShaderBasedPixelScaler;
	
	// Source
	if (this->_useDeposterize)
	{
		this->_texVideoSourceID = this->_filterDeposterize->RunFilterOGL(this->_texVideoInputDataID);
		
		if (isUsingCPUPixelScaler) // Hybrid CPU/GPU-based path (may cause a performance hit on pixel download)
		{
			this->_filterDeposterize->DownloadDstBufferOGL(currentFilter->GetSrcBufferPtr(), 0, this->_normalHeight);
		}
	}
	else
	{
		this->_texVideoSourceID = this->_texVideoInputDataID;
	}
	
	// Pixel scaler
	if (!isUsingCPUPixelScaler)
	{
		if (this->_useShaderBasedPixelScaler)
		{
			this->_texVideoPixelScalerID = this->_shaderFilter->RunFilterOGL(this->_texVideoSourceID);
			this->UpdateTexCoords(this->_shaderFilter->GetDstWidth(), this->_shaderFilter->GetDstHeight());
		}
		else
		{
			this->_texVideoPixelScalerID = this->_texVideoSourceID;
			this->UpdateTexCoords(this->_normalWidth, this->_normalHeight);
		}
	}
	else
	{
		uint32_t *texData = currentFilter->RunFilter();
		
		const GLfloat w = currentFilter->GetDstWidth();
		const GLfloat h = currentFilter->GetDstHeight();
		this->_texVideoPixelScalerID = this->_texCPUFilterDstID;
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoPixelScalerID);
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, texData);
		this->UpdateTexCoords(w, h);
	}
	
	// Output
	this->_texVideoOutputID = this->_texVideoPixelScalerID;
	this->UploadTexCoordsOGL();
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
}

void OGLImage::RenderOGL()
{
	glUseProgram(this->_finalOutputProgram->GetProgramID());
	this->UploadTransformationOGL();
	
	if (this->_needUploadVertices)
	{
		this->UploadVerticesOGL();
	}
	
	// Enable vertex attributes
	glBindVertexArrayDESMUME(this->_vaoMainStatesID);
	
	glClear(GL_COLOR_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoOutputID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	// Disable vertex attributes
	glBindVertexArrayDESMUME(0);
}

#pragma mark -

void OGLVideoLayer::SetNeedsUpdateVertices()
{
	this->_needUpdateVertices = true;
}

bool OGLVideoLayer::IsVisible()
{
	return this->_isVisible;
}

void OGLVideoLayer::SetVisibility(const bool visibleState)
{
	if (!this->_isVisible && visibleState)
	{
		this->_isVisible = visibleState;
		this->UpdateViewportOGL();
		this->ProcessOGL();
		
		return;
	}
	
	this->_isVisible = visibleState;
}

#pragma mark -

OGLHUDLayer::OGLHUDLayer(OGLVideoOutput *oglVO)
{
	_needUpdateVertices = true;
	_isVisible = false;
	_output = oglVO;
	
	_glyphInfo = NULL;
	_glyphSize = 0.0f;
	_glyphTileSize = 0.0f;
	
	if (_output->GetContextInfo()->IsShaderSupported())
	{
		_program = new OGLShaderProgram;
		_program->SetShaderSupport(oglVO->GetContextInfo()->GetShaderSupport());
		_program->SetVertexAndFragmentShaderOGL(HUDOutputVertShader_100, HUDOutputFragShader_110, oglVO->GetContextInfo()->IsUsingShader150());
		
		glUseProgram(_program->GetProgramID());
		_uniformViewSize = glGetUniformLocation(_program->GetProgramID(), "viewSize");
		glUseProgram(0);
	}
	else
	{
		_program = NULL;
	}
	
	glGenTextures(1, &_texCharMap);
	
	// Set up VBOs
	glGenBuffersARB(1, &_vboVertexID);
	glGenBuffersARB(1, &_vboTexCoordID);
	glGenBuffersARB(1, &_vboElementID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE, NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE, NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, _vboElementID);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(GLshort) * HUD_MAX_CHARACTERS * 6, NULL, GL_STATIC_DRAW_ARB);
	GLshort *idxBufferPtr = (GLshort *)glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		
	for (size_t i = 0; i < HUD_MAX_CHARACTERS; i++)
	{
		idxBufferPtr[(i*6)+0] = (i*4)+0;
		idxBufferPtr[(i*6)+1] = (i*4)+1;
		idxBufferPtr[(i*6)+2] = (i*4)+2;
		idxBufferPtr[(i*6)+3] = (i*4)+2;
		idxBufferPtr[(i*6)+4] = (i*4)+3;
		idxBufferPtr[(i*6)+5] = (i*4)+0;
	}
	
	glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	
	// Set up VAO
	glGenVertexArraysDESMUME(1, &_vaoMainStatesID);
	glBindVertexArrayDESMUME(_vaoMainStatesID);
	
	if (oglVO->GetContextInfo()->IsShaderSupported())
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, _vboElementID);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	else
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
		glVertexPointer(2, GL_FLOAT, 0, NULL);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
		glTexCoordPointer(2, GL_FLOAT, 0, NULL);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, _vboElementID);
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	
	glBindVertexArrayDESMUME(0);
}

OGLHUDLayer::~OGLHUDLayer()
{
	if (_output->GetContextInfo()->IsShaderSupported())
	{
		glUseProgram(0);
		delete this->_program;
	}
	
	glDeleteVertexArraysDESMUME(1, &this->_vaoMainStatesID);
	glDeleteBuffersARB(1, &this->_vboVertexID);
	glDeleteBuffersARB(1, &this->_vboTexCoordID);
	glDeleteBuffersARB(1, &this->_vboElementID);
	
	glDeleteTextures(1, &this->_texCharMap);
}

void OGLHUDLayer::CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo)
{
	FT_Error error = FT_Err_Ok;
	
	this->_glyphInfo = glyphInfo;
	this->_glyphSize = (GLfloat)glyphSize;
	this->_glyphTileSize = (GLfloat)glyphTileSize;
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, this->_texCharMap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	GLint texLevel = 0;
	for (size_t tileSize = glyphTileSize, gSize = glyphSize; tileSize >= 4; texLevel++, tileSize >>= 1, gSize = (GLfloat)tileSize * 0.75f)
	{
		const size_t charMapBufferPixCount = (16 * tileSize) * (16 * tileSize);
		
		const uint32_t fontColor = 0x00FFFFFF;
		uint32_t *charMapBuffer = (uint32_t *)malloc(charMapBufferPixCount * 2 * sizeof(uint32_t));
		for (size_t i = 0; i < charMapBufferPixCount; i++)
		{
			charMapBuffer[i] = fontColor;
		}
		
		error = FT_Set_Char_Size(fontFace, gSize << 6, gSize << 6, 72, 72);
		if (error)
		{
			printf("OGLVideoOutput: FreeType failed to set the font size!\n");
		}
		
		const FT_GlyphSlot glyphSlot = fontFace->glyph;
		
		// Fill the box with a translucent black color.
		for (size_t rowIndex = 0; rowIndex < tileSize; rowIndex++)
		{
			for (size_t pixIndex = 0; pixIndex < tileSize; pixIndex++)
			{
				const uint32_t colorRGBA8888 = 0x50000000;
				charMapBuffer[(tileSize + pixIndex) + (rowIndex * (16 * tileSize))] = colorRGBA8888;
			}
		}
		
		// Set up the glyphs.
		for (unsigned char c = 32; c < 255; c++)
		{
			error = FT_Load_Char(fontFace, c, FT_LOAD_RENDER);
			if (error)
			{
				continue;
			}
			
			const uint16_t tileOffsetX = (c & 0x0F) * tileSize;
			const uint16_t tileOffsetY = (c >> 4) * tileSize;
			const uint16_t tileOffsetY_texture = tileOffsetY - (tileSize - gSize + (gSize / 16));
			const uint16_t texSize = tileSize * 16;
			const GLuint glyphWidth = glyphSlot->bitmap.width;
			
			if (tileSize == glyphTileSize)
			{
				GlyphInfo &gi = glyphInfo[c];
				gi.width = (c != ' ') ? glyphWidth : (GLfloat)tileSize / 5.0f;
				gi.texCoord[0] = (GLfloat)tileOffsetX / (GLfloat)texSize;					gi.texCoord[1] = (GLfloat)tileOffsetY / (GLfloat)texSize;
				gi.texCoord[2] = (GLfloat)(tileOffsetX + glyphWidth) / (GLfloat)texSize;	gi.texCoord[3] = (GLfloat)tileOffsetY / (GLfloat)texSize;
				gi.texCoord[4] = (GLfloat)(tileOffsetX + glyphWidth) / (GLfloat)texSize;	gi.texCoord[5] = (GLfloat)(tileOffsetY + tileSize) / (GLfloat)texSize;
				gi.texCoord[6] = (GLfloat)tileOffsetX / (GLfloat)texSize;					gi.texCoord[7] = (GLfloat)(tileOffsetY + tileSize) / (GLfloat)texSize;
			}
			
			// Draw the glyph to the client-side buffer.
			for (size_t rowIndex = 0; rowIndex < glyphSlot->bitmap.rows; rowIndex++)
			{
				for (size_t pixIndex = 0; pixIndex < glyphWidth; pixIndex++)
				{
					const uint32_t colorRGBA8888 = fontColor | ((uint32_t)((uint8_t *)(glyphSlot->bitmap.buffer))[pixIndex + (rowIndex * glyphWidth)] << 24);
					charMapBuffer[(tileOffsetX + pixIndex) + ((tileOffsetY_texture + rowIndex + (tileSize - glyphSlot->bitmap_top)) * (16 * tileSize))] = colorRGBA8888;
				}
			}
		}
		
		glTexImage2D(GL_TEXTURE_2D, texLevel, GL_RGBA, 16 * tileSize, 16 * tileSize, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, charMapBuffer);
	}
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, texLevel - 1);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void OGLHUDLayer::_UpdateVerticesOGL()
{
	const size_t length = this->_output->GetHUDString().length();
	if (length <= 1)
	{
		this->_needUpdateVertices = false;
		return;
	}
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE, NULL, GL_STREAM_DRAW_ARB);
	float *vtxBufferPtr = (float *)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	
	this->_output->SetHUDVertices((float)this->_output->GetViewportWidth(), (float)this->_output->GetViewportHeight(), vtxBufferPtr);
	
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	this->_needUpdateVertices = false;
}

void OGLHUDLayer::UpdateViewportOGL()
{
	if (this->_output->GetContextInfo()->IsShaderSupported())
	{
		glUseProgram(this->_program->GetProgramID());
		glUniform2f(this->_uniformViewSize, this->_output->GetViewProperties().clientWidth, this->_output->GetViewProperties().clientHeight);
	}
	
	this->_needUpdateVertices = true;
};

void OGLHUDLayer::ProcessOGL()
{
	const size_t length = this->_output->GetHUDString().length();
	if (length <= 1)
	{
		return;
	}
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE, NULL, GL_STREAM_DRAW_ARB);
	float *texCoordBufferPtr = (float *)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	
	this->_output->SetHUDTextureCoordinates(texCoordBufferPtr);
	
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	this->_needUpdateVertices = true;
}

void OGLHUDLayer::RenderOGL()
{
	const size_t length = this->_output->GetHUDString().length();
	if (length <= 1)
	{
		return;
	}
	
	if (this->_output->GetContextInfo()->IsShaderSupported())
	{
		glUseProgram(this->_program->GetProgramID());
	}
	
	if (this->_needUpdateVertices)
	{
		this->_UpdateVerticesOGL();
	}
	
	glBindVertexArrayDESMUME(this->_vaoMainStatesID);
	glBindTexture(GL_TEXTURE_2D, this->_texCharMap);
	
	// First, draw the backing text box.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
	
	// Next, draw each character inside the box.
	const GLfloat textBoxScale = (GLfloat)HUD_TEXTBOX_BASE_SCALE * this->_output->GetHUDObjectScale() / this->_output->GetScaleFactor();
	if (textBoxScale >= (2.0/3.0))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0.00f);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.50f);
	}
	glDrawElements(GL_TRIANGLES, (length - 1) * 6, GL_UNSIGNED_SHORT, (GLvoid *)(6 * sizeof(GLshort)));
	
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArrayDESMUME(0);
}

#pragma mark -

OGLDisplayLayer::OGLDisplayLayer(OGLVideoOutput *oglVO)
{
	_isVisible = true;
	_output = oglVO;
	_useClientStorage = GL_FALSE;
	_needUpdateRotationScale = true;
	_needUpdateVertices = true;
	
	_vf[0] = new VideoFilter(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, VideoFilterTypeID_None, 0);
	_vf[1] = new VideoFilter(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, VideoFilterTypeID_None, 0);
	
	_vfMasterDstBuffer = (uint32_t *)calloc(_vf[0]->GetDstWidth() * (_vf[0]->GetDstHeight() + _vf[1]->GetDstHeight()), sizeof(uint32_t));
	_vfMasterDstBufferSize = _vf[0]->GetDstWidth() * (_vf[0]->GetDstHeight() + _vf[1]->GetDstHeight()) * sizeof(uint32_t);
	_vf[0]->SetDstBufferPtr(_vfMasterDstBuffer);
	_vf[1]->SetDstBufferPtr(_vfMasterDstBuffer + (_vf[0]->GetDstWidth() * _vf[0]->GetDstHeight()));
	
	_displayTexFilter[0] = GL_NEAREST;
	_displayTexFilter[1] = GL_NEAREST;
	
	_videoColorFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	_videoSrcBufferHead = NULL;
	_videoSrcNativeBuffer[0] = NULL;
	_videoSrcNativeBuffer[1] = NULL;
	_videoSrcCustomBuffer[0] = NULL;
	_videoSrcCustomBuffer[1] = NULL;
	
	// Set up textures
	glGenTextures(2, _texCPUFilterDstID);
	glGenTextures(2, _texVideoInputDataNativeID);
	glGenTextures(2, _texVideoInputDataCustomID);
	_texVideoOutputID[0] = _texVideoInputDataNativeID[0];
	_texVideoOutputID[1] = _texVideoInputDataNativeID[1];
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texCPUFilterDstID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, _vf[0]->GetDstWidth(), _vf[0]->GetDstHeight(), 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, _vf[0]->GetDstBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texCPUFilterDstID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, _vf[1]->GetDstWidth(), _vf[1]->GetDstHeight(), 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, _vf[1]->GetDstBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texVideoInputDataNativeID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, _vf[0]->GetSrcBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texVideoInputDataNativeID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, _vf[1]->GetSrcBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texVideoInputDataCustomID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, _vf[0]->GetSrcBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texVideoInputDataCustomID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, _vf[1]->GetSrcBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	// Set up VBOs
	glGenBuffersARB(1, &_vboVertexID);
	glGenBuffersARB(1, &_vboTexCoordID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLfloat) * (4 * 8), NULL, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLfloat) * (4 * 8), NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	// Set up VAO
	glGenVertexArraysDESMUME(1, &this->_vaoMainStatesID);
	glBindVertexArrayDESMUME(this->_vaoMainStatesID);
	
	if (this->_output->GetContextInfo()->IsShaderSupported())
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	else
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
		glVertexPointer(2, GL_FLOAT, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
		glTexCoordPointer(2, GL_FLOAT, 0, 0);
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	
	glBindVertexArrayDESMUME(0);
	
	_useShader150 = _output->GetContextInfo()->IsUsingShader150();
	_shaderSupport = _output->GetContextInfo()->GetShaderSupport();
	if (_output->GetContextInfo()->IsShaderSupported())
	{
		_finalOutputProgram = new OGLShaderProgram;
		_finalOutputProgram->SetShaderSupport(_shaderSupport);
		_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, _useShader150);
		
		const GLuint finalOutputProgramID = _finalOutputProgram->GetProgramID();
		glUseProgram(finalOutputProgramID);
		_uniformFinalOutputAngleDegrees = glGetUniformLocation(finalOutputProgramID, "angleDegrees");
		_uniformFinalOutputScalar = glGetUniformLocation(finalOutputProgramID, "scalar");
		_uniformFinalOutputViewSize = glGetUniformLocation(finalOutputProgramID, "viewSize");
		glUseProgram(0);
	}
	else
	{
		_finalOutputProgram = NULL;
	}
	
	if (_output->CanFilterOnGPU())
	{
		for (size_t i = 0; i < 2; i++)
		{
			_filterDeposterize[i] = new OGLFilterDeposterize(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, _shaderSupport, _useShader150);
			
			_shaderFilter[i] = new OGLFilter(_vf[i]->GetSrcWidth(), _vf[i]->GetSrcHeight(), 1);
			OGLShaderProgram *shaderFilterProgram = _shaderFilter[i]->GetProgram();
			shaderFilterProgram->SetShaderSupport(_shaderSupport);
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, _useShader150);
		}
		
		UploadHQnxLUTs();
	}
	else
	{
		_filterDeposterize[0] = NULL;
		_filterDeposterize[1] = NULL;
		_shaderFilter[0] = NULL;
		_shaderFilter[1] = NULL;
	}
}

OGLDisplayLayer::~OGLDisplayLayer()
{
	if (_output->GetContextInfo()->IsVAOSupported())
	{
		glDeleteVertexArraysDESMUME(1, &this->_vaoMainStatesID);
	}
	
	glDeleteBuffersARB(1, &this->_vboVertexID);
	glDeleteBuffersARB(1, &this->_vboTexCoordID);
	glDeleteTextures(2, this->_texCPUFilterDstID);
	glDeleteTextures(2, this->_texVideoInputDataNativeID);
	glDeleteTextures(2, this->_texVideoInputDataCustomID);
	
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_3D, 0);
	glDeleteTextures(1, &this->_texLQ2xLUT);
	glDeleteTextures(1, &this->_texHQ2xLUT);
	glDeleteTextures(1, &this->_texHQ3xLUT);
	glDeleteTextures(1, &this->_texHQ4xLUT);
	glActiveTexture(GL_TEXTURE0);
	
	if (_output->GetContextInfo()->IsShaderSupported())
	{
		glUseProgram(0);
		delete this->_finalOutputProgram;
	}
	
	if (_output->CanFilterOnGPU())
	{
		delete this->_filterDeposterize[0];
		delete this->_filterDeposterize[1];
		delete this->_shaderFilter[0];
		delete this->_shaderFilter[1];
	}
	
	delete this->_vf[0];
	delete this->_vf[1];
	free(_vfMasterDstBuffer);
	_vfMasterDstBufferSize = 0;
}

void OGLDisplayLayer::UploadHQnxLUTs()
{
	InitHQnxLUTs();
	
	glGenTextures(1, &_texLQ2xLUT);
	glGenTextures(1, &_texHQ2xLUT);
	glGenTextures(1, &_texHQ3xLUT);
	glGenTextures(1, &_texHQ4xLUT);
	glActiveTexture(GL_TEXTURE0 + 1);
	
	glBindTexture(GL_TEXTURE_3D, _texLQ2xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 256*2, 4, 16, 0, GL_BGR, GL_UNSIGNED_BYTE, _LQ2xLUT);
	
	glBindTexture(GL_TEXTURE_3D, _texHQ2xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 256*2, 4, 16, 0, GL_BGR, GL_UNSIGNED_BYTE, _HQ2xLUT);
	
	glBindTexture(GL_TEXTURE_3D, _texHQ3xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 256*2, 9, 16, 0, GL_BGR, GL_UNSIGNED_BYTE, _HQ3xLUT);
	
	glBindTexture(GL_TEXTURE_3D, _texHQ4xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 256*2, 16, 16, 0, GL_BGR, GL_UNSIGNED_BYTE, _HQ4xLUT);
	
	glBindTexture(GL_TEXTURE_3D, 0);
	glActiveTexture(GL_TEXTURE0);
}

void OGLDisplayLayer::DetermineTextureStorageHints(GLint &videoSrcTexStorageHint, GLint &cpuFilterTexStorageHint)
{
	const bool isUsingCPUPixelScaler = (this->_output->GetPixelScaler() != VideoFilterTypeID_None) && !this->_output->WillFilterOnGPU();
	videoSrcTexStorageHint = GL_STORAGE_PRIVATE_APPLE;
	cpuFilterTexStorageHint = GL_STORAGE_PRIVATE_APPLE;
	
	glFinish();
	
	if (this->_videoSrcBufferHead == NULL)
	{
		glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_ARB, 0, NULL);
		this->_useClientStorage = GL_FALSE;
	}
	else
	{
		if (isUsingCPUPixelScaler && (this->_vfMasterDstBufferSize >= this->_videoSrcBufferSize))
		{
			cpuFilterTexStorageHint = GL_STORAGE_SHARED_APPLE;
			glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_ARB, this->_vfMasterDstBufferSize, this->_vfMasterDstBuffer);
		}
		else
		{
			videoSrcTexStorageHint = GL_STORAGE_SHARED_APPLE;
			glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_ARB, this->_videoSrcBufferSize, this->_videoSrcBufferHead);
		}
		
		this->_useClientStorage = GL_TRUE;
	}
	
	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, this->_useClientStorage);
}

void OGLDisplayLayer::SetVideoBuffers(const uint32_t colorFormat,
									  const void *videoBufferHead,
									  const void *nativeBuffer0,
									  const void *nativeBuffer1,
									  const void *customBuffer0, const size_t customWidth0, const size_t customHeight0,
									  const void *customBuffer1, const size_t customWidth1, const size_t customHeight1)
{
	GLint videoSrcTexStorageHint = GL_STORAGE_PRIVATE_APPLE;
	GLint cpuFilterTexStorageHint = GL_STORAGE_PRIVATE_APPLE;
	
	const u8 bitCount = (colorFormat & 0x0000003F);
	const GLenum glColorFormat = (bitCount == 5) ? GL_UNSIGNED_SHORT_1_5_5_5_REV : GL_UNSIGNED_INT_8_8_8_8_REV;
	const size_t pixelBytes = (glColorFormat == GL_UNSIGNED_SHORT_1_5_5_5_REV) ? sizeof(uint16_t) : sizeof(uint32_t);
	
	this->_videoColorFormat = glColorFormat;
	this->_videoSrcBufferHead = videoBufferHead;
	this->_videoSrcBufferSize = (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * pixelBytes) + (customWidth0 * customHeight0 * pixelBytes) + (customWidth1 * customHeight1 * pixelBytes);
	this->_videoSrcNativeBuffer[0] = nativeBuffer0;
	this->_videoSrcNativeBuffer[1] = nativeBuffer1;
	this->_videoSrcCustomBuffer[0] = customBuffer0;
	this->_videoSrcCustomBuffer[1] = customBuffer1;
	
	this->DetermineTextureStorageHints(videoSrcTexStorageHint, cpuFilterTexStorageHint);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, cpuFilterTexStorageHint);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, _vf[0]->GetDstWidth(), _vf[0]->GetDstHeight(), 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, _vf[0]->GetDstBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, cpuFilterTexStorageHint);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, _vf[1]->GetDstWidth(), _vf[1]->GetDstHeight(), 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, _vf[1]->GetDstBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataNativeID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, this->_videoColorFormat, this->_videoSrcNativeBuffer[0]);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataNativeID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, this->_videoColorFormat, this->_videoSrcNativeBuffer[1]);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataCustomID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, customWidth0, customHeight0, 0, GL_RGBA, this->_videoColorFormat, this->_videoSrcCustomBuffer[0]);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataCustomID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, customWidth1, customHeight1, 0, GL_RGBA, this->_videoColorFormat, this->_videoSrcCustomBuffer[1]);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	glFinish();
}

void OGLDisplayLayer::SetNeedsUpdateRotationScale()
{
	this->_needUpdateRotationScale = true;
	this->_needUpdateVertices = true;
}

void OGLDisplayLayer::SetFiltersPreferGPUOGL()
{
	GLint videoSrcTexStorageHint = GL_STORAGE_PRIVATE_APPLE;
	GLint cpuFilterTexStorageHint = GL_STORAGE_PRIVATE_APPLE;
	this->DetermineTextureStorageHints(videoSrcTexStorageHint, cpuFilterTexStorageHint);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, cpuFilterTexStorageHint);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, cpuFilterTexStorageHint);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataNativeID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataNativeID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataCustomID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataCustomID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
}

void OGLDisplayLayer::_UpdateRotationScaleOGL()
{
	const ClientDisplayViewProperties &cdv = this->_output->GetViewProperties();
	
	const double r = cdv.rotation;
	const double s = cdv.viewScale;
	
	if (this->_output->GetContextInfo()->IsShaderSupported())
	{
		glUniform1f(this->_uniformFinalOutputAngleDegrees, r);
		glUniform1f(this->_uniformFinalOutputScalar, s);
	}
	else
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(r, 0.0f, 0.0f, 1.0f);
		glScalef(s, s, 1.0f);
	}
	
	this->_needUpdateRotationScale = false;
}

void OGLDisplayLayer::_UpdateVerticesOGL()
{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
	float *vtxBufferPtr = (float *)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	
	this->_output->SetScreenVertices(vtxBufferPtr);
	
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	this->_needUpdateVertices = false;
}

void OGLDisplayLayer::ResizeCPUPixelScalerOGL(const size_t srcWidthMain, const size_t srcHeightMain, const size_t srcWidthTouch, const size_t srcHeightTouch, const size_t scaleMultiply, const size_t scaleDivide)
{
	this->FinishOGL();
	
	const GLsizei newDstBufferWidth = (srcWidthMain + srcWidthTouch) * scaleMultiply / scaleDivide;
	const GLsizei newDstBufferHeight = (srcHeightMain + srcHeightTouch) * scaleMultiply / scaleDivide;
	
	uint32_t *oldMasterBuffer = _vfMasterDstBuffer;
	uint32_t *newMasterBuffer = (uint32_t *)calloc(newDstBufferWidth * newDstBufferHeight, sizeof(uint32_t));
	this->_vf[0]->SetDstBufferPtr(newMasterBuffer);
	this->_vf[1]->SetDstBufferPtr(newMasterBuffer + ((srcWidthMain * scaleMultiply / scaleDivide) * (srcHeightMain * scaleMultiply / scaleDivide)) );
	
	const GLsizei newDstBufferSingleWidth = srcWidthMain * scaleMultiply / scaleDivide;
	const GLsizei newDstBufferSingleHeight = srcHeightMain * scaleMultiply / scaleDivide;
	
	GLint videoSrcTexStorageHint = GL_STORAGE_PRIVATE_APPLE;
	GLint cpuFilterTexStorageHint = GL_STORAGE_PRIVATE_APPLE;
	this->DetermineTextureStorageHints(videoSrcTexStorageHint, cpuFilterTexStorageHint);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, cpuFilterTexStorageHint);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, newDstBufferSingleWidth, newDstBufferSingleHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, newMasterBuffer);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, cpuFilterTexStorageHint);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, srcWidthTouch * scaleMultiply / scaleDivide, srcHeightTouch * scaleMultiply / scaleDivide, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, newMasterBuffer + (newDstBufferSingleWidth * newDstBufferSingleHeight));
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataNativeID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataNativeID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataCustomID[0]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataCustomID[1]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, videoSrcTexStorageHint);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	_vfMasterDstBuffer = newMasterBuffer;
	_vfMasterDstBufferSize = newDstBufferWidth * newDstBufferHeight * sizeof(uint32_t);
	free(oldMasterBuffer);
}

OutputFilterTypeID OGLDisplayLayer::SetOutputFilterOGL(const OutputFilterTypeID filterID)
{
	OutputFilterTypeID outputFilter = OutputFilterTypeID_NearestNeighbor;
	
	this->_displayTexFilter[0] = GL_NEAREST;
	this->_displayTexFilter[1] = GL_NEAREST;
	
	if (this->_output->CanFilterOnGPU())
	{
		outputFilter = filterID;
		
		switch (filterID)
		{
			case OutputFilterTypeID_NearestNeighbor:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, _useShader150);
				break;
				
			case OutputFilterTypeID_Bilinear:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, _useShader150);
				this->_displayTexFilter[0] = GL_LINEAR;
				this->_displayTexFilter[1] = GL_LINEAR;
				break;
				
			case OutputFilterTypeID_BicubicBSpline:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterBicubicBSplineFragShader_110, _useShader150);
				break;
				
			case OutputFilterTypeID_BicubicMitchell:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterBicubicMitchellNetravaliFragShader_110, _useShader150);
				break;
				
			case OutputFilterTypeID_Lanczos2:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterLanczos2FragShader_110, _useShader150);
				break;
				
			case OutputFilterTypeID_Lanczos3:
			{
				if (this->_shaderSupport >= ShaderSupport_HighTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample6x6Output_VertShader_110, FilterLanczos3FragShader_110, _useShader150);
				}
				else if (this->_shaderSupport >= ShaderSupport_MidTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample5x5Output_VertShader_110, FilterLanczos3FragShader_110, _useShader150);
				}
				else
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterLanczos3FragShader_110, _useShader150);
				}
				break;
			}
				
			default:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, _useShader150);
				outputFilter = OutputFilterTypeID_NearestNeighbor;
				break;
		}
	}
	else
	{
		if (filterID == OutputFilterTypeID_Bilinear)
		{
			this->_displayTexFilter[0] = GL_LINEAR;
			this->_displayTexFilter[1] = GL_LINEAR;
			outputFilter = filterID;
		}
	}
	
	return outputFilter;
}

bool OGLDisplayLayer::SetGPUPixelScalerOGL(const VideoFilterTypeID filterID)
{
	bool willUseShaderBasedPixelScaler = true;
	
	if (!this->_output->CanFilterOnGPU() || filterID == VideoFilterTypeID_None)
	{
		willUseShaderBasedPixelScaler = false;
		return willUseShaderBasedPixelScaler;
	}
	
	for (size_t i = 0; i < 2; i++)
	{
		OGLShaderProgram *shaderFilterProgram = _shaderFilter[i]->GetProgram();
		VideoFilterAttributes vfAttr = VideoFilter::GetAttributesByID((VideoFilterTypeID)filterID);
		GLfloat vfScale = (GLfloat)vfAttr.scaleMultiply / (GLfloat)vfAttr.scaleDivide;
		
		switch (filterID)
		{
			case VideoFilterTypeID_Nearest1_5X:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, _useShader150);
				break;
				
			case VideoFilterTypeID_Nearest2X:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, _useShader150);
				break;
				
			case VideoFilterTypeID_Scanline:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, Scalar2xScanlineFragShader_110, _useShader150);
				break;
				
			case VideoFilterTypeID_EPX:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, Scalar2xEPXFragShader_110, _useShader150);
				break;
				
			case VideoFilterTypeID_EPXPlus:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, Scalar2xEPXPlusFragShader_110, _useShader150);
				break;
				
			case VideoFilterTypeID_2xSaI:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, Scalar2xSaIFragShader_110, _useShader150);
				break;
				
			case VideoFilterTypeID_Super2xSaI:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, ScalarSuper2xSaIFragShader_110, _useShader150);
				break;
				
			case VideoFilterTypeID_SuperEagle:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, ScalarSuperEagle2xFragShader_110, _useShader150);
				break;
				
			case VideoFilterTypeID_LQ2X:
			{
				glActiveTexture(GL_TEXTURE0 + 1);
				glBindTexture(GL_TEXTURE_3D, this->_texLQ2xLUT);
				glActiveTexture(GL_TEXTURE0);
				
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerLQ2xFragShader_110, _useShader150);
				
				glUseProgram(shaderFilterProgram->GetProgramID());
				GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
				glUniform1i(uniformTexSampler, 0);
				
				uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
				glUniform1i(uniformTexSampler, 1);
				glUseProgram(0);
				break;
			}
				
			case VideoFilterTypeID_LQ2XS:
			{
				glActiveTexture(GL_TEXTURE0 + 1);
				glBindTexture(GL_TEXTURE_3D, this->_texLQ2xLUT);
				glActiveTexture(GL_TEXTURE0);
				
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerLQ2xSFragShader_110, _useShader150);
				
				glUseProgram(shaderFilterProgram->GetProgramID());
				GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
				glUniform1i(uniformTexSampler, 0);
				
				uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
				glUniform1i(uniformTexSampler, 1);
				glUseProgram(0);
				break;
			}
				
			case VideoFilterTypeID_HQ2X:
			{
				glActiveTexture(GL_TEXTURE0 + 1);
				glBindTexture(GL_TEXTURE_3D, this->_texHQ2xLUT);
				glActiveTexture(GL_TEXTURE0);
				
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ2xFragShader_110, _useShader150);
				
				glUseProgram(shaderFilterProgram->GetProgramID());
				GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
				glUniform1i(uniformTexSampler, 0);
				
				uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
				glUniform1i(uniformTexSampler, 1);
				glUseProgram(0);
				break;
			}
				
			case VideoFilterTypeID_HQ2XS:
			{
				glActiveTexture(GL_TEXTURE0 + 1);
				glBindTexture(GL_TEXTURE_3D, this->_texHQ2xLUT);
				glActiveTexture(GL_TEXTURE0);
				
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ2xSFragShader_110, _useShader150);
				
				glUseProgram(shaderFilterProgram->GetProgramID());
				GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
				glUniform1i(uniformTexSampler, 0);
				
				uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
				glUniform1i(uniformTexSampler, 1);
				glUseProgram(0);
				break;
			}
				
			case VideoFilterTypeID_HQ3X:
			{
				glActiveTexture(GL_TEXTURE0 + 1);
				glBindTexture(GL_TEXTURE_3D, this->_texHQ3xLUT);
				glActiveTexture(GL_TEXTURE0);
				
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ3xFragShader_110, _useShader150);
				
				glUseProgram(shaderFilterProgram->GetProgramID());
				GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
				glUniform1i(uniformTexSampler, 0);
				
				uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
				glUniform1i(uniformTexSampler, 1);
				glUseProgram(0);
				break;
			}
				
			case VideoFilterTypeID_HQ3XS:
			{
				glActiveTexture(GL_TEXTURE0 + 1);
				glBindTexture(GL_TEXTURE_3D, this->_texHQ3xLUT);
				glActiveTexture(GL_TEXTURE0);
				
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ3xSFragShader_110, _useShader150);
				
				glUseProgram(shaderFilterProgram->GetProgramID());
				GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
				glUniform1i(uniformTexSampler, 0);
				
				uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
				glUniform1i(uniformTexSampler, 1);
				glUseProgram(0);
				break;
			}
				
			case VideoFilterTypeID_HQ4X:
			{
				glActiveTexture(GL_TEXTURE0 + 1);
				glBindTexture(GL_TEXTURE_3D, this->_texHQ4xLUT);
				glActiveTexture(GL_TEXTURE0);
				
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ4xFragShader_110, _useShader150);
				
				glUseProgram(shaderFilterProgram->GetProgramID());
				GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
				glUniform1i(uniformTexSampler, 0);
				
				uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
				glUniform1i(uniformTexSampler, 1);
				glUseProgram(0);
				break;
			}
				
			case VideoFilterTypeID_HQ4XS:
			{
				glActiveTexture(GL_TEXTURE0 + 1);
				glBindTexture(GL_TEXTURE_3D, this->_texHQ4xLUT);
				glActiveTexture(GL_TEXTURE0);
				
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ4xSFragShader_110, _useShader150);
				
				glUseProgram(shaderFilterProgram->GetProgramID());
				GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
				glUniform1i(uniformTexSampler, 0);
				
				uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
				glUniform1i(uniformTexSampler, 1);
				glUseProgram(0);
				break;
			}
				
			case VideoFilterTypeID_2xBRZ:
			{
				if (this->_shaderSupport >= ShaderSupport_MidTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler2xBRZFragShader_110, _useShader150);
				}
				else if (this->_shaderSupport >= ShaderSupport_LowTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, Scaler2xBRZFragShader_110, _useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_3xBRZ:
			{
				if (this->_shaderSupport >= ShaderSupport_MidTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler3xBRZFragShader_110, _useShader150);
				}
				else if (this->_shaderSupport >= ShaderSupport_LowTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, Scaler3xBRZFragShader_110, _useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_4xBRZ:
			{
				if (this->_shaderSupport >= ShaderSupport_MidTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler4xBRZFragShader_110, _useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_5xBRZ:
			{
				if (this->_shaderSupport >= ShaderSupport_MidTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler5xBRZFragShader_110, _useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_6xBRZ:
			{
				if (this->_shaderSupport >= ShaderSupport_MidTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler6xBRZFragShader_110, _useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			default:
				willUseShaderBasedPixelScaler = false;
				break;
		}
		
		if (willUseShaderBasedPixelScaler)
		{
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
			this->_shaderFilter[i]->SetScaleOGL(vfScale);
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, this->_useClientStorage);
		}
	}
	
	return willUseShaderBasedPixelScaler;
}

void OGLDisplayLayer::SetCPUPixelScalerOGL(const VideoFilterTypeID filterID)
{
	const VideoFilterAttributes newFilterAttr = VideoFilter::GetAttributesByID(filterID);
	const GLsizei oldDstBufferWidth = this->_vf[0]->GetDstWidth() + this->_vf[1]->GetDstWidth();
	const GLsizei oldDstBufferHeight = this->_vf[0]->GetDstHeight() + this->_vf[1]->GetDstHeight();
	const GLsizei newDstBufferWidth = (this->_vf[0]->GetSrcWidth() + this->_vf[1]->GetSrcWidth()) * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	const GLsizei newDstBufferHeight = (this->_vf[0]->GetSrcHeight() + this->_vf[1]->GetSrcHeight()) * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	
	if (oldDstBufferWidth != newDstBufferWidth || oldDstBufferHeight != newDstBufferHeight)
	{
		this->ResizeCPUPixelScalerOGL(this->_vf[0]->GetSrcWidth(), this->_vf[0]->GetSrcHeight(), this->_vf[1]->GetSrcWidth(), this->_vf[1]->GetSrcHeight(), newFilterAttr.scaleMultiply, newFilterAttr.scaleDivide);
	}
	
	this->_vf[0]->ChangeFilterByID(filterID);
	this->_vf[1]->ChangeFilterByID(filterID);
}

void OGLDisplayLayer::LoadNativeDisplayByID_OGL(const NDSDisplayID displayID)
{
	const bool useDeposterize = this->_output->GetSourceDeposterize();
	const bool isUsingCPUPixelScaler = (this->_output->GetPixelScaler() != VideoFilterTypeID_None) && !this->_output->WillFilterOnGPU();
	
	if (!isUsingCPUPixelScaler || useDeposterize)
	{
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataNativeID[displayID]);
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GL_RGBA, this->_videoColorFormat, this->_videoSrcNativeBuffer[displayID]);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
		glFlush();
	}
	else
	{
		if (this->_videoColorFormat == GL_UNSIGNED_SHORT_1_5_5_5_REV)
		{
			ColorspaceConvertBuffer555To8888Opaque<true, false>((const uint16_t *)this->_videoSrcNativeBuffer[displayID], this->_vf[displayID]->GetSrcBufferPtr(), GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
		else
		{
			ColorspaceConvertBuffer888XTo8888Opaque<true, false>((const uint32_t *)this->_videoSrcNativeBuffer[displayID], this->_vf[displayID]->GetSrcBufferPtr(), GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
	}
}

void OGLDisplayLayer::LoadCustomDisplayByID_OGL(const NDSDisplayID displayID)
{
	const NDSDisplayInfo &emuDisplayInfo = this->_output->GetEmuDisplayInfo();
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoInputDataCustomID[displayID]);
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, emuDisplayInfo.customWidth, emuDisplayInfo.customHeight, GL_RGBA, this->_videoColorFormat, this->_videoSrcCustomBuffer[displayID]);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glFlush();
}

void OGLDisplayLayer::UpdateViewportOGL()
{
	const ClientDisplayViewProperties &cdv = this->_output->GetViewProperties();
	
	const double w = cdv.clientWidth;
	const double h = cdv.clientHeight;
	
	if (this->_output->GetContextInfo()->IsShaderSupported())
	{
		glUseProgram(this->_finalOutputProgram->GetProgramID());
		glUniform2f(this->_uniformFinalOutputViewSize, w, h);
	}
	else
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-w/2.0, -w/2.0 + w, -h/2.0, -h/2.0 + h, -1.0, 1.0);
	}
}

void OGLDisplayLayer::_ProcessDisplayByID(const NDSDisplayID displayID, GLsizei &inoutWidth, GLsizei &inoutHeight, GLuint &inoutTexID)
{
	const bool willFilterOnGPU = this->_output->WillFilterOnGPU();
	const bool useDeposterize = this->_output->GetSourceDeposterize();
	const bool isUsingCPUPixelScaler = (this->_output->GetPixelScaler() != VideoFilterTypeID_None) && !willFilterOnGPU;
	
	// Source
	if (useDeposterize)
	{
		// For all shader-based filters, we need to temporarily disable GL_UNPACK_CLIENT_STORAGE_APPLE.
		// Filtered images are supposed to remain on the GPU for immediate use for further GPU processing,
		// so using client-backed buffers for filtered images would simply waste memory here.
		glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
		inoutTexID = this->_filterDeposterize[displayID]->RunFilterOGL(inoutTexID);
		glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, this->_useClientStorage);
		
		if (isUsingCPUPixelScaler) // Hybrid CPU/GPU-based path (may cause a performance hit on pixel download)
		{
			this->_filterDeposterize[displayID]->DownloadDstBufferOGL(this->_vf[displayID]->GetSrcBufferPtr(), 0, this->_filterDeposterize[displayID]->GetSrcHeight());
		}
	}
	
	// Pixel scaler
	if (!isUsingCPUPixelScaler)
	{
		if (willFilterOnGPU)
		{
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
			inoutTexID = this->_shaderFilter[displayID]->RunFilterOGL(inoutTexID);
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, this->_useClientStorage);
			
			inoutWidth  = this->_shaderFilter[displayID]->GetDstWidth();
			inoutHeight = this->_shaderFilter[displayID]->GetDstHeight();
		}
	}
	else
	{
		uint32_t *texData = this->_vf[displayID]->RunFilter();
		inoutTexID = this->_texCPUFilterDstID[displayID];
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, inoutTexID);
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, this->_vf[displayID]->GetDstWidth(), this->_vf[displayID]->GetDstHeight(), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, texData);
		glFlush();
		
		inoutWidth  = (GLsizei)this->_vf[displayID]->GetDstWidth();
		inoutHeight = (GLsizei)this->_vf[displayID]->GetDstHeight();
	}
}

void OGLDisplayLayer::ProcessOGL()
{
	const NDSDisplayInfo &emuDisplayInfo = this->_output->GetEmuDisplayInfo();
	const ClientDisplayMode mode = this->_output->GetViewProperties().mode;
	
	GLuint texVideoSourceID[2]	= { (!emuDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])  ? this->_texVideoInputDataNativeID[0] : this->_texVideoInputDataCustomID[0],
								    (!emuDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch]) ? this->_texVideoInputDataNativeID[1] : this->_texVideoInputDataCustomID[1] };
	GLsizei width[2]  = { emuDisplayInfo.renderedWidth[NDSDisplayID_Main],  emuDisplayInfo.renderedWidth[NDSDisplayID_Touch] };
	GLsizei height[2] = { emuDisplayInfo.renderedHeight[NDSDisplayID_Main], emuDisplayInfo.renderedHeight[NDSDisplayID_Touch] };
	
	// Run the video source filters and the pixel scalers
	if ( !emuDisplayInfo.didPerformCustomRender[NDSDisplayID_Main] && emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Main] && (mode == ClientDisplayMode_Main || mode == ClientDisplayMode_Dual) )
	{
		this->_ProcessDisplayByID(NDSDisplayID_Main, width[NDSDisplayID_Main], height[NDSDisplayID_Main], texVideoSourceID[NDSDisplayID_Main]);
	}
	
	if ( !emuDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch] && emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch] && (mode == ClientDisplayMode_Touch || mode == ClientDisplayMode_Dual) )
	{
		this->_ProcessDisplayByID(NDSDisplayID_Touch, width[NDSDisplayID_Touch], height[NDSDisplayID_Touch], texVideoSourceID[NDSDisplayID_Touch]);
	}
	
	// Set the final output texture IDs
	this->_texVideoOutputID[NDSDisplayID_Main]  = texVideoSourceID[NDSDisplayID_Main];
	this->_texVideoOutputID[NDSDisplayID_Touch] = texVideoSourceID[NDSDisplayID_Touch];
	
	// Update the texture coordinates
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, (4 * 8) * sizeof(GLfloat), NULL, GL_STREAM_DRAW_ARB);
	float *texCoordPtr = (float *)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	
	this->_output->SetScreenTextureCoordinates((float)width[NDSDisplayID_Main], (float)height[NDSDisplayID_Main], (float)width[NDSDisplayID_Touch], (float)height[NDSDisplayID_Touch], texCoordPtr);
	
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	// OpenGL shader-based filters can modify the viewport, so it needs to be reset here.
	glViewport(0, 0, this->_output->GetViewportWidth(), this->_output->GetViewportHeight());
}

void OGLDisplayLayer::RenderOGL()
{
	const NDSDisplayInfo &emuDisplayInfo = this->_output->GetEmuDisplayInfo();
	
	if (this->_output->GetContextInfo()->IsShaderSupported())
	{
		glUseProgram(this->_finalOutputProgram->GetProgramID());
	}
	
	if (this->_needUpdateRotationScale)
	{
		this->_UpdateRotationScaleOGL();
	}
	
	if (this->_needUpdateVertices)
	{
		this->_UpdateVerticesOGL();
	}
	
	// Enable vertex attributes
	glBindVertexArrayDESMUME(this->_vaoMainStatesID);
	
	switch (this->_output->GetViewProperties().mode)
	{
		case ClientDisplayMode_Main:
		{
			if (emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Main])
			{
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoOutputID[NDSDisplayID_Main]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter[NDSDisplayID_Main]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter[NDSDisplayID_Main]);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			}
			
			break;
		}
			
		case ClientDisplayMode_Touch:
		{
			if (emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch])
			{
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoOutputID[NDSDisplayID_Touch]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter[NDSDisplayID_Touch]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter[NDSDisplayID_Touch]);
				glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
			}
			break;
		}
			
		case ClientDisplayMode_Dual:
		{
			const NDSDisplayID majorDisplayID = (this->_output->GetViewProperties().order == ClientDisplayOrder_MainFirst) ? NDSDisplayID_Main : NDSDisplayID_Touch;
			const size_t majorDisplayVtx = (this->_output->GetViewProperties().order == ClientDisplayOrder_MainFirst) ? 8 : 12;
			
			switch (this->_output->GetViewProperties().layout)
			{
				case ClientDisplayLayout_Hybrid_2_1:
				case ClientDisplayLayout_Hybrid_16_9:
				case ClientDisplayLayout_Hybrid_16_10:
				{
					if (emuDisplayInfo.isDisplayEnabled[majorDisplayID])
					{
						glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoOutputID[majorDisplayID]);
						glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter[majorDisplayID]);
						glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter[majorDisplayID]);
						glDrawArrays(GL_TRIANGLE_STRIP, majorDisplayVtx, 4);
					}
					break;
				}
					
				default:
					break;
			}
			
			if (emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Main])
			{
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoOutputID[NDSDisplayID_Main]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter[NDSDisplayID_Main]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter[NDSDisplayID_Main]);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			}
			
			if (emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch])
			{
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texVideoOutputID[NDSDisplayID_Touch]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter[NDSDisplayID_Touch]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter[NDSDisplayID_Touch]);
				glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
			}
		}
			
		default:
			break;
	}
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	// Disable vertex attributes
	glBindVertexArrayDESMUME(0);
}

void OGLDisplayLayer::FinishOGL()
{
	const NDSDisplayInfo &emuDisplayInfo = this->_output->GetEmuDisplayInfo();
	const bool isUsingCPUPixelScaler = (this->_output->GetPixelScaler() != VideoFilterTypeID_None) && !this->_output->WillFilterOnGPU();
	
	glFinishObjectAPPLE(GL_TEXTURE_RECTANGLE_ARB, (isUsingCPUPixelScaler) ? this->_texCPUFilterDstID[0] : ( (!emuDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])  ? this->_texVideoInputDataNativeID[0] : this->_texVideoInputDataCustomID[0]) );
	glFinishObjectAPPLE(GL_TEXTURE_RECTANGLE_ARB, (isUsingCPUPixelScaler) ? this->_texCPUFilterDstID[1] : ( (!emuDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch]) ? this->_texVideoInputDataNativeID[1] : this->_texVideoInputDataCustomID[1]) );
}
