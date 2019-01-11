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
#include "../../common.h"
#include "../../utils/colorspacehandler/colorspacehandler.h"
#include "../../filter/videofilter.h"

#include <sstream>


// VERTEX SHADER FOR HUD OUTPUT
static const char *HUDOutputVertShader_100 = {"\
	ATTRIBUTE vec2 inPosition; \n\
	ATTRIBUTE vec4 inColor; \n\
	ATTRIBUTE vec2 inTexCoord0; \n\
	\n\
	uniform vec2 viewSize; \n\
	uniform float scalar; \n\
	uniform float angleDegrees; \n\
	uniform bool renderFlipped; \n\
	\n\
	VARYING vec4 vtxColor; \n\
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
		vtxColor = inColor; \n\
		texCoord[0] = inTexCoord0; \n\
		gl_Position = vec4(projection * rotation * scale * inPosition, 0.0, 1.0);\n\
		\n\
		if (renderFlipped)\n\
		{\n\
			gl_Position.y *= -1.0;\n\
		}\n\
	} \n\
"};

// FRAGMENT SHADER FOR HUD OUTPUT
static const char *HUDOutputFragShader_110 = {"\
	VARYING vec4 vtxColor;\n\
	VARYING vec2 texCoord[1];\n\
	uniform sampler2D tex;\n\
	\n\
	void main()\n\
	{\n\
		OUT_FRAG_COLOR = SAMPLE4_TEX_2D(tex, texCoord[0]) * vtxColor;\n\
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
	uniform bool renderFlipped; \n\
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
		\n\
		if (renderFlipped)\n\
		{\n\
			gl_Position.y *= -1.0;\n\
		}\n\
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
	uniform bool renderFlipped; \n\
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
		\n\
		if (renderFlipped)\n\
		{\n\
			gl_Position.y *= -1.0;\n\
		}\n\
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
	uniform bool renderFlipped; \n\
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
		\n\
		if (renderFlipped)\n\
		{\n\
			gl_Position.y *= -1.0;\n\
		}\n\
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
	uniform bool renderFlipped; \n\
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
		\n\
		if (renderFlipped)\n\
		{\n\
			gl_Position.y *= -1.0;\n\
		}\n\
	}\n\
"};

// FRAGMENT SHADER FOR DISPLAY OUTPUT
static const char *PassthroughOutputFragShader_110 = {"\
	VARYING vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	uniform float backlightIntensity;\n\
	\n\
	void main()\n\
	{\n\
		OUT_FRAG_COLOR.rgb = SAMPLE3_TEX_RECT(tex, texCoord[0]) * backlightIntensity;\n\
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
	uniform float backlightIntensity;\n\
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
		OUT_FRAG_COLOR.rgb *= backlightIntensity;\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *FilterBicubicBSplineFastFragShader_110 = {"\
	VARYING vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	uniform float backlightIntensity;\n\
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
		OUT_FRAG_COLOR.rgb *= backlightIntensity;\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *FilterBicubicMitchellNetravaliFragShader_110 = {"\
	VARYING vec2 texCoord[16];\n\
	uniform sampler2DRect tex;\n\
	uniform float backlightIntensity;\n\
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
		OUT_FRAG_COLOR.rgb *= backlightIntensity;\n\
		OUT_FRAG_COLOR.a = 1.0;\n\
	}\n\
"};

static const char *FilterBicubicMitchellNetravaliFastFragShader_110 = {"\
	VARYING vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	uniform float backlightIntensity;\n\
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
		OUT_FRAG_COLOR.rgb *= backlightIntensity;\n\
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
	uniform float backlightIntensity;\n\
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
		OUT_FRAG_COLOR.rgb *= backlightIntensity;\n\
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
	uniform float backlightIntensity;\n\
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
		OUT_FRAG_COLOR.rgb *= backlightIntensity;\n\
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
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/4.0, 0.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/4.0, 0.0));\n\
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
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/4.0, 0.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/4.0, 0.0));\n\
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
	// Output Pixel Mapping:  00|01|02\n\
	//                        03|04|05\n\
	//                        06|07|08\n\
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
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/9.0, 0.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/9.0, 0.0));\n\
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
		vec3 p = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+0.0)+0.5)/512.0, (k+0.5)/16.0, 0.0));\n\
		vec3 w = SAMPLE3_TEX_3D(lut, vec3(((pattern*2.0+1.0)+0.5)/512.0, (k+0.5)/16.0, 0.0));\n\
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
	// Output Pixel Mapping:     00|01\n\
	//                           02|03\n\
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
			int i = int( dot(floor(fract(texCoord[0]) * 2.05), vec2(1.05, 2.05)) );\n\
			\n\
			if (i == 0)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveSteepLine[1]) ? 0.25 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? ((haveShallowLine[2]) ? ((haveSteepLine[2]) ? 5.0/6.0 : 0.75) : ((haveSteepLine[2]) ? 0.75 : 0.50)) : 1.0 - (M_PI/4.0)) : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.25 : 0.00);\n\
			}\n\
			else if (i == 1)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? ((haveShallowLine[1]) ? ((haveSteepLine[1]) ? 5.0/6.0 : 0.75) : ((haveSteepLine[1]) ? 0.75 : 0.50)) : 1.0 - (M_PI/4.0)) : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.25 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.25 : 0.00);\n\
			}\n\
			else if (i == 2)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.25 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.25 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? ((haveShallowLine[3]) ? ((haveSteepLine[3]) ? 5.0/6.0 : 0.75) : ((haveSteepLine[3]) ? 0.75 : 0.50)) : 1.0 - (M_PI/4.0)) : 0.00);\n\
			}\n\
			else if (i == 3)\n\
			{\n\
				newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? ((haveShallowLine[0]) ? ((haveSteepLine[0]) ? 5.0/6.0 : 0.75) : ((haveSteepLine[0]) ? 0.75 : 0.50)) : 1.0 - (M_PI/4.0)) : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.25 : 0.00);\n\
				newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.25 : 0.00);\n\
			}\n\
		}\n\
		\n\
		OUT_FRAG_COLOR = vec4(newFragColor, 1.0);\n\
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
	// Output Pixel Mapping:    00|01|02\n\
	//                          03|04|05\n\
	//                          06|07|08\n\
	\n\
	void main()\n\
	{\n\
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
		\n\
		vec3 newFragColor = src[0];\n\
		int i = int( dot(floor(fract(texCoord[0]) * 3.05), vec2(1.05, 3.05)) );\n\
		\n\
		if (i != 4)\n\
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
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2]) ? ((doLineBlend[2]) ? ((!haveShallowLine[2] && !haveSteepLine[2]) ? 0.875 : 1.000) : 0.4545939598) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveShallowLine[3]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 1)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((haveSteepLine[1]) ? 0.750 : ((haveShallowLine[1]) ? 0.250 : 0.125)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((haveShallowLine[2]) ? 0.750 : ((haveSteepLine[2]) ? 0.250 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 2)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveSteepLine[0]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1]) ? ((doLineBlend[1]) ? ((!haveShallowLine[1] && !haveSteepLine[1]) ? 0.875 : 1.000) : 0.4545939598) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveShallowLine[2]) ? 0.250 : 0.000);\n\
				}\n\
				else if (i == 3)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2]) ? ((haveSteepLine[2]) ? 0.750 : ((haveShallowLine[2]) ? 0.250 : 0.125)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((haveShallowLine[3]) ? 0.750 : ((haveSteepLine[3]) ? 0.250 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 5)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((haveSteepLine[0]) ? 0.750 : ((haveShallowLine[0]) ? 0.250 : 0.125)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1]) ? ((haveShallowLine[1]) ? 0.750 : ((haveSteepLine[1]) ? 0.250 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 6)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0] && haveShallowLine[0]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[2], (needBlend[2] && doLineBlend[2] && haveSteepLine[2]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3]) ? ((doLineBlend[3]) ? ((!haveShallowLine[3] && !haveSteepLine[3]) ? 0.875 : 1.000) : 0.4545939598) : 0.000);\n\
				}\n\
				else if (i == 7)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0] && doLineBlend[0]) ? ((haveShallowLine[0]) ? 0.750 : ((haveSteepLine[0]) ? 0.250 : 0.125)) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3]) ? ((haveSteepLine[3]) ? 0.750 : ((haveShallowLine[3]) ? 0.250 : 0.125)) : 0.000);\n\
				}\n\
				else if (i == 8)\n\
				{\n\
					newFragColor = mix(newFragColor, blendPix[0], (needBlend[0]) ? ((doLineBlend[0]) ? ((!haveShallowLine[0] && !haveSteepLine[0]) ? 0.875 : 1.000) : 0.4545939598) : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[1], (needBlend[1] && doLineBlend[1] && haveShallowLine[1]) ? 0.250 : 0.000);\n\
					newFragColor = mix(newFragColor, blendPix[3], (needBlend[3] && doLineBlend[3] && haveSteepLine[3]) ? 0.250 : 0.000);\n\
				}\n\
			}\n\
		}\n\
		\n\
		OUT_FRAG_COLOR = vec4(newFragColor, 1.0);\n\
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
	OGLVertexAttributeID_Position	= 0,
	OGLVertexAttributeID_TexCoord0	= 8,
	OGLVertexAttributeID_Color		= 3
};

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

void SetupHQnxLUTs_OGL(GLenum textureIndex, GLuint &texLQ2xLUT, GLuint &texHQ2xLUT, GLuint &texHQ3xLUT, GLuint &texHQ4xLUT)
{
	InitHQnxLUTs();
	
	glGenTextures(1, &texLQ2xLUT);
	glGenTextures(1, &texHQ2xLUT);
	glGenTextures(1, &texHQ3xLUT);
	glGenTextures(1, &texHQ4xLUT);
	glActiveTexture(textureIndex);
	
	glBindTexture(GL_TEXTURE_3D, texLQ2xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 256*2, 4, 16, 0, GL_BGRA, GL_UNSIGNED_BYTE, _LQ2xLUT);
	
	glBindTexture(GL_TEXTURE_3D, texHQ2xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 256*2, 4, 16, 0, GL_BGRA, GL_UNSIGNED_BYTE, _HQ2xLUT);
	
	glBindTexture(GL_TEXTURE_3D, texHQ3xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 256*2, 9, 16, 0, GL_BGRA, GL_UNSIGNED_BYTE, _HQ3xLUT);
	
	glBindTexture(GL_TEXTURE_3D, texHQ4xLUT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 256*2, 16, 16, 0, GL_BGRA, GL_UNSIGNED_BYTE, _HQ4xLUT);
	
	glBindTexture(GL_TEXTURE_3D, 0);
	glActiveTexture(GL_TEXTURE0);
}

void DeleteHQnxLUTs_OGL(GLenum textureIndex, GLuint &texLQ2xLUT, GLuint &texHQ2xLUT, GLuint &texHQ3xLUT, GLuint &texHQ4xLUT)
{
	glActiveTexture(textureIndex);
	glBindTexture(GL_TEXTURE_3D, 0);
	glDeleteTextures(1, &texLQ2xLUT);
	glDeleteTextures(1, &texHQ2xLUT);
	glDeleteTextures(1, &texHQ3xLUT);
	glDeleteTextures(1, &texHQ4xLUT);
	glActiveTexture(GL_TEXTURE0);
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
					if (maxVaryingFloats < 60)
					{
						_shaderSupport = ShaderSupport_BottomTier;
					}
					else if (maxVaryingFloats < 124)
					{
						_shaderSupport = ShaderSupport_LowTier;
					}
					else if (maxVaryingFloats >= 124)
					{
						_shaderSupport = ShaderSupport_MidTier;
					}
				}
				else
				{
					if (maxVaryingFloats < 41)
					{
						_shaderSupport = ShaderSupport_BottomTier;
					}
					else if (maxVaryingFloats < 60)
					{
						_shaderSupport = ShaderSupport_LowTier;
					}
					else if (maxVaryingFloats < 124)
					{
						_shaderSupport = ShaderSupport_MidTier;
					}
					else if (maxVaryingFloats >= 124)
					{
						_shaderSupport = ShaderSupport_HighTier;
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
		
		if (logBuf != NULL)
		{
			GLsizei logSize = 0;
			glGetShaderInfoLog(shaderID, logBytes * sizeof(GLchar), &logSize, logBuf);
			
			printf("OpenGL Error - Failed to compile %s.\n%s\n",
				   (shaderType == GL_VERTEX_SHADER) ? "GL_VERTEX_SHADER" : ((shaderType == GL_FRAGMENT_SHADER) ? "GL_FRAGMENT_SHADER" : "OTHER SHADER TYPE"),
				   (char *)logBuf);
		}
		
		free(logBuf);
		
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

void OGLShaderProgram::SetVertexShaderOGL(const char *shaderProgram, bool useVtxColors, bool useShader150)
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
		
		if (useVtxColors)
		{
			glBindAttribLocation(this->_programID, OGLVertexAttributeID_Color, "inColor");
		}
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

void OGLShaderProgram::SetVertexAndFragmentShaderOGL(const char *vertShaderProgram, const char *fragShaderProgram, bool useVtxColors, bool useShader150)
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
		
		if (useVtxColors)
		{
			glBindAttribLocation(this->_programID, OGLVertexAttributeID_Color, "inColor");
		}
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

OGLClientFetchObject::OGLClientFetchObject()
{
	_contextInfo = NULL;
	_useDirectToCPUFilterPipeline = true;
	_fetchColorFormatOGL = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	
	pthread_rwlock_init(&_texFetchRWLock[NDSDisplayID_Main],  NULL);
	pthread_rwlock_init(&_texFetchRWLock[NDSDisplayID_Touch], NULL);
	
	_srcNativeCloneMaster = (uint32_t *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * OPENGL_FETCH_BUFFER_COUNT * sizeof(uint32_t));
	memset(_srcNativeCloneMaster, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * OPENGL_FETCH_BUFFER_COUNT * sizeof(uint32_t));
	
	for (size_t i = 0; i < OPENGL_FETCH_BUFFER_COUNT; i++)
	{
		pthread_rwlock_init(&_srcCloneRWLock[NDSDisplayID_Main][i],  NULL);
		pthread_rwlock_init(&_srcCloneRWLock[NDSDisplayID_Touch][i], NULL);
		
		_srcCloneNeedsUpdate[NDSDisplayID_Main][i]  = true;
		_srcCloneNeedsUpdate[NDSDisplayID_Touch][i] = true;
		
		_srcNativeClone[NDSDisplayID_Main][i]  = _srcNativeCloneMaster + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * ((i * 2) + 0));
		_srcNativeClone[NDSDisplayID_Touch][i] = _srcNativeCloneMaster + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * ((i * 2) + 1));
	}
}

OGLClientFetchObject::~OGLClientFetchObject()
{
	if (this->_contextInfo->IsShaderSupported())
	{
		DeleteHQnxLUTs_OGL(GL_TEXTURE0 + 1, this->_texLQ2xLUT, this->_texHQ2xLUT, this->_texHQ3xLUT, this->_texHQ4xLUT);
	}
	
	glDeleteTextures(4, &this->_texDisplayFetchNative[0][0]);
	glDeleteTextures(4, &this->_texDisplayFetchCustom[0][0]);
	
	for (size_t i = 0; i < OPENGL_FETCH_BUFFER_COUNT; i++)
	{
		pthread_rwlock_wrlock(&this->_srcCloneRWLock[NDSDisplayID_Main][i]);
		pthread_rwlock_wrlock(&this->_srcCloneRWLock[NDSDisplayID_Touch][i]);
		this->_srcNativeClone[NDSDisplayID_Main][i]  = NULL;
		this->_srcNativeClone[NDSDisplayID_Touch][i] = NULL;
	}
	
	free_aligned(this->_srcNativeCloneMaster);
	this->_srcNativeCloneMaster = NULL;
	
	for (size_t i = OPENGL_FETCH_BUFFER_COUNT - 1; i < OPENGL_FETCH_BUFFER_COUNT; i--)
	{
		pthread_rwlock_unlock(&this->_srcCloneRWLock[NDSDisplayID_Touch][i]);
		pthread_rwlock_unlock(&this->_srcCloneRWLock[NDSDisplayID_Main][i]);
		pthread_rwlock_destroy(&this->_srcCloneRWLock[NDSDisplayID_Touch][i]);
		pthread_rwlock_destroy(&this->_srcCloneRWLock[NDSDisplayID_Main][i]);
	}
	
	pthread_rwlock_destroy(&this->_texFetchRWLock[NDSDisplayID_Main]);
	pthread_rwlock_destroy(&this->_texFetchRWLock[NDSDisplayID_Touch]);
}

OGLContextInfo* OGLClientFetchObject::GetContextInfo() const
{
	return this->_contextInfo;
}

uint32_t* OGLClientFetchObject::GetSrcClone(const NDSDisplayID displayID, const u8 bufferIndex) const
{
	return this->_srcNativeClone[displayID][bufferIndex];
}

GLuint OGLClientFetchObject::GetTexNative(const NDSDisplayID displayID, const u8 bufferIndex) const
{
	return this->_texDisplayFetchNative[displayID][bufferIndex];
}

GLuint OGLClientFetchObject::GetTexCustom(const NDSDisplayID displayID, const u8 bufferIndex) const
{
	return this->_texDisplayFetchCustom[displayID][bufferIndex];
}

GLuint OGLClientFetchObject::GetTexLQ2xLUT() const
{
	return this->_texLQ2xLUT;
}

GLuint OGLClientFetchObject::GetTexHQ2xLUT() const
{
	return this->_texHQ2xLUT;
}

GLuint OGLClientFetchObject::GetTexHQ3xLUT() const
{
	return this->_texHQ3xLUT;
}

GLuint OGLClientFetchObject::GetTexHQ4xLUT() const
{
	return this->_texHQ4xLUT;
}

void OGLClientFetchObject::CopyFromSrcClone(uint32_t *dstBufferPtr, const NDSDisplayID displayID, const u8 bufferIndex)
{
	pthread_rwlock_rdlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
	memcpy(dstBufferPtr, this->_srcNativeClone[displayID][bufferIndex], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(uint32_t));
	pthread_rwlock_unlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
}

void OGLClientFetchObject::FetchNativeDisplayToSrcClone(const NDSDisplayID displayID, const u8 bufferIndex, bool needsLock)
{
	if (needsLock)
	{
		pthread_rwlock_wrlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
	}
	
	if (!this->_srcCloneNeedsUpdate[displayID][bufferIndex])
	{
		if (needsLock)
		{
			pthread_rwlock_unlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
		}
		return;
	}
	
	if (this->_fetchColorFormatOGL == GL_UNSIGNED_SHORT_1_5_5_5_REV)
	{
		ColorspaceConvertBuffer555To8888Opaque<false, false>((const uint16_t *)this->_fetchDisplayInfo[bufferIndex].nativeBuffer[displayID], this->_srcNativeClone[displayID][bufferIndex], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	}
	else
	{
		ColorspaceConvertBuffer888XTo8888Opaque<false, false>((const uint32_t *)this->_fetchDisplayInfo[bufferIndex].nativeBuffer[displayID], this->_srcNativeClone[displayID][bufferIndex], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	}
	
	this->_srcCloneNeedsUpdate[displayID][bufferIndex] = false;
	
	if (needsLock)
	{
		pthread_rwlock_unlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
	}
}

void OGLClientFetchObject::FetchTextureWriteLock(const NDSDisplayID displayID)
{
	pthread_rwlock_wrlock(&this->_texFetchRWLock[displayID]);
}

void OGLClientFetchObject::FetchTextureReadLock(const NDSDisplayID displayID)
{
	pthread_rwlock_rdlock(&this->_texFetchRWLock[displayID]);
}

void OGLClientFetchObject::FetchTextureUnlock(const NDSDisplayID displayID)
{
	pthread_rwlock_unlock(&this->_texFetchRWLock[displayID]);
}

void OGLClientFetchObject::Init()
{
	glGenTextures(2 * OPENGL_FETCH_BUFFER_COUNT, &this->_texDisplayFetchNative[0][0]);
	glGenTextures(2 * OPENGL_FETCH_BUFFER_COUNT, &this->_texDisplayFetchCustom[0][0]);
	
	for (size_t i = 0; i < OPENGL_FETCH_BUFFER_COUNT; i++)
	{
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDisplayFetchNative[NDSDisplayID_Main][i]);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, this->_fetchColorFormatOGL, this->_srcNativeClone[NDSDisplayID_Main][i]);
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDisplayFetchNative[NDSDisplayID_Touch][i]);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, this->_fetchColorFormatOGL, this->_srcNativeClone[NDSDisplayID_Touch][i]);
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDisplayFetchCustom[NDSDisplayID_Main][i]);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, this->_fetchColorFormatOGL, this->_srcNativeClone[NDSDisplayID_Main][i]);
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDisplayFetchCustom[NDSDisplayID_Touch][i]);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, this->_fetchColorFormatOGL, this->_srcNativeClone[NDSDisplayID_Touch][i]);
		
		this->_srcCloneNeedsUpdate[NDSDisplayID_Main][i]  = true;
		this->_srcCloneNeedsUpdate[NDSDisplayID_Touch][i] = true;
	}
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	this->_texFetch[NDSDisplayID_Main]  = this->_texDisplayFetchNative[NDSDisplayID_Main][0];
	this->_texFetch[NDSDisplayID_Touch] = this->_texDisplayFetchNative[NDSDisplayID_Touch][0];
	
	if (this->_contextInfo->IsShaderSupported())
	{
		SetupHQnxLUTs_OGL(GL_TEXTURE0 + 1, this->_texLQ2xLUT, this->_texHQ2xLUT, this->_texHQ3xLUT, this->_texHQ4xLUT);
	}
}

void OGLClientFetchObject::SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo)
{
	this->GPUClientFetchObject::SetFetchBuffers(currentDisplayInfo);
	
#ifdef MSB_FIRST
	this->_fetchColorFormatOGL = (currentDisplayInfo.pixelBytes == 2) ? GL_UNSIGNED_SHORT_1_5_5_5_REV : GL_UNSIGNED_INT_8_8_8_8;
#else
	this->_fetchColorFormatOGL = (currentDisplayInfo.pixelBytes == 2) ? GL_UNSIGNED_SHORT_1_5_5_5_REV : GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
	
	glFinish();
	
	glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_ARB, currentDisplayInfo.framebufferPageSize * currentDisplayInfo.framebufferPageCount, currentDisplayInfo.masterFramebufferHead);
	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
	
	for (size_t i = 0; i < currentDisplayInfo.framebufferPageCount; i++)
	{
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDisplayFetchNative[NDSDisplayID_Main][i]);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, this->_fetchColorFormatOGL, this->_fetchDisplayInfo[i].nativeBuffer[NDSDisplayID_Main]);
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDisplayFetchNative[NDSDisplayID_Touch][i]);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, this->_fetchColorFormatOGL, this->_fetchDisplayInfo[i].nativeBuffer[NDSDisplayID_Touch]);
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDisplayFetchCustom[NDSDisplayID_Main][i]);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, currentDisplayInfo.customWidth, currentDisplayInfo.customHeight, 0, GL_RGBA, this->_fetchColorFormatOGL, this->_fetchDisplayInfo[i].customBuffer[NDSDisplayID_Main]);
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDisplayFetchCustom[NDSDisplayID_Touch][i]);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, currentDisplayInfo.customWidth, currentDisplayInfo.customHeight, 0, GL_RGBA, this->_fetchColorFormatOGL, this->_fetchDisplayInfo[i].customBuffer[NDSDisplayID_Touch]);
		
		pthread_rwlock_wrlock(&this->_srcCloneRWLock[NDSDisplayID_Main][i]);
		this->_srcCloneNeedsUpdate[NDSDisplayID_Main][i] = true;
		pthread_rwlock_unlock(&this->_srcCloneRWLock[NDSDisplayID_Main][i]);
		
		pthread_rwlock_wrlock(&this->_srcCloneRWLock[NDSDisplayID_Touch][i]);
		this->_srcCloneNeedsUpdate[NDSDisplayID_Touch][i] = true;
		pthread_rwlock_unlock(&this->_srcCloneRWLock[NDSDisplayID_Touch][i]);
	}
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	glFinish();
}

void OGLClientFetchObject::FetchFromBufferIndex(const u8 index)
{
	GPUClientFetchObject::FetchFromBufferIndex(index);
	glFlush();
	
	GLuint texFetchMain = 0;
	GLuint texFetchTouch = 0;
	const NDSDisplayInfo &currentDisplayInfo = this->GetFetchDisplayInfoForBufferIndex(index);
	const bool isMainEnabled  = currentDisplayInfo.isDisplayEnabled[NDSDisplayID_Main];
	const bool isTouchEnabled = currentDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch];
	
	if (isMainEnabled)
	{
		if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])
		{
			texFetchMain = this->_texDisplayFetchNative[NDSDisplayID_Main][index];
		}
		else
		{
			texFetchMain = this->_texDisplayFetchCustom[NDSDisplayID_Main][index];
		}
	}
	
	if (isTouchEnabled)
	{
		if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
		{
			texFetchTouch = this->_texDisplayFetchNative[NDSDisplayID_Touch][index];
		}
		else
		{
			texFetchTouch = this->_texDisplayFetchCustom[NDSDisplayID_Touch][index];
		}
	}
	
	this->SetFetchTexture(NDSDisplayID_Main,  texFetchMain);
	this->SetFetchTexture(NDSDisplayID_Touch, texFetchTouch);
}

void OGLClientFetchObject::_FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	pthread_rwlock_wrlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
	this->_srcCloneNeedsUpdate[displayID][bufferIndex] = true;
	
	if (this->_useDirectToCPUFilterPipeline)
	{
		this->FetchNativeDisplayToSrcClone(displayID, bufferIndex, false);
	}
	
	pthread_rwlock_unlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDisplayFetchNative[displayID][bufferIndex]);
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GL_RGBA, this->_fetchColorFormatOGL, this->_fetchDisplayInfo[bufferIndex].nativeBuffer[displayID]);
}

void OGLClientFetchObject::_FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDisplayFetchCustom[displayID][bufferIndex]);
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, this->_fetchDisplayInfo[bufferIndex].customWidth, this->_fetchDisplayInfo[bufferIndex].customHeight, GL_RGBA, this->_fetchColorFormatOGL, this->_fetchDisplayInfo[bufferIndex].customBuffer[displayID]);
}

GLuint OGLClientFetchObject::GetFetchTexture(const NDSDisplayID displayID)
{
	return this->_texFetch[displayID];
}

void OGLClientFetchObject::SetFetchTexture(const NDSDisplayID displayID, GLuint texID)
{
	this->_texFetch[displayID] = texID;
}

#pragma mark -

OGLVideoOutput::OGLVideoOutput()
{
	_contextInfo = NULL;
	_viewportWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_viewportHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2;
	_needUpdateViewport = true;
	_hasOGLPixelScaler = false;
	
	_texCPUFilterDstID[NDSDisplayID_Main] = 0;
	_texCPUFilterDstID[NDSDisplayID_Touch] = 0;
	_fboFrameCopyID = 0;
	
	_processedFrameInfo.bufferIndex = 0;
	_processedFrameInfo.texID[NDSDisplayID_Main]  = 0;
	_processedFrameInfo.texID[NDSDisplayID_Touch] = 0;
	_processedFrameInfo.isMainDisplayProcessed = false;
	_processedFrameInfo.isTouchDisplayProcessed = false;
	
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
	
	glDeleteFramebuffersEXT(1, &this->_fboFrameCopyID);
	glDeleteTextures(2, this->_texCPUFilterDstID);
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
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		OGLVideoLayer *theLayer = (*_layerList)[i];
		theLayer->SetNeedsUpdateRotationScale();
	}
}

void OGLVideoOutput::_UpdateClientSize()
{
	this->_viewportWidth  = (GLsizei)(this->_renderProperty.clientWidth  + 0.0001);
	this->_viewportHeight = (GLsizei)(this->_renderProperty.clientHeight + 0.0001);
	this->_needUpdateViewport = true;
	
	this->GetHUDLayer()->SetNeedsUpdateVertices();
	this->ClientDisplay3DPresenter::_UpdateClientSize();
}

void OGLVideoOutput::_UpdateViewScale()
{
	this->ClientDisplay3DPresenter::_UpdateViewScale();
	
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		OGLVideoLayer *theLayer = (*_layerList)[i];
		theLayer->SetNeedsUpdateRotationScale();
	}
}

void OGLVideoOutput::_UpdateViewport()
{
	glViewport(0, 0, this->_viewportWidth, this->_viewportHeight);
	
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		OGLVideoLayer *theLayer = (*_layerList)[i];
		theLayer->SetNeedsUpdateViewport();
	}
}

void OGLVideoOutput::_LoadNativeDisplayByID(const NDSDisplayID displayID)
{
	this->GetDisplayLayer()->LoadNativeDisplayByID_OGL(displayID);
}

void OGLVideoOutput::_ResizeCPUPixelScaler(const VideoFilterTypeID filterID)
{
	const VideoFilterAttributes newFilterAttr = VideoFilter::GetAttributesByID(filterID);
	
	glFinish();
	
	this->ClientDisplay3DPresenter::_ResizeCPUPixelScaler(filterID);
	
	glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_ARB, this->_vfMasterDstBufferSize, this->_vfMasterDstBuffer);
	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID[NDSDisplayID_Main]);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, this->_vf[NDSDisplayID_Main]->GetSrcWidth()  * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide, this->_vf[NDSDisplayID_Main]->GetSrcWidth()  * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, this->_vf[NDSDisplayID_Main]->GetDstBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID[NDSDisplayID_Touch]);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, this->_vf[NDSDisplayID_Touch]->GetSrcWidth() * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide, this->_vf[NDSDisplayID_Touch]->GetSrcWidth() * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, this->_vf[NDSDisplayID_Touch]->GetDstBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	glFinish();
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
	
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
	
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
	
	// Set up textures
	glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_ARB, this->_vfMasterDstBufferSize, this->_vfMasterDstBuffer);
	
	glGenTextures(2, this->_texCPUFilterDstID);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID[NDSDisplayID_Main]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, this->_vf[NDSDisplayID_Main]->GetDstWidth(),  this->_vf[NDSDisplayID_Main]->GetDstHeight(),  0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, this->_vf[NDSDisplayID_Main]->GetDstBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID[NDSDisplayID_Touch]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, this->_vf[NDSDisplayID_Touch]->GetDstWidth(), this->_vf[NDSDisplayID_Touch]->GetDstHeight(), 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, this->_vf[NDSDisplayID_Touch]->GetDstBufferPtr());
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	glGenFramebuffersEXT(1, &this->_fboFrameCopyID);
}

void OGLVideoOutput::SetOutputFilter(const OutputFilterTypeID filterID)
{
	this->_outputFilter = this->GetDisplayLayer()->SetOutputFilterOGL(filterID);
	this->GetDisplayLayer()->SetNeedsUpdateRotationScale();
	this->_needUpdateViewport = true;
}

void OGLVideoOutput::SetPixelScaler(const VideoFilterTypeID filterID)
{
	this->ClientDisplay3DPresenter::SetPixelScaler(filterID);
	
	this->_hasOGLPixelScaler = this->GetDisplayLayer()->SetGPUPixelScalerOGL(this->_pixelScaler);
	this->_willFilterOnGPU = (this->GetFiltersPreferGPU()) ? this->_hasOGLPixelScaler : false;
}

void OGLVideoOutput::CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo)
{
	this->GetHUDLayer()->CopyHUDFont(fontFace, glyphSize, glyphTileSize, glyphInfo);
}

void OGLVideoOutput::SetHUDVisibility(const bool visibleState)
{
	this->GetHUDLayer()->SetVisibility(visibleState);
	this->ClientDisplay3DPresenter::SetHUDVisibility(visibleState);
}

void OGLVideoOutput::SetFiltersPreferGPU(const bool preferGPU)
{
	this->_filtersPreferGPU = preferGPU;
	this->_willFilterOnGPU = (preferGPU) ? this->_hasOGLPixelScaler : false;
}

GLuint OGLVideoOutput::GetTexCPUFilterDstID(const NDSDisplayID displayID) const
{
	return this->_texCPUFilterDstID[displayID];
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

void OGLVideoOutput::CopyFrameToBuffer(uint32_t *dstBuffer)
{
	GLuint texFrameCopyID = 0;
	
	glGenTextures(1, &texFrameCopyID);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texFrameCopyID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, this->_viewportWidth, this->_viewportHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, dstBuffer);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, this->_fboFrameCopyID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, texFrameCopyID, 0);
	
	this->_needUpdateViewport = true;
	this->RenderFrameOGL(true);
	
#ifdef MSB_FIRST
	glReadPixels(0, 0, this->_renderProperty.clientWidth, this->_renderProperty.clientHeight, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, dstBuffer);
#else
	glReadPixels(0, 0, this->_renderProperty.clientWidth, this->_renderProperty.clientHeight, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, dstBuffer);
#endif
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glDeleteTextures(1, &texFrameCopyID);
	
	this->_needUpdateViewport = true;
}

void OGLVideoOutput::RenderFrameOGL(bool isRenderingFlipped)
{
	if (this->_needUpdateViewport)
	{
		this->_UpdateViewport();
		this->_needUpdateViewport = false;
	}
	
	glClear(GL_COLOR_BUFFER_BIT);
	
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		OGLVideoLayer *theLayer = (*_layerList)[i];
		
		if (theLayer->IsVisible())
		{
			theLayer->RenderOGL(isRenderingFlipped);
		}
	}
}

const OGLProcessedFrameInfo& OGLVideoOutput::GetProcessedFrameInfo()
{
	return this->_processedFrameInfo;
}

void OGLVideoOutput::SetProcessedFrameInfo(const OGLProcessedFrameInfo &processedInfo)
{
	this->_processedFrameInfo = processedInfo;
}

void OGLVideoOutput::WriteLockEmuFramebuffer(const uint8_t bufferIndex)
{
	// Do nothing. This is implementation dependent.
}

void OGLVideoOutput::ReadLockEmuFramebuffer(const uint8_t bufferIndex)
{
	// Do nothing. This is implementation dependent.
}

void OGLVideoOutput::UnlockEmuFramebuffer(const uint8_t bufferIndex)
{
	// Do nothing. This is implementation dependent.
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

void OGLFilter::SetScaleOGL(GLfloat scale, void *buffer)
{
	this->_scale = scale;
	this->_dstWidth = this->_srcWidth * this->_scale;
	this->_dstHeight = this->_srcHeight * this->_scale;
	
	bool usedInternalAllocation = false;
	if (buffer == NULL)
	{
		buffer = (uint32_t *)calloc(this->_dstWidth * this->_dstHeight, sizeof(uint32_t));
		usedInternalAllocation = true;
	}
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texDstID);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, this->_dstWidth, this->_dstHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	if (usedInternalAllocation)
	{
		free(buffer);
	}
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
	glReadPixels(0, lineOffset, this->_dstWidth, readLineCount, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, dstBuffer);
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
	_program->SetVertexShaderOGL(Sample3x3_VertShader_110, false, useShader150);
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
	
	_vfMasterDstBuffer = (uint32_t *)malloc_alignedPage(_vf->GetDstWidth() * _vf->GetDstHeight() * sizeof(uint32_t));
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
		_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, false, _useShader150);
		
		const GLuint finalOutputProgramID = _finalOutputProgram->GetProgramID();
		glUseProgram(finalOutputProgramID);
		_uniformAngleDegrees = glGetUniformLocation(finalOutputProgramID, "angleDegrees");
		_uniformScalar = glGetUniformLocation(finalOutputProgramID, "scalar");
		_uniformViewSize = glGetUniformLocation(finalOutputProgramID, "viewSize");
		_uniformRenderFlipped = glGetUniformLocation(finalOutputProgramID, "renderFlipped");
		_uniformBacklightIntensity = glGetUniformLocation(finalOutputProgramID, "backlightIntensity");
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
		shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, false, _useShader150);
		
		SetupHQnxLUTs_OGL(GL_TEXTURE0 + 1, _texLQ2xLUT, _texHQ2xLUT, _texHQ3xLUT, _texHQ4xLUT);
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
	
	if (_canUseShaderOutput)
	{
		glUseProgram(0);
		delete this->_finalOutputProgram;
	}
	
	if (_canUseShaderBasedFilters)
	{
		delete this->_filterDeposterize;
		delete this->_shaderFilter;
		DeleteHQnxLUTs_OGL(GL_TEXTURE0 + 1, this->_texLQ2xLUT, this->_texHQ2xLUT, this->_texHQ3xLUT, this->_texHQ4xLUT);
	}
	
	delete this->_vf;
	free(_vfMasterDstBuffer);
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
	const GLdouble s = ClientDisplayPresenter::GetMaxScalarWithinBounds(this->_normalWidth, this->_normalHeight, w, h);
	
	if (this->_canUseShaderOutput)
	{
		glUniform2f(this->_uniformViewSize, w, h);
		glUniform1f(this->_uniformAngleDegrees, 0.0f);
		glUniform1f(this->_uniformScalar, s);
		glUniform1i(this->_uniformRenderFlipped, GL_FALSE);
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
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, false, _useShader150);
				break;
				
			case OutputFilterTypeID_Bilinear:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, false, _useShader150);
				this->_displayTexFilter = GL_LINEAR;
				break;
				
			case OutputFilterTypeID_BicubicBSpline:
			{
				if (this->_shaderSupport == ShaderSupport_BottomTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, FilterBicubicBSplineFastFragShader_110, false, _useShader150);
				}
				else
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterBicubicBSplineFragShader_110, false, _useShader150);
				}
				break;
			}
				
			case OutputFilterTypeID_BicubicMitchell:
			{
				if (this->_shaderSupport == ShaderSupport_BottomTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, FilterBicubicMitchellNetravaliFastFragShader_110, false, _useShader150);
				}
				else
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterBicubicMitchellNetravaliFragShader_110, false, _useShader150);
				}
				break;
			}
				
			case OutputFilterTypeID_Lanczos2:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterLanczos2FragShader_110, false, _useShader150);
				break;
				
			case OutputFilterTypeID_Lanczos3:
			{
				if (this->_shaderSupport >= ShaderSupport_HighTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample6x6Output_VertShader_110, FilterLanczos3FragShader_110, false, _useShader150);
				}
				else if (this->_shaderSupport >= ShaderSupport_MidTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample5x5Output_VertShader_110, FilterLanczos3FragShader_110, false, _useShader150);
				}
				else
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterLanczos3FragShader_110, false, _useShader150);
				}
				break;
			}
				
			default:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, false, _useShader150);
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
	GLuint currentHQnxLUT = 0;
	
	if (!this->_canUseShaderBasedFilters || filterID == VideoFilterTypeID_None)
	{
		willUseShaderBasedPixelScaler = false;
		return willUseShaderBasedPixelScaler;
	}
	
	OGLShaderProgram *shaderFilterProgram = _shaderFilter->GetProgram();
	
	switch (filterID)
	{
		case VideoFilterTypeID_Nearest1_5X:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, false, _useShader150);
			break;
			
		case VideoFilterTypeID_Nearest2X:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, false, _useShader150);
			break;
			
		case VideoFilterTypeID_Scanline:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, Scalar2xScanlineFragShader_110, false, _useShader150);
			break;
			
		case VideoFilterTypeID_EPX:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, Scalar2xEPXFragShader_110, false, _useShader150);
			break;
			
		case VideoFilterTypeID_EPXPlus:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, Scalar2xEPXPlusFragShader_110, false, _useShader150);
			break;
			
		case VideoFilterTypeID_2xSaI:
		{
			if (this->_shaderSupport >= ShaderSupport_LowTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, Scalar2xSaIFragShader_110, false, _useShader150);
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		case VideoFilterTypeID_Super2xSaI:
		{
			if (this->_shaderSupport >= ShaderSupport_LowTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, ScalarSuper2xSaIFragShader_110, false, _useShader150);
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		case VideoFilterTypeID_SuperEagle:
		{
			if (this->_shaderSupport >= ShaderSupport_LowTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, ScalarSuperEagle2xFragShader_110, false, _useShader150);
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		case VideoFilterTypeID_LQ2X:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerLQ2xFragShader_110, false, _useShader150);
			currentHQnxLUT = this->_texLQ2xLUT;
			break;
			
		case VideoFilterTypeID_LQ2XS:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerLQ2xSFragShader_110, false, _useShader150);
			currentHQnxLUT = this->_texLQ2xLUT;
			break;
			
		case VideoFilterTypeID_HQ2X:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ2xFragShader_110, false, _useShader150);
			currentHQnxLUT = this->_texHQ2xLUT;
			break;
			
		case VideoFilterTypeID_HQ2XS:
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ2xSFragShader_110, false, _useShader150);
			currentHQnxLUT = this->_texHQ2xLUT;
			break;
			
		case VideoFilterTypeID_HQ3X:
		{
			if (this->_shaderSupport >= ShaderSupport_LowTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ3xFragShader_110, false, _useShader150);
				currentHQnxLUT = this->_texHQ3xLUT;
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		case VideoFilterTypeID_HQ3XS:
		{
			if (this->_shaderSupport >= ShaderSupport_LowTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ3xSFragShader_110, false, _useShader150);
				currentHQnxLUT = this->_texHQ3xLUT;
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		case VideoFilterTypeID_HQ4X:
		{
			if (this->_shaderSupport >= ShaderSupport_LowTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ4xFragShader_110, false, _useShader150);
				currentHQnxLUT = this->_texHQ4xLUT;
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		case VideoFilterTypeID_HQ4XS:
		{
			if (this->_shaderSupport >= ShaderSupport_LowTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ4xSFragShader_110, false, _useShader150);
				currentHQnxLUT = this->_texHQ4xLUT;
			}
			else
			{
				willUseShaderBasedPixelScaler = false;
			}
			break;
		}
			
		case VideoFilterTypeID_2xBRZ:
		{
			if (this->_shaderSupport >= ShaderSupport_MidTier)
			{
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler2xBRZFragShader_110, false, _useShader150);
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
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler3xBRZFragShader_110, false, _useShader150);
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
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler4xBRZFragShader_110, false, _useShader150);
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
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler5xBRZFragShader_110, false, _useShader150);
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
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler6xBRZFragShader_110, false, _useShader150);
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
	
	if (currentHQnxLUT != 0)
	{
		glUseProgram(shaderFilterProgram->GetProgramID());
		GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
		glUniform1i(uniformTexSampler, 0);
		
		uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
		glUniform1i(uniformTexSampler, 1);
		glUseProgram(0);
	}
	
	if (willUseShaderBasedPixelScaler)
	{
		const VideoFilterAttributes vfAttr = VideoFilter::GetAttributesByID((VideoFilterTypeID)filterID);
		const GLfloat vfScale = (GLfloat)vfAttr.scaleMultiply / (GLfloat)vfAttr.scaleDivide;
		
		_shaderFilter->SetScaleOGL(vfScale, NULL);
	}
	
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_3D, currentHQnxLUT);
	glActiveTexture(GL_TEXTURE0);
	
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
		uint32_t *newMasterBuffer = (uint32_t *)malloc_alignedPage(newDstBufferWidth * newDstBufferHeight * sizeof(uint32_t));
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
			this->UpdateTexCoords(this->_vf->GetDstWidth(), this->_vf->GetDstHeight());
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
	if (this->_canUseShaderOutput)
	{
		glUseProgram(this->_finalOutputProgram->GetProgramID());
		glUniform1f(this->_uniformBacklightIntensity, 1.0f);
	}
	
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

void OGLVideoLayer::SetNeedsUpdateViewport()
{
	this->_needUpdateViewport = true;
}

void OGLVideoLayer::SetNeedsUpdateRotationScale()
{
	this->_needUpdateRotationScale = true;
}

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
	this->_isVisible = visibleState;
}

#pragma mark -

OGLHUDLayer::OGLHUDLayer(OGLVideoOutput *oglVO)
{
	_isVisible = false;
	_needUpdateViewport = true;
	_output = oglVO;
	
	if (_output->GetContextInfo()->IsShaderSupported())
	{
		_program = new OGLShaderProgram;
		_program->SetShaderSupport(oglVO->GetContextInfo()->GetShaderSupport());
		_program->SetVertexAndFragmentShaderOGL(HUDOutputVertShader_100, HUDOutputFragShader_110, true, oglVO->GetContextInfo()->IsUsingShader150());
		
		glUseProgram(_program->GetProgramID());
		_uniformAngleDegrees = glGetUniformLocation(_program->GetProgramID(), "angleDegrees");
		_uniformScalar = glGetUniformLocation(_program->GetProgramID(), "scalar");
		_uniformViewSize = glGetUniformLocation(_program->GetProgramID(), "viewSize");
		_uniformRenderFlipped = glGetUniformLocation(_program->GetProgramID(), "renderFlipped");
		glUseProgram(0);
	}
	else
	{
		_program = NULL;
	}
	
	glGenTextures(1, &_texCharMap);
	
	// Set up VBOs
	glGenBuffersARB(1, &_vboPositionVertexID);
	glGenBuffersARB(1, &_vboColorVertexID);
	glGenBuffersARB(1, &_vboTexCoordID);
	glGenBuffersARB(1, &_vboElementID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboPositionVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE, NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboColorVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, HUD_VERTEX_COLOR_ATTRIBUTE_BUFFER_SIZE, NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE, NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, _vboElementID);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(GLshort) * HUD_TOTAL_ELEMENTS * 6, NULL, GL_STATIC_DRAW_ARB);
	GLshort *idxBufferPtr = (GLshort *)glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		
	for (size_t i = 0, j = 0, k = 0; i < HUD_TOTAL_ELEMENTS; i++, j+=6, k+=4)
	{
		idxBufferPtr[j+0] = k+0;
		idxBufferPtr[j+1] = k+1;
		idxBufferPtr[j+2] = k+2;
		idxBufferPtr[j+3] = k+2;
		idxBufferPtr[j+4] = k+3;
		idxBufferPtr[j+5] = k+0;
	}
	
	glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	
	// Set up VAO
	glGenVertexArraysDESMUME(1, &_vaoMainStatesID);
	glBindVertexArrayDESMUME(_vaoMainStatesID);
	
	if (oglVO->GetContextInfo()->IsShaderSupported())
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboPositionVertexID);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboColorVertexID);
		glVertexAttribPointer(OGLVertexAttributeID_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, NULL);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, _vboElementID);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_Color);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	else
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboPositionVertexID);
		glVertexPointer(2, GL_FLOAT, 0, NULL);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboColorVertexID);
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, NULL);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
		glTexCoordPointer(2, GL_FLOAT, 0, NULL);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, _vboElementID);
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
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
	glDeleteBuffersARB(1, &this->_vboPositionVertexID);
	glDeleteBuffersARB(1, &this->_vboColorVertexID);
	glDeleteBuffersARB(1, &this->_vboTexCoordID);
	glDeleteBuffersARB(1, &this->_vboElementID);
	
	glDeleteTextures(1, &this->_texCharMap);
}

void OGLHUDLayer::CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo)
{
	FT_Error error = FT_Err_Ok;
	
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
				const uint32_t colorRGBA8888 = 0xFFFFFFFF;
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
		this->_output->ClearHUDNeedsUpdate();
		return;
	}
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboPositionVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE, NULL, GL_STREAM_DRAW_ARB);
	float *vtxPositionBufferPtr = (float *)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	this->_output->SetHUDPositionVertices((float)this->_output->GetViewportWidth(), (float)this->_output->GetViewportHeight(), vtxPositionBufferPtr);
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboColorVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, HUD_VERTEX_COLOR_ATTRIBUTE_BUFFER_SIZE, NULL, GL_STREAM_DRAW_ARB);
	uint32_t *vtxColorBufferPtr = (uint32_t *)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	this->_output->SetHUDColorVertices(vtxColorBufferPtr);
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE, NULL, GL_STREAM_DRAW_ARB);
	float *texCoordBufferPtr = (float *)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	this->_output->SetHUDTextureCoordinates(texCoordBufferPtr);
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	this->_output->ClearHUDNeedsUpdate();
}

void OGLHUDLayer::RenderOGL(bool isRenderingFlipped)
{
	size_t hudLength = this->_output->GetHUDString().length();
	size_t hudTouchLineLength = 0;
	
	if (this->_output->GetHUDShowInput())
	{
		hudLength += HUD_INPUT_ELEMENT_LENGTH;
		
		switch (this->_output->GetMode())
		{
			case ClientDisplayMode_Main:
			case ClientDisplayMode_Touch:
				hudTouchLineLength = HUD_INPUT_TOUCH_LINE_ELEMENTS / 2;
				break;
				
			case ClientDisplayMode_Dual:
			{
				switch (this->_output->GetLayout())
				{
					case ClientDisplayLayout_Vertical:
					case ClientDisplayLayout_Horizontal:
						hudTouchLineLength = HUD_INPUT_TOUCH_LINE_ELEMENTS / 2;
						break;
						
					case ClientDisplayLayout_Hybrid_2_1:
					case ClientDisplayLayout_Hybrid_16_9:
					case ClientDisplayLayout_Hybrid_16_10:
						hudTouchLineLength = HUD_INPUT_TOUCH_LINE_ELEMENTS;
						break;
				}
				
				break;
			}
		}
		
		hudLength += hudTouchLineLength;
	}
	
	if (hudLength <= 1)
	{
		return;
	}
	
	if (this->_output->GetContextInfo()->IsShaderSupported())
	{
		glUseProgram(this->_program->GetProgramID());
		
		if (this->_needUpdateViewport)
		{
			glUniform2f(this->_uniformViewSize, this->_output->GetPresenterProperties().clientWidth, this->_output->GetPresenterProperties().clientHeight);
			glUniform1i(this->_uniformRenderFlipped, (isRenderingFlipped) ? GL_TRUE : GL_FALSE);
			this->_needUpdateViewport = false;
		}
	}
	
	if (this->_output->HUDNeedsUpdate())
	{
		this->_UpdateVerticesOGL();
	}
	
	glBindVertexArrayDESMUME(this->_vaoMainStatesID);
	glBindTexture(GL_TEXTURE_2D, this->_texCharMap);
	
	// First, draw the inputs.
	if (this->_output->GetHUDShowInput())
	{
		const ClientDisplayPresenterProperties &cdv = this->_output->GetPresenterProperties();
		
		if (this->_output->GetContextInfo()->IsShaderSupported())
		{
			glUniform1f(this->_uniformAngleDegrees, cdv.rotation);
			glUniform1f(this->_uniformScalar, cdv.viewScale);
		}
		else
		{
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glRotatef(cdv.rotation, 0.0f, 0.0f, 1.0f);
			glScalef(cdv.viewScale, cdv.viewScale, 1.0f);
		}
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glDrawElements(GL_TRIANGLES, hudTouchLineLength * 6, GL_UNSIGNED_SHORT, (GLvoid *)((this->_output->GetHUDString().length() + HUD_INPUT_ELEMENT_LENGTH) * 6 * sizeof(uint16_t)));
		
		if (this->_output->GetContextInfo()->IsShaderSupported())
		{
			glUniform1f(this->_uniformAngleDegrees, 0.0f);
			glUniform1f(this->_uniformScalar, 1.0f);
		}
		else
		{
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
			glScalef(1.0f, 1.0f, 1.0f);
		}
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glDrawElements(GL_TRIANGLES, HUD_INPUT_ELEMENT_LENGTH * 6, GL_UNSIGNED_SHORT, (GLvoid *)(this->_output->GetHUDString().length() * 6 * sizeof(uint16_t)));
	}
	
	// Next, draw the backing text box.
	if (this->_output->GetContextInfo()->IsShaderSupported())
	{
		glUniform1f(this->_uniformAngleDegrees, 0.0f);
		glUniform1f(this->_uniformScalar, 1.0f);
	}
	else
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
		glScalef(1.0f, 1.0f, 1.0f);
	}
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
	
	// Finally, draw each character inside the box.
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
	glDrawElements(GL_TRIANGLES, (this->_output->GetHUDString().length() - 1) * 6, GL_UNSIGNED_SHORT, (GLvoid *)(6 * sizeof(GLshort)));
	
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArrayDESMUME(0);
}

#pragma mark -

OGLDisplayLayer::OGLDisplayLayer(OGLVideoOutput *oglVO)
{
	_isVisible = true;
	_output = oglVO;
	_needUpdateViewport = true;
	_needUpdateRotationScale = true;
	_needUpdateVertices = true;
	
	_displayTexFilter[0] = GL_NEAREST;
	_displayTexFilter[1] = GL_NEAREST;
	
	pthread_rwlock_init(&_cpuFilterRWLock[NDSDisplayID_Main][0],  NULL);
	pthread_rwlock_init(&_cpuFilterRWLock[NDSDisplayID_Touch][0], NULL);
	pthread_rwlock_init(&_cpuFilterRWLock[NDSDisplayID_Main][1],  NULL);
	pthread_rwlock_init(&_cpuFilterRWLock[NDSDisplayID_Touch][1], NULL);
	
	// Set up VBOs
	glGenBuffersARB(1, &_vboVertexID);
	glGenBuffersARB(1, &_vboTexCoordID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLfloat) * (4 * 8), NULL, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLfloat) * (4 * 8), NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	// Set up VAO
	glGenVertexArraysDESMUME(1, &_vaoMainStatesID);
	glBindVertexArrayDESMUME(_vaoMainStatesID);
	
	if (this->_output->GetContextInfo()->IsShaderSupported())
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboVertexID);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, _vboTexCoordID);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		
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
	
	const bool useShader150 = _output->GetContextInfo()->IsUsingShader150();
	const ShaderSupportTier shaderSupport = _output->GetContextInfo()->GetShaderSupport();
	if (_output->GetContextInfo()->IsShaderSupported())
	{
		_finalOutputProgram = new OGLShaderProgram;
		_finalOutputProgram->SetShaderSupport(shaderSupport);
		_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, false, useShader150);
		
		const GLuint finalOutputProgramID = _finalOutputProgram->GetProgramID();
		glUseProgram(finalOutputProgramID);
		_uniformAngleDegrees = glGetUniformLocation(finalOutputProgramID, "angleDegrees");
		_uniformScalar = glGetUniformLocation(finalOutputProgramID, "scalar");
		_uniformViewSize = glGetUniformLocation(finalOutputProgramID, "viewSize");
		_uniformRenderFlipped = glGetUniformLocation(finalOutputProgramID, "renderFlipped");
		_uniformBacklightIntensity = glGetUniformLocation(finalOutputProgramID, "backlightIntensity");
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
			_filterDeposterize[i] = new OGLFilterDeposterize(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, shaderSupport, useShader150);
			
			_shaderFilter[i] = new OGLFilter(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 1);
			OGLShaderProgram *shaderFilterProgram = _shaderFilter[i]->GetProgram();
			shaderFilterProgram->SetShaderSupport(shaderSupport);
			shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, false, useShader150);
		}
	}
	else
	{
		_filterDeposterize[0] = NULL;
		_filterDeposterize[1] = NULL;
		_shaderFilter[0] = NULL;
		_shaderFilter[1] = NULL;
	}
	
	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
}

OGLDisplayLayer::~OGLDisplayLayer()
{
	if (_output->GetContextInfo()->IsVAOSupported())
	{
		glDeleteVertexArraysDESMUME(1, &this->_vaoMainStatesID);
	}
	
	glDeleteBuffersARB(1, &this->_vboVertexID);
	glDeleteBuffersARB(1, &this->_vboTexCoordID);
	
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
	
	pthread_rwlock_destroy(&this->_cpuFilterRWLock[NDSDisplayID_Main][0]);
	pthread_rwlock_destroy(&this->_cpuFilterRWLock[NDSDisplayID_Touch][0]);
	pthread_rwlock_destroy(&this->_cpuFilterRWLock[NDSDisplayID_Main][1]);
	pthread_rwlock_destroy(&this->_cpuFilterRWLock[NDSDisplayID_Touch][1]);
}

void OGLDisplayLayer::_UpdateRotationScaleOGL()
{
	const ClientDisplayPresenterProperties &cdv = this->_output->GetPresenterProperties();
	const double r = cdv.rotation;
	const double s = cdv.viewScale;
	
	if (this->_output->GetContextInfo()->IsShaderSupported())
	{
		glUniform1f(this->_uniformAngleDegrees, r);
		glUniform1f(this->_uniformScalar, s);
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
	
	this->_needUpdateVertices = false;
}

OutputFilterTypeID OGLDisplayLayer::SetOutputFilterOGL(const OutputFilterTypeID filterID)
{
	OutputFilterTypeID outputFilter = OutputFilterTypeID_NearestNeighbor;
	
	this->_displayTexFilter[0] = GL_NEAREST;
	this->_displayTexFilter[1] = GL_NEAREST;
	
	if (this->_output->CanFilterOnGPU())
	{
		const bool useShader150 = _output->GetContextInfo()->IsUsingShader150();
		const ShaderSupportTier shaderSupport = _output->GetContextInfo()->GetShaderSupport();
		
		outputFilter = filterID;
		
		switch (filterID)
		{
			case OutputFilterTypeID_NearestNeighbor:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, false, useShader150);
				break;
				
			case OutputFilterTypeID_Bilinear:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, false, useShader150);
				this->_displayTexFilter[0] = GL_LINEAR;
				this->_displayTexFilter[1] = GL_LINEAR;
				break;
				
			case OutputFilterTypeID_BicubicBSpline:
			{
				if (shaderSupport == ShaderSupport_BottomTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, FilterBicubicBSplineFastFragShader_110, false, useShader150);
				}
				else
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterBicubicBSplineFragShader_110, false, useShader150);
				}
				break;
			}
				
			case OutputFilterTypeID_BicubicMitchell:
			{
				if (shaderSupport == ShaderSupport_BottomTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, FilterBicubicMitchellNetravaliFastFragShader_110, false, useShader150);
				}
				else
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterBicubicMitchellNetravaliFragShader_110, false, useShader150);
				}
				break;
			}
				
			case OutputFilterTypeID_Lanczos2:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterLanczos2FragShader_110, false, useShader150);
				break;
				
			case OutputFilterTypeID_Lanczos3:
			{
				if (shaderSupport >= ShaderSupport_HighTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample6x6Output_VertShader_110, FilterLanczos3FragShader_110, false, useShader150);
				}
				else if (shaderSupport >= ShaderSupport_MidTier)
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample5x5Output_VertShader_110, FilterLanczos3FragShader_110, false, useShader150);
				}
				else
				{
					this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterLanczos3FragShader_110, false, useShader150);
				}
				break;
			}
				
			default:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110, false, useShader150);
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
	GLuint currentHQnxLUT = 0;
	
	if (!this->_output->CanFilterOnGPU() || filterID == VideoFilterTypeID_None)
	{
		willUseShaderBasedPixelScaler = false;
		return willUseShaderBasedPixelScaler;
	}
	
	for (size_t i = 0; i < 2; i++)
	{
		OGLShaderProgram *shaderFilterProgram = _shaderFilter[i]->GetProgram();
		const bool useShader150 = _output->GetContextInfo()->IsUsingShader150();
		const ShaderSupportTier shaderSupport = _output->GetContextInfo()->GetShaderSupport();
		
		switch (filterID)
		{
			case VideoFilterTypeID_Nearest1_5X:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, false, useShader150);
				break;
				
			case VideoFilterTypeID_Nearest2X:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110, false, useShader150);
				break;
				
			case VideoFilterTypeID_Scanline:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, Scalar2xScanlineFragShader_110, false, useShader150);
				break;
				
			case VideoFilterTypeID_EPX:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, Scalar2xEPXFragShader_110, false, useShader150);
				break;
				
			case VideoFilterTypeID_EPXPlus:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, Scalar2xEPXPlusFragShader_110, false, useShader150);
				break;
				
			case VideoFilterTypeID_2xSaI:
			{
				if (shaderSupport >= ShaderSupport_LowTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, Scalar2xSaIFragShader_110, false, useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_Super2xSaI:
			{
				if (shaderSupport >= ShaderSupport_LowTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, ScalarSuper2xSaIFragShader_110, false, useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_SuperEagle:
			{
				if (shaderSupport >= ShaderSupport_LowTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, ScalarSuperEagle2xFragShader_110, false, useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_LQ2X:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerLQ2xFragShader_110, false, useShader150);
				currentHQnxLUT = ((const OGLClientFetchObject &)this->_output->GetFetchObject()).GetTexLQ2xLUT();
				break;
				
			case VideoFilterTypeID_LQ2XS:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerLQ2xSFragShader_110, false, useShader150);
				currentHQnxLUT = ((const OGLClientFetchObject &)this->_output->GetFetchObject()).GetTexLQ2xLUT();
				break;
				
			case VideoFilterTypeID_HQ2X:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ2xFragShader_110, false, useShader150);
				currentHQnxLUT = ((const OGLClientFetchObject &)this->_output->GetFetchObject()).GetTexHQ2xLUT();
				break;
				
			case VideoFilterTypeID_HQ2XS:
				shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ2xSFragShader_110, false, useShader150);
				currentHQnxLUT = ((const OGLClientFetchObject &)this->_output->GetFetchObject()).GetTexHQ2xLUT();
				break;
				
			case VideoFilterTypeID_HQ3X:
			{
				if (shaderSupport >= ShaderSupport_LowTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ3xFragShader_110, false, useShader150);
					currentHQnxLUT = ((const OGLClientFetchObject &)this->_output->GetFetchObject()).GetTexHQ3xLUT();
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_HQ3XS:
			{
				if (shaderSupport >= ShaderSupport_LowTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ3xSFragShader_110, false, useShader150);
					currentHQnxLUT = ((const OGLClientFetchObject &)this->_output->GetFetchObject()).GetTexHQ3xLUT();
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_HQ4X:
			{
				if (shaderSupport >= ShaderSupport_LowTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ4xFragShader_110, false, useShader150);
					currentHQnxLUT = ((const OGLClientFetchObject &)this->_output->GetFetchObject()).GetTexHQ4xLUT();
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_HQ4XS:
			{
				if (shaderSupport >= ShaderSupport_LowTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, ScalerHQ4xSFragShader_110, false, useShader150);
					currentHQnxLUT = ((const OGLClientFetchObject &)this->_output->GetFetchObject()).GetTexHQ4xLUT();
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_2xBRZ:
			{
				if (shaderSupport >= ShaderSupport_MidTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler2xBRZFragShader_110, false, useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_3xBRZ:
			{
				if (shaderSupport >= ShaderSupport_MidTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler3xBRZFragShader_110, false, useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_4xBRZ:
			{
				if (shaderSupport >= ShaderSupport_MidTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler4xBRZFragShader_110, false, useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_5xBRZ:
			{
				if (shaderSupport >= ShaderSupport_MidTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler5xBRZFragShader_110, false, useShader150);
				}
				else
				{
					willUseShaderBasedPixelScaler = false;
				}
				break;
			}
				
			case VideoFilterTypeID_6xBRZ:
			{
				if (shaderSupport >= ShaderSupport_MidTier)
				{
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample5x5_VertShader_110, Scaler6xBRZFragShader_110, false, useShader150);
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
		
		if (currentHQnxLUT != 0)
		{
			glUseProgram(shaderFilterProgram->GetProgramID());
			GLint uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "tex");
			glUniform1i(uniformTexSampler, 0);
			
			uniformTexSampler = glGetUniformLocation(shaderFilterProgram->GetProgramID(), "lut");
			glUniform1i(uniformTexSampler, 1);
			glUseProgram(0);
		}
		
		if (willUseShaderBasedPixelScaler)
		{
			const VideoFilterAttributes vfAttr = VideoFilter::GetAttributesByID((VideoFilterTypeID)filterID);
			const GLfloat vfScale = (GLfloat)vfAttr.scaleMultiply / (GLfloat)vfAttr.scaleDivide;
			
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
			this->_shaderFilter[i]->SetScaleOGL(vfScale, this->_output->GetPixelScalerObject((NDSDisplayID)i)->GetDstBufferPtr());
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
		}
	}
	
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_3D, currentHQnxLUT);
	glActiveTexture(GL_TEXTURE0);
	
	return willUseShaderBasedPixelScaler;
}

void OGLDisplayLayer::LoadNativeDisplayByID_OGL(const NDSDisplayID displayID)
{
	if ((this->_output->GetPixelScaler() != VideoFilterTypeID_None) && !this->_output->WillFilterOnGPU() && !this->_output->GetSourceDeposterize())
	{
		OGLClientFetchObject &fetchObjMutable = (OGLClientFetchObject &)this->_output->GetFetchObject();
		VideoFilter *vf = this->_output->GetPixelScalerObject(displayID);
		
		const uint8_t bufferIndex = fetchObjMutable.GetLastFetchIndex();
		
		pthread_rwlock_wrlock(&this->_cpuFilterRWLock[displayID][bufferIndex]);
		fetchObjMutable.CopyFromSrcClone(vf->GetSrcBufferPtr(), displayID, bufferIndex);
		pthread_rwlock_unlock(&this->_cpuFilterRWLock[displayID][bufferIndex]);
	}
}

void OGLDisplayLayer::ProcessOGL()
{
	OGLClientFetchObject &fetchObj = (OGLClientFetchObject &)this->_output->GetFetchObject();
	const uint8_t bufferIndex = fetchObj.GetLastFetchIndex();
	const NDSDisplayInfo &emuDisplayInfo = this->_output->GetEmuDisplayInfo();
	const ClientDisplayMode mode = this->_output->GetPresenterProperties().mode;
	const bool useDeposterize = this->_output->GetSourceDeposterize();
	const NDSDisplayID selectedDisplaySource[2] = { this->_output->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Main), this->_output->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Touch) };
	
	const bool didRenderNative[2] = { !emuDisplayInfo.didPerformCustomRender[selectedDisplaySource[NDSDisplayID_Main]], !emuDisplayInfo.didPerformCustomRender[selectedDisplaySource[NDSDisplayID_Touch]] };

	GLuint texMain  = (selectedDisplaySource[NDSDisplayID_Main]  == NDSDisplayID_Main)  ? fetchObj.GetFetchTexture(NDSDisplayID_Main)  : fetchObj.GetFetchTexture(NDSDisplayID_Touch);
	GLuint texTouch = (selectedDisplaySource[NDSDisplayID_Touch] == NDSDisplayID_Touch) ? fetchObj.GetFetchTexture(NDSDisplayID_Touch) : fetchObj.GetFetchTexture(NDSDisplayID_Main);
	
	GLsizei width[2]  = { emuDisplayInfo.renderedWidth[selectedDisplaySource[NDSDisplayID_Main]],  emuDisplayInfo.renderedWidth[selectedDisplaySource[NDSDisplayID_Touch]] };
	GLsizei height[2] = { emuDisplayInfo.renderedHeight[selectedDisplaySource[NDSDisplayID_Main]], emuDisplayInfo.renderedHeight[selectedDisplaySource[NDSDisplayID_Touch]] };
	
	VideoFilter *vfMain  = this->_output->GetPixelScalerObject(NDSDisplayID_Main);
	VideoFilter *vfTouch = this->_output->GetPixelScalerObject(NDSDisplayID_Touch);
	
	bool isDisplayProcessedMain  = false;
	bool isDisplayProcessedTouch = false;
	
	if ( (emuDisplayInfo.pixelBytes != 0) && (useDeposterize || (this->_output->GetPixelScaler() != VideoFilterTypeID_None)) )
	{
		// Run the video source filters and the pixel scalers
		const bool willFilterOnGPU = this->_output->WillFilterOnGPU();
		const bool shouldProcessDisplay[2] = { (didRenderNative[NDSDisplayID_Main]  || !emuDisplayInfo.isCustomSizeRequested) && this->_output->IsSelectedDisplayEnabled(NDSDisplayID_Main)  && (mode == ClientDisplayMode_Main  || mode == ClientDisplayMode_Dual),
		                                       (didRenderNative[NDSDisplayID_Touch] || !emuDisplayInfo.isCustomSizeRequested) && this->_output->IsSelectedDisplayEnabled(NDSDisplayID_Touch) && (mode == ClientDisplayMode_Touch || mode == ClientDisplayMode_Dual) && (selectedDisplaySource[NDSDisplayID_Main] != selectedDisplaySource[NDSDisplayID_Touch]) };
		
		bool texFetchMainNeedsLock  = (useDeposterize || ((this->_output->GetPixelScaler() != VideoFilterTypeID_None) && willFilterOnGPU)) && shouldProcessDisplay[NDSDisplayID_Main];
		bool texFetchTouchNeedsLock = (useDeposterize || ((this->_output->GetPixelScaler() != VideoFilterTypeID_None) && willFilterOnGPU)) && shouldProcessDisplay[NDSDisplayID_Touch];
		bool needsFetchBuffersLock = texFetchMainNeedsLock || texFetchTouchNeedsLock;
		
		if (needsFetchBuffersLock)
		{
			this->_output->ReadLockEmuFramebuffer(bufferIndex);
		}
		
		if (useDeposterize)
		{
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				if (texFetchMainNeedsLock)
				{
					fetchObj.FetchTextureReadLock(NDSDisplayID_Main);
				}
				
				// For all shader-based filters, we need to temporarily disable GL_UNPACK_CLIENT_STORAGE_APPLE.
				// Filtered images are supposed to remain on the GPU for immediate use for further GPU processing,
				// so using client-backed buffers for filtered images would simply waste memory here.
				glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
				texMain = this->_filterDeposterize[NDSDisplayID_Main]->RunFilterOGL(texMain);
				glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
				
				if (texFetchMainNeedsLock)
				{
					fetchObj.FetchTextureUnlock(NDSDisplayID_Main);
				}
				
				isDisplayProcessedMain = true;
				texFetchMainNeedsLock = false;
				
				if (selectedDisplaySource[NDSDisplayID_Main] == selectedDisplaySource[NDSDisplayID_Touch])
				{
					isDisplayProcessedTouch = true;
					texFetchTouchNeedsLock = false;
				}
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				if (texFetchTouchNeedsLock)
				{
					fetchObj.FetchTextureReadLock(NDSDisplayID_Touch);
				}
				
				glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
				texTouch = this->_filterDeposterize[NDSDisplayID_Touch]->RunFilterOGL(texTouch);
				glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
				
				if (texFetchTouchNeedsLock)
				{
					fetchObj.FetchTextureUnlock(NDSDisplayID_Touch);
				}
				
				isDisplayProcessedTouch = true;
				texFetchTouchNeedsLock = false;
			}
			
			if (needsFetchBuffersLock && (!shouldProcessDisplay[NDSDisplayID_Main] || isDisplayProcessedMain) && (!shouldProcessDisplay[NDSDisplayID_Touch] || isDisplayProcessedTouch))
			{
				glFinish();
				needsFetchBuffersLock = false;
				this->_output->UnlockEmuFramebuffer(bufferIndex);
			}
		}
		
		// Run the pixel scalers. First attempt on the GPU.
		if ( (this->_output->GetPixelScaler() != VideoFilterTypeID_None) && willFilterOnGPU )
		{
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				if (texFetchMainNeedsLock)
				{
					fetchObj.FetchTextureReadLock(NDSDisplayID_Main);
				}
				
				glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
				texMain = this->_shaderFilter[NDSDisplayID_Main]->RunFilterOGL(texMain);
				glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
				
				if (texFetchMainNeedsLock)
				{
					fetchObj.FetchTextureUnlock(NDSDisplayID_Main);
				}
				
				width[NDSDisplayID_Main]  = (GLsizei)vfMain->GetDstWidth();
				height[NDSDisplayID_Main] = (GLsizei)vfMain->GetDstHeight();
				isDisplayProcessedMain = true;
				texFetchMainNeedsLock = false;
				
				if (selectedDisplaySource[NDSDisplayID_Main] == selectedDisplaySource[NDSDisplayID_Touch])
				{
					isDisplayProcessedTouch = true;
					texFetchTouchNeedsLock = false;
				}
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				if (texFetchTouchNeedsLock)
				{
					fetchObj.FetchTextureReadLock(NDSDisplayID_Touch);
				}
				
				glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
				texTouch = this->_shaderFilter[NDSDisplayID_Touch]->RunFilterOGL(texTouch);
				glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
				
				if (texFetchTouchNeedsLock)
				{
					fetchObj.FetchTextureUnlock(NDSDisplayID_Touch);
				}
				
				width[NDSDisplayID_Touch]  = (GLsizei)vfTouch->GetDstWidth();
				height[NDSDisplayID_Touch] = (GLsizei)vfTouch->GetDstHeight();
				isDisplayProcessedTouch = true;
				texFetchTouchNeedsLock = false;
			}
			
			if (needsFetchBuffersLock && (!shouldProcessDisplay[NDSDisplayID_Main] || isDisplayProcessedMain) && (!shouldProcessDisplay[NDSDisplayID_Touch] || isDisplayProcessedTouch))
			{
				glFinish();
				needsFetchBuffersLock = false;
				this->_output->UnlockEmuFramebuffer(bufferIndex);
			}
		}
		
		// If the pixel scaler didn't already run on the GPU, then run the pixel scaler on the CPU.
		if ( (this->_output->GetPixelScaler() != VideoFilterTypeID_None) && !willFilterOnGPU )
		{
			if (useDeposterize)
			{
				// Hybrid CPU/GPU-based path (may cause a performance hit on pixel download)
				if (shouldProcessDisplay[NDSDisplayID_Main])
				{
					this->_filterDeposterize[NDSDisplayID_Main]->DownloadDstBufferOGL(vfMain->GetSrcBufferPtr(), 0, vfMain->GetSrcHeight());
				}
				
				if (shouldProcessDisplay[NDSDisplayID_Touch])
				{
					this->_filterDeposterize[NDSDisplayID_Touch]->DownloadDstBufferOGL(vfTouch->GetSrcBufferPtr(), 0, vfTouch->GetSrcHeight());
				}
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				pthread_rwlock_rdlock(&this->_cpuFilterRWLock[NDSDisplayID_Main][bufferIndex]);
				vfMain->RunFilter();
				pthread_rwlock_unlock(&this->_cpuFilterRWLock[NDSDisplayID_Main][bufferIndex]);
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				pthread_rwlock_rdlock(&this->_cpuFilterRWLock[NDSDisplayID_Touch][bufferIndex]);
				vfTouch->RunFilter();
				pthread_rwlock_unlock(&this->_cpuFilterRWLock[NDSDisplayID_Touch][bufferIndex]);
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				texMain = this->_output->GetTexCPUFilterDstID(NDSDisplayID_Main);
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texMain);
				glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, vfMain->GetDstWidth(), vfMain->GetDstHeight(), GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, vfMain->GetDstBufferPtr());
				
				width[NDSDisplayID_Main]  = (GLsizei)vfMain->GetDstWidth();
				height[NDSDisplayID_Main] = (GLsizei)vfMain->GetDstHeight();
				isDisplayProcessedMain = true;
				
				if (selectedDisplaySource[NDSDisplayID_Main] == selectedDisplaySource[NDSDisplayID_Touch])
				{
					isDisplayProcessedTouch = true;
				}
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				texTouch = this->_output->GetTexCPUFilterDstID(NDSDisplayID_Touch);
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texTouch);
				glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, vfTouch->GetDstWidth(), vfTouch->GetDstHeight(), GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, vfTouch->GetDstBufferPtr());
				
				width[NDSDisplayID_Touch]  = (GLsizei)vfTouch->GetDstWidth();
				height[NDSDisplayID_Touch] = (GLsizei)vfTouch->GetDstHeight();
				isDisplayProcessedTouch = true;
			}
		}
	}
	
	// Set the final output texture IDs
	if (selectedDisplaySource[NDSDisplayID_Touch] == selectedDisplaySource[NDSDisplayID_Main])
	{
		texTouch = texMain;
		width[NDSDisplayID_Touch]  = width[NDSDisplayID_Main];
		height[NDSDisplayID_Touch] = height[NDSDisplayID_Main];
	}
	
	// Update the texture coordinates
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, (4 * 8) * sizeof(GLfloat), NULL, GL_STREAM_DRAW_ARB);
	float *texCoordPtr = (float *)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	
	this->_output->SetScreenTextureCoordinates((float)width[NDSDisplayID_Main],  (float)height[NDSDisplayID_Main],
											   (float)width[NDSDisplayID_Touch], (float)height[NDSDisplayID_Touch],
											   texCoordPtr);
	
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	
	// OpenGL shader-based filters can modify the viewport, so it needs to be reset here.
	glViewport(0, 0, this->_output->GetViewportWidth(), this->_output->GetViewportHeight());
	
	glFlush();
	
	OGLProcessedFrameInfo newFrameInfo;
	newFrameInfo.bufferIndex = bufferIndex;
	newFrameInfo.isMainDisplayProcessed  = isDisplayProcessedMain;
	newFrameInfo.isTouchDisplayProcessed = isDisplayProcessedTouch;
	newFrameInfo.texID[NDSDisplayID_Main]  = texMain;
	newFrameInfo.texID[NDSDisplayID_Touch] = texTouch;
	
	this->_output->SetProcessedFrameInfo(newFrameInfo);
}

void OGLDisplayLayer::RenderOGL(bool isRenderingFlipped)
{
	const bool isShaderSupported = this->_output->GetContextInfo()->IsShaderSupported();
	
	if (isShaderSupported)
	{
		glUseProgram(this->_finalOutputProgram->GetProgramID());
		
		if (this->_needUpdateViewport)
		{
			glUniform2f(this->_uniformViewSize, this->_output->GetViewportWidth(), this->_output->GetViewportHeight());
			glUniform1i(this->_uniformRenderFlipped, (isRenderingFlipped) ? GL_TRUE : GL_FALSE);
			this->_needUpdateViewport = false;
		}
	}
	
	if (this->_needUpdateViewport)
	{
		const GLdouble w = this->_output->GetViewportWidth();
		const GLdouble h = this->_output->GetViewportHeight();
		
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		
		if (isRenderingFlipped)
		{
			glOrtho(-w/2.0, -w/2.0 + w, -h/2.0 + h, -h/2.0, -1.0, 1.0);
		}
		else
		{
			glOrtho(-w/2.0, -w/2.0 + w, -h/2.0, -h/2.0 + h, -1.0, 1.0);
		}
		
		this->_needUpdateViewport = false;
	}
	
	if (this->_needUpdateRotationScale)
	{
		this->_UpdateRotationScaleOGL();
	}
	
	if (this->_needUpdateVertices)
	{
		this->_UpdateVerticesOGL();
	}
	
	OGLClientFetchObject &fetchObj = (OGLClientFetchObject &)this->_output->GetFetchObject();
	const NDSDisplayInfo &emuDisplayInfo = this->_output->GetEmuDisplayInfo();
	const float backlightIntensity[2] = { emuDisplayInfo.backlightIntensity[NDSDisplayID_Main], emuDisplayInfo.backlightIntensity[NDSDisplayID_Touch] };
	
	const OGLProcessedFrameInfo processedInfo = this->_output->GetProcessedFrameInfo();
	bool texFetchMainNeedsLock  = !processedInfo.isMainDisplayProcessed;
	bool texFetchTouchNeedsLock = !processedInfo.isTouchDisplayProcessed;
	const bool needsFetchBuffersLock = texFetchMainNeedsLock || texFetchTouchNeedsLock;
	
	if (needsFetchBuffersLock)
	{
		this->_output->ReadLockEmuFramebuffer(processedInfo.bufferIndex);
	}
	
	glBindVertexArrayDESMUME(this->_vaoMainStatesID);
	
	switch (this->_output->GetPresenterProperties().mode)
	{
		case ClientDisplayMode_Main:
		{
			if (this->_output->IsSelectedDisplayEnabled(NDSDisplayID_Main))
			{
				if (isShaderSupported)
				{
					glUniform1f(this->_uniformBacklightIntensity, backlightIntensity[NDSDisplayID_Main]);
				}
				
				if (texFetchMainNeedsLock)
				{
					fetchObj.FetchTextureWriteLock(NDSDisplayID_Main);
				}
				
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB, processedInfo.texID[NDSDisplayID_Main]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter[NDSDisplayID_Main]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter[NDSDisplayID_Main]);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				
				if (texFetchMainNeedsLock)
				{
					fetchObj.FetchTextureUnlock(NDSDisplayID_Main);
					texFetchMainNeedsLock = false;
				}
			}
			break;
		}
			
		case ClientDisplayMode_Touch:
		{
			if (this->_output->IsSelectedDisplayEnabled(NDSDisplayID_Touch))
			{
				if (isShaderSupported)
				{
					glUniform1f(this->_uniformBacklightIntensity, backlightIntensity[NDSDisplayID_Touch]);
				}
				
				if (texFetchTouchNeedsLock)
				{
					fetchObj.FetchTextureWriteLock(NDSDisplayID_Touch);
				}
				
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB, processedInfo.texID[NDSDisplayID_Touch]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter[NDSDisplayID_Touch]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter[NDSDisplayID_Touch]);
				glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
				
				if (texFetchTouchNeedsLock)
				{
					fetchObj.FetchTextureUnlock(NDSDisplayID_Touch);
					texFetchTouchNeedsLock = false;
				}
			}
			break;
		}
			
		case ClientDisplayMode_Dual:
		{
			const NDSDisplayID majorDisplayID = (this->_output->GetPresenterProperties().order == ClientDisplayOrder_MainFirst) ? NDSDisplayID_Main : NDSDisplayID_Touch;
			const size_t majorDisplayVtx = (this->_output->GetPresenterProperties().order == ClientDisplayOrder_MainFirst) ? 8 : 12;
			
			bool texFetchMainAlreadyLocked = false;
			bool texFetchTouchAlreadyLocked = false;
			
			switch (this->_output->GetPresenterProperties().layout)
			{
				case ClientDisplayLayout_Hybrid_2_1:
				case ClientDisplayLayout_Hybrid_16_9:
				case ClientDisplayLayout_Hybrid_16_10:
				{
					if (this->_output->IsSelectedDisplayEnabled(majorDisplayID))
					{
						if (isShaderSupported)
						{
							glUniform1f(this->_uniformBacklightIntensity, backlightIntensity[majorDisplayID]);
						}
						
						const bool texFetchMajorNeedsLock = (majorDisplayID == NDSDisplayID_Main) ? texFetchMainNeedsLock : texFetchTouchNeedsLock;
						
						if (texFetchMajorNeedsLock)
						{
							fetchObj.FetchTextureWriteLock(majorDisplayID);
						}
						
						glBindTexture(GL_TEXTURE_RECTANGLE_ARB, processedInfo.texID[majorDisplayID]);
						glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter[majorDisplayID]);
						glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter[majorDisplayID]);
						glDrawArrays(GL_TRIANGLE_STRIP, majorDisplayVtx, 4);
						
						if (texFetchMajorNeedsLock)
						{
							if (majorDisplayID == NDSDisplayID_Main)
							{
								texFetchMainAlreadyLocked = true;
							}
							else
							{
								texFetchTouchAlreadyLocked = true;
							}
						}
					}
					break;
				}
					
				default:
					break;
			}
			
			if (this->_output->IsSelectedDisplayEnabled(NDSDisplayID_Main))
			{
				if (isShaderSupported)
				{
					glUniform1f(this->_uniformBacklightIntensity, backlightIntensity[NDSDisplayID_Main]);
				}
				
				if (texFetchMainNeedsLock && !texFetchMainAlreadyLocked)
				{
					fetchObj.FetchTextureWriteLock(NDSDisplayID_Main);
				}
				
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB, processedInfo.texID[NDSDisplayID_Main]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter[NDSDisplayID_Main]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter[NDSDisplayID_Main]);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				
				if (texFetchMainNeedsLock)
				{
					fetchObj.FetchTextureUnlock(NDSDisplayID_Main);
					texFetchMainNeedsLock = false;
				}
			}
			
			if (this->_output->IsSelectedDisplayEnabled(NDSDisplayID_Touch))
			{
				if (isShaderSupported)
				{
					glUniform1f(this->_uniformBacklightIntensity, backlightIntensity[NDSDisplayID_Touch]);
				}
				
				if (texFetchTouchNeedsLock && !texFetchTouchAlreadyLocked)
				{
					fetchObj.FetchTextureWriteLock(NDSDisplayID_Touch);
				}
				
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB, processedInfo.texID[NDSDisplayID_Touch]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter[NDSDisplayID_Touch]);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter[NDSDisplayID_Touch]);
				glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
				
				if (texFetchTouchNeedsLock)
				{
					fetchObj.FetchTextureUnlock(NDSDisplayID_Touch);
					texFetchTouchNeedsLock = false;
				}
			}
		}
			
		default:
			break;
	}
	
	glBindVertexArrayDESMUME(0);
	
	if (needsFetchBuffersLock)
	{
		this->_output->UnlockEmuFramebuffer(processedInfo.bufferIndex);
	}
}
