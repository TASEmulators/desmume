/*
	Copyright (C) 2014-2015 DeSmuME team

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
#include "../filter/videofilter.h"


// VERTEX SHADER FOR DISPLAY OUTPUT
static const char *Sample1x1OutputVertShader_100 = {"\
	attribute vec2 inPosition; \n\
	attribute vec2 inTexCoord0; \n\
	\n\
	uniform vec2 viewSize; \n\
	uniform float scalar; \n\
	uniform float angleDegrees; \n\
	\n\
	varying vec2 texCoord[1]; \n\
	\n\
	void main() \n\
	{ \n\
		float angleRadians = radians(angleDegrees); \n\
		\n\
		mat2 projection	= mat2(	vec2(2.0/viewSize.x,            0.0), \n\
								vec2(           0.0, 2.0/viewSize.y)); \n\
		\n\
		mat2 rotation	= mat2(	vec2(cos(angleRadians), -sin(angleRadians)), \n\
								vec2(sin(angleRadians),  cos(angleRadians))); \n\
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
	attribute vec2 inPosition;\n\
	attribute vec2 inTexCoord0;\n\
	\n\
	uniform vec2 viewSize; \n\
	uniform float scalar; \n\
	uniform float angleDegrees; \n\
	\n\
	varying vec2 texCoord[16];\n\
	\n\
	void main()\n\
	{\n\
		float angleRadians = radians(angleDegrees); \n\
		\n\
		mat2 projection	= mat2(	vec2(2.0/viewSize.x,            0.0), \n\
								vec2(           0.0, 2.0/viewSize.y)); \n\
		\n\
		mat2 rotation	= mat2(	vec2(cos(angleRadians), -sin(angleRadians)), \n\
								vec2(sin(angleRadians),  cos(angleRadians))); \n\
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

// FRAGMENT SHADER FOR DISPLAY OUTPUT
static const char *PassthroughOutputFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	\n\
	void main()\n\
	{\n\
		gl_FragColor = texture2DRect(tex, texCoord[0]);\n\
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
	attribute vec2 inPosition;\n\
	attribute vec2 inTexCoord0;\n\
	varying vec2 texCoord[1];\n\
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
	attribute vec2 inPosition;\n\
	attribute vec2 inTexCoord0;\n\
	varying vec2 texCoord[4];\n\
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
	attribute vec2 inPosition;\n\
	attribute vec2 inTexCoord0;\n\
	varying vec2 texCoord[9];\n\
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
	attribute vec2 inPosition;\n\
	attribute vec2 inTexCoord0;\n\
	varying vec2 texCoord[16];\n\
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

// FRAGMENT SHADER PASSTHROUGH FOR FILTERS
static const char *PassthroughFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	\n\
	void main()\n\
	{\n\
		gl_FragColor = texture2DRect(tex, texCoord[0]);\n\
	}\n\
"};

// FRAGMENT SHADER FOR DEPOSTERIZATION
static const char *FilterDeposterizeFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	\n\
	vec4 InterpLTE(vec4 pixA, vec4 pixB, vec4 threshold)\n\
	{\n\
		vec4 interpPix = mix(pixA, pixB, 0.5);\n\
		vec4 pixCompare = vec4( lessThanEqual(abs(pixB - pixA), threshold) );\n\
		\n\
		return mix(pixA, interpPix, pixCompare);\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  --|07|--\n\
	//                       05|00|01\n\
	//                       --|03|--\n\
	//\n\
	// Output Pixel Mapping:    A\n\
	\n\
	void main()\n\
	{\n\
		const vec4 threshold = vec4(0.0355);\n\
		vec4 pU = texture2DRect(tex, texCoord[7]);\n\
		vec4 pL = texture2DRect(tex, texCoord[5]);\n\
		vec4 pC = texture2DRect(tex, texCoord[0]); // Center pixel\n\
		vec4 pR = texture2DRect(tex, texCoord[1]);\n\
		vec4 pD = texture2DRect(tex, texCoord[3]);\n\
		\n\
		vec4 tempL = InterpLTE(pC, pL, threshold);\n\
		vec4 tempR = InterpLTE(pC, pR, threshold);\n\
		vec4 tempU = InterpLTE(pC, pU, threshold);\n\
		vec4 tempD = InterpLTE(pC, pD, threshold);\n\
		\n\
		gl_FragColor = mix( mix(tempL, tempR, 0.5), mix(tempU, tempD, 0.5), 0.5 );\n\
	}\n\
"};

static const char *FilterBicubicBSplineFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[16];\n\
	uniform sampler2DRect tex;\n\
	\n\
	vec4 WeightBSpline(float f)\n\
	{\n\
		return	vec4(pow((1.0 - f), 3.0) / 6.0,\n\
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
		gl_FragColor	= (texture2DRect(tex, texCoord[ 6]) * wx.r\n\
						+  texture2DRect(tex, texCoord[ 7]) * wx.g\n\
						+  texture2DRect(tex, texCoord[ 8]) * wx.b\n\
						+  texture2DRect(tex, texCoord[ 9]) * wx.a) * wy.r\n\
						+ (texture2DRect(tex, texCoord[ 5]) * wx.r\n\
						+  texture2DRect(tex, texCoord[ 0]) * wx.g\n\
						+  texture2DRect(tex, texCoord[ 1]) * wx.b\n\
						+  texture2DRect(tex, texCoord[10]) * wx.a) * wy.g\n\
						+ (texture2DRect(tex, texCoord[ 4]) * wx.r\n\
						+  texture2DRect(tex, texCoord[ 3]) * wx.g\n\
						+  texture2DRect(tex, texCoord[ 2]) * wx.b\n\
						+  texture2DRect(tex, texCoord[11]) * wx.a) * wy.b\n\
						+ (texture2DRect(tex, texCoord[15]) * wx.r\n\
						+  texture2DRect(tex, texCoord[14]) * wx.g\n\
						+  texture2DRect(tex, texCoord[13]) * wx.b\n\
						+  texture2DRect(tex, texCoord[12]) * wx.a) * wy.a;\n\
	}\n\
"};

static const char *FilterBicubicBSplineFastFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	\n\
	void main()\n\
	{\n\
		vec2 texCenterPosition = floor(texCoord[0] - 0.5) + 0.5;\n\
		vec2 f = abs(texCoord[0] - texCenterPosition);\n\
		\n\
		vec2 w0 = pow((1.0 - f), vec2(3.0, 3.0)) / 6.0;\n\
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
		gl_FragColor	= (texture2DRect(tex, vec2(t0.x, t0.y)) * s0.x +\n\
						   texture2DRect(tex, vec2(t1.x, t0.y)) * s1.x) * s0.y +\n\
						  (texture2DRect(tex, vec2(t0.x, t1.y)) * s0.x +\n\
						   texture2DRect(tex, vec2(t1.x, t1.y)) * s1.x) * s1.y;\n\
	}\n\
"};

static const char *FilterBicubicMitchellNetravaliFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[16];\n\
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
		gl_FragColor	= (texture2DRect(tex, texCoord[ 6]) * wx.r\n\
						+  texture2DRect(tex, texCoord[ 7]) * wx.g\n\
						+  texture2DRect(tex, texCoord[ 8]) * wx.b\n\
						+  texture2DRect(tex, texCoord[ 9]) * wx.a) * wy.r\n\
						+ (texture2DRect(tex, texCoord[ 5]) * wx.r\n\
						+  texture2DRect(tex, texCoord[ 0]) * wx.g\n\
						+  texture2DRect(tex, texCoord[ 1]) * wx.b\n\
						+  texture2DRect(tex, texCoord[10]) * wx.a) * wy.g\n\
						+ (texture2DRect(tex, texCoord[ 4]) * wx.r\n\
						+  texture2DRect(tex, texCoord[ 3]) * wx.g\n\
						+  texture2DRect(tex, texCoord[ 2]) * wx.b\n\
						+  texture2DRect(tex, texCoord[11]) * wx.a) * wy.b\n\
						+ (texture2DRect(tex, texCoord[15]) * wx.r\n\
						+  texture2DRect(tex, texCoord[14]) * wx.g\n\
						+  texture2DRect(tex, texCoord[13]) * wx.b\n\
						+  texture2DRect(tex, texCoord[12]) * wx.a) * wy.a;\n\
	}\n\
"};

static const char *FilterBicubicMitchellNetravaliFastFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[1];\n\
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
		gl_FragColor	= (texture2DRect(tex, vec2(t0.x, t0.y)) * s0.x +\n\
						   texture2DRect(tex, vec2(t1.x, t0.y)) * s1.x) * s0.y +\n\
						  (texture2DRect(tex, vec2(t0.x, t1.y)) * s0.x +\n\
						   texture2DRect(tex, vec2(t1.x, t1.y)) * s1.x) * s1.y;\n\
	}\n\
"};

static const char *FilterLanczos2FragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	#define PI 3.1415926535897932384626433832795\n\
	#define RADIUS 2.0\n\
	#define FIX(c) max(abs(c), 1e-5)\n\
	\n\
	varying vec2 texCoord[16];\n\
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
		gl_FragColor	= (texture2DRect(tex, texCoord[ 6]) * wx.r\n\
						+  texture2DRect(tex, texCoord[ 7]) * wx.g\n\
						+  texture2DRect(tex, texCoord[ 8]) * wx.b\n\
						+  texture2DRect(tex, texCoord[ 9]) * wx.a) * wy.r\n\
						+ (texture2DRect(tex, texCoord[ 5]) * wx.r\n\
						+  texture2DRect(tex, texCoord[ 0]) * wx.g\n\
						+  texture2DRect(tex, texCoord[ 1]) * wx.b\n\
						+  texture2DRect(tex, texCoord[10]) * wx.a) * wy.g\n\
						+ (texture2DRect(tex, texCoord[ 4]) * wx.r\n\
						+  texture2DRect(tex, texCoord[ 3]) * wx.g\n\
						+  texture2DRect(tex, texCoord[ 2]) * wx.b\n\
						+  texture2DRect(tex, texCoord[11]) * wx.a) * wy.b\n\
						+ (texture2DRect(tex, texCoord[15]) * wx.r\n\
						+  texture2DRect(tex, texCoord[14]) * wx.g\n\
						+  texture2DRect(tex, texCoord[13]) * wx.b\n\
						+  texture2DRect(tex, texCoord[12]) * wx.a) * wy.a;\n\
	}\n\
"};

static const char *FilterLanczos3FragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	#define PI 3.1415926535897932384626433832795\n\
	#define RADIUS 3.0\n\
	#define FIX(c) max(abs(c), 1e-5)\n\
	\n\
	// It would be nice to pass all 36 sample coordinates to the fragment\n\
	// shader, but since I want this to work on older GPUs, passing 36\n\
	// coordinates would result in 72 varying floats. Since most older GPUs\n\
	// can't support that many varying floats, I'm only going to pass the\n\
	// center coordinate, and then calculate the other sample coordinates\n\
	// from within the fragment shader.\n\
	//\n\
	// Note that sampling in this manner causes 36 dependent texel reads,\n\
	// which may incur a performance penalty. However, I think that having\n\
	// compatibility is better in this case, since any newer GPU (most from\n\
	// 2008 and later) will be fast enough to overcome the penalty.\n\
	\n\
	varying vec2 texCoord[1];\n\
	uniform sampler2DRect tex;\n\
	\n\
	vec3 weight3(float x)\n\
	{\n\
		vec3 sample = FIX(2.0 * PI * vec3(x - 1.5, x - 0.5, x + 0.5));\n\
		return ( sin(sample) * sin(sample / RADIUS) / (sample * sample) );\n\
	}\n\
	\n\
	void main()\n\
	{\n\
		vec2 tc = floor(texCoord[0] - 0.5) + 0.5;\n\
		vec2 f = abs(texCoord[0] - tc);\n\
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
		gl_FragColor	= (texture2DRect(tex, tc + vec2(-2.0,-2.0)) * wx1.r\n\
						+  texture2DRect(tex, tc + vec2(-1.0,-2.0)) * wx2.r\n\
						+  texture2DRect(tex, tc + vec2( 0.0,-2.0)) * wx1.g\n\
						+  texture2DRect(tex, tc + vec2( 1.0,-2.0)) * wx2.g\n\
						+  texture2DRect(tex, tc + vec2( 2.0,-2.0)) * wx1.b\n\
						+  texture2DRect(tex, tc + vec2( 3.0,-2.0)) * wx2.b) * wy1.r\n\
						+ (texture2DRect(tex, tc + vec2(-2.0,-1.0)) * wx1.r\n\
						+  texture2DRect(tex, tc + vec2(-1.0,-1.0)) * wx2.r\n\
						+  texture2DRect(tex, tc + vec2( 0.0,-1.0)) * wx1.g\n\
						+  texture2DRect(tex, tc + vec2( 1.0,-1.0)) * wx2.g\n\
						+  texture2DRect(tex, tc + vec2( 2.0,-1.0)) * wx1.b\n\
						+  texture2DRect(tex, tc + vec2( 3.0,-1.0)) * wx2.b) * wy2.r\n\
						+ (texture2DRect(tex, tc + vec2(-2.0, 0.0)) * wx1.r\n\
						+  texture2DRect(tex, tc + vec2(-1.0, 0.0)) * wx2.r\n\
						+  texture2DRect(tex, tc + vec2( 0.0, 0.0)) * wx1.g\n\
						+  texture2DRect(tex, tc + vec2( 1.0, 0.0)) * wx2.g\n\
						+  texture2DRect(tex, tc + vec2( 2.0, 0.0)) * wx1.b\n\
						+  texture2DRect(tex, tc + vec2( 3.0, 0.0)) * wx2.b) * wy1.g\n\
						+ (texture2DRect(tex, tc + vec2(-2.0, 1.0)) * wx1.r\n\
						+  texture2DRect(tex, tc + vec2(-1.0, 1.0)) * wx2.r\n\
						+  texture2DRect(tex, tc + vec2( 0.0, 1.0)) * wx1.g\n\
						+  texture2DRect(tex, tc + vec2( 1.0, 1.0)) * wx2.g\n\
						+  texture2DRect(tex, tc + vec2( 2.0, 1.0)) * wx1.b\n\
						+  texture2DRect(tex, tc + vec2( 3.0, 1.0)) * wx2.b) * wy2.g\n\
						+ (texture2DRect(tex, tc + vec2(-2.0, 2.0)) * wx1.r\n\
						+  texture2DRect(tex, tc + vec2(-1.0, 2.0)) * wx2.r\n\
						+  texture2DRect(tex, tc + vec2( 0.0, 2.0)) * wx1.g\n\
						+  texture2DRect(tex, tc + vec2( 1.0, 2.0)) * wx2.g\n\
						+  texture2DRect(tex, tc + vec2( 2.0, 2.0)) * wx1.b\n\
						+  texture2DRect(tex, tc + vec2( 3.0, 2.0)) * wx2.b) * wy1.b\n\
						+ (texture2DRect(tex, tc + vec2(-2.0, 3.0)) * wx1.r\n\
						+  texture2DRect(tex, tc + vec2(-1.0, 3.0)) * wx2.r\n\
						+  texture2DRect(tex, tc + vec2( 0.0, 3.0)) * wx1.g\n\
						+  texture2DRect(tex, tc + vec2( 1.0, 3.0)) * wx2.g\n\
						+  texture2DRect(tex, tc + vec2( 2.0, 3.0)) * wx1.b\n\
						+  texture2DRect(tex, tc + vec2( 3.0, 3.0)) * wx2.b) * wy2.b;\n\
	}\n\
"};

static const char *Scalar2xScanlineFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[1];\n\
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
		vec4 outA = texture2DRect(tex, texCoord[0]) * vec4(1.000, 1.000, 1.000, 1.000);\n\
		vec4 outB = texture2DRect(tex, texCoord[0]) * vec4(0.875, 0.875, 0.875, 1.000);\n\
		vec4 outC = texture2DRect(tex, texCoord[0]) * vec4(0.875, 0.875, 0.875, 1.000);\n\
		vec4 outD = texture2DRect(tex, texCoord[0]) * vec4(0.750, 0.750, 0.750, 1.000);\n\
		\n\
		gl_FragColor = mix( mix(outA, outB, f.x), mix(outC, outD, f.x), f.y );\n\
	}\n\
"};

static const char *Scalar2xEPXFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	\n\
	const vec3 dt = vec3(65536.0, 256.0, 1.0);\n\
	\n\
	float reduce(vec3 color)\n\
	{\n\
		return dot(color, dt);\n\
	}\n\
	\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  --|07|--\n\
	//                       05|00|01\n\
	//                       --|03|--\n\
	//\n\
	// Output Pixel Mapping:   A|B\n\
	//                         C|D\n\
	\n\
	void main()\n\
	{\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		vec3 pU = texture2DRect(tex, texCoord[7]).rgb;\n\
		vec3 pL = texture2DRect(tex, texCoord[5]).rgb;\n\
		vec3 pC = texture2DRect(tex, texCoord[0]).rgb;\n\
		vec3 pR = texture2DRect(tex, texCoord[1]).rgb;\n\
		vec3 pD = texture2DRect(tex, texCoord[3]).rgb;\n\
		float rU = reduce(pU);\n\
		float rL = reduce(pL);\n\
		float rR = reduce(pR);\n\
		float rD = reduce(pD);\n\
		\n\
		vec3 outA = pC;\n\
		vec3 outB = pC;\n\
		vec3 outC = pC;\n\
		vec3 outD = pC;\n\
		\n\
		if (rL != rR && rU != rD) \n\
		{\n\
			if (rU == rL) outA = pU;\n\
			if (rR == rU) outB = pR;\n\
			if (rL == rD) outC = pL;\n\
			if (rD == rR) outD = pD;\n\
		}\n\
		\n\
		gl_FragColor.rgb = mix( mix(outA, outB, f.x), mix(outC, outD, f.x), f.y );\n\
		gl_FragColor.a = 1.0;\n\
	}\n\
"};

static const char *Scalar2xEPXPlusFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[9];\n\
	uniform sampler2DRect tex;\n\
	//---------------------------------------\n\
	// Input Pixel Mapping:  --|07|--\n\
	//                       05|00|01\n\
	//                       --|03|--\n\
	//\n\
	// Output Pixel Mapping:   A|B\n\
	//                         C|D\n\
	\n\
	void main()\n\
	{\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		vec3 pU = texture2DRect(tex, texCoord[7]).rgb;\n\
		vec3 pL = texture2DRect(tex, texCoord[5]).rgb;\n\
		vec3 pC = texture2DRect(tex, texCoord[0]).rgb;\n\
		vec3 pR = texture2DRect(tex, texCoord[1]).rgb;\n\
		vec3 pD = texture2DRect(tex, texCoord[3]).rgb;\n\
		\n\
		vec3 outA = ( distance(pL, pU) < min(distance(pL, pD), distance(pR, pU)) ) ? mix(pL, pU, 0.5) : pC;\n\
		vec3 outB = ( distance(pR, pU) < min(distance(pL, pU), distance(pR, pD)) ) ? mix(pR, pU, 0.5) : pC;\n\
		vec3 outC = ( distance(pL, pD) < min(distance(pL, pU), distance(pR, pD)) ) ? mix(pL, pD, 0.5) : pC;\n\
		vec3 outD = ( distance(pR, pD) < min(distance(pL, pD), distance(pR, pU)) ) ? mix(pR, pD, 0.5) : pC;\n\
		\n\
		gl_FragColor.rgb = mix( mix(outA, outB, f.x), mix(outC, outD, f.x), f.y );\n\
		gl_FragColor.a = 1.0;\n\
	}\n\
"};

static const char *Scalar2xSaIFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[16];\n\
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
	// Output Pixel Mapping:     A|B\n\
	//                           C|D\n\
	\n\
	//---------------------------------------\n\
	// 2xSaI Pixel Mapping:    I|E|F|J\n\
	//                         G|A|B|K\n\
	//                         H|C|D|L\n\
	//                         M|N|O|P\n\
	\n\
	void main()\n\
	{\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		\n\
		float Iv = reduce( texture2DRect(tex, texCoord[6]).rgb );\n\
		float Ev = reduce( texture2DRect(tex, texCoord[7]).rgb );\n\
		float Fv = reduce( texture2DRect(tex, texCoord[8]).rgb );\n\
		float Jv = reduce( texture2DRect(tex, texCoord[9]).rgb );\n\
		\n\
		float Gv = reduce( texture2DRect(tex, texCoord[5]).rgb );\n\
		vec3 Ac = texture2DRect(tex, texCoord[0]).rgb; float Av = reduce(Ac);\n\
		vec3 Bc = texture2DRect(tex, texCoord[1]).rgb; float Bv = reduce(Bc);\n\
		float Kv = reduce( texture2DRect(tex, texCoord[10]).rgb );\n\
		\n\
		float Hv = reduce( texture2DRect(tex, texCoord[4]).rgb );\n\
		vec3 Cc = texture2DRect(tex, texCoord[3]).rgb; float Cv = reduce(Cc);\n\
		vec3 Dc = texture2DRect(tex, texCoord[2]).rgb; float Dv = reduce(Dc);\n\
		float Lv = reduce( texture2DRect(tex, texCoord[11]).rgb );\n\
		\n\
		float Mv = reduce( texture2DRect(tex, texCoord[15]).rgb );\n\
		float Nv = reduce( texture2DRect(tex, texCoord[14]).rgb );\n\
		float Ov = reduce( texture2DRect(tex, texCoord[13]).rgb );\n\
		// Pv is unused, so skip this one.\n\
		\n\
		vec3 outA = Ac;\n\
		vec3 outB = Ac;\n\
		vec3 outC = Ac;\n\
		vec3 outD = Ac;\n\
		\n\
		bool compAD = (Av == Dv);\n\
		bool compBC = (Bv == Cv);\n\
		\n\
		if (compAD && !compBC)\n\
		{\n\
			outB = ((Av == Ev) && (Bv == Lv)) || ((Av == Cv) && (Av == Fv) && (Bv != Ev) && (Bv == Jv)) ? Ac : mix(Ac, Bc, 0.5);\n\
			outC = ((Av == Gv) && (Cv == Ov)) || ((Av == Bv) && (Av == Hv) && (Cv != Gv) && (Cv == Mv)) ? Ac : mix(Ac, Cc, 0.5);\n\
		}\n\
		else if (!compAD && compBC)\n\
		{\n\
			outB = ((Bv == Fv) && (Av == Hv)) || ((Bv == Ev) && (Bv == Dv) && (Av != Fv) && (Av == Iv)) ? Bc : mix(Ac, Bc, 0.5);\n\
			outC = ((Cv == Hv) && (Av == Fv)) || ((Cv == Gv) && (Cv == Dv) && (Av != Hv) && (Av == Iv)) ? Cc : mix(Ac, Cc, 0.5);\n\
			outD = Bc;\n\
		}\n\
		else if (compAD && compBC)\n\
		{\n\
			outB = (Av == Bv) ? Ac : mix(Ac, Bc, 0.5);\n\
			outC = (Av == Bv) ? Ac : mix(Ac, Cc, 0.5);\n\
			\n\
			float r = (Av == Bv) ? 1.0 : GetResult(Av, Bv, Gv, Ev) - GetResult(Bv, Av, Kv, Fv) - GetResult(Bv, Av, Hv, Nv) + GetResult(Av, Bv, Lv, Ov);\n\
			outD = (r > 0.0) ? Ac : ( (r < 0.0) ? Bc : mix( mix(Ac, Bc, 0.5), mix(Cc, Dc, 0.5), 0.5) );\n\
		}\n\
		else\n\
		{\n\
			outB = ((Av == Cv) && (Av == Fv) && (Bv != Ev) && (Bv == Jv)) ? Ac : ( ((Bv == Ev) && (Bv == Dv) && (Av != Fv) && (Av == Iv)) ? Bc : mix(Ac, Bc, 0.5) );\n\
			outC = ((Av == Bv) && (Av == Hv) && (Cv != Gv) && (Cv == Mv)) ? Ac : ( ((Cv == Gv) && (Cv == Dv) && (Av != Hv) && (Av == Iv)) ? Cc : mix(Ac, Cc, 0.5) );\n\
			outD = mix( mix(Ac, Bc, 0.5), mix(Cc, Dc, 0.5), 0.5 );\n\
		}\n\
		\n\
		gl_FragColor.rgb = mix( mix(outA, outB, f.x), mix(outC, outD, f.x), f.y );\n\
		gl_FragColor.a = 1.0;\n\
	}\n\
"};

static const char *ScalarSuper2xSaIFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[16];\n\
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
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		\n\
		float Iv = reduce( texture2DRect(tex, texCoord[6]).rgb );\n\
		float Ev = reduce( texture2DRect(tex, texCoord[7]).rgb );\n\
		float Fv = reduce( texture2DRect(tex, texCoord[8]).rgb );\n\
		float Jv = reduce( texture2DRect(tex, texCoord[9]).rgb );\n\
		\n\
		float Gv = reduce( texture2DRect(tex, texCoord[5]).rgb );\n\
		vec3 Ac = texture2DRect(tex, texCoord[0]).rgb; float Av = reduce(Ac);\n\
		vec3 Bc = texture2DRect(tex, texCoord[1]).rgb; float Bv = reduce(Bc);\n\
		float Kv = reduce( texture2DRect(tex, texCoord[10]).rgb );\n\
		\n\
		float Hv = reduce( texture2DRect(tex, texCoord[4]).rgb );\n\
		vec3 Cc = texture2DRect(tex, texCoord[3]).rgb; float Cv = reduce(Cc);\n\
		vec3 Dc = texture2DRect(tex, texCoord[2]).rgb; float Dv = reduce(Dc);\n\
		float Lv = reduce( texture2DRect(tex, texCoord[11]).rgb );\n\
		\n\
		float Mv = reduce( texture2DRect(tex, texCoord[15]).rgb );\n\
		float Nv = reduce( texture2DRect(tex, texCoord[14]).rgb );\n\
		float Ov = reduce( texture2DRect(tex, texCoord[13]).rgb );\n\
		float Pv = reduce( texture2DRect(tex, texCoord[12]).rgb );\n\
		\n\
		bool compAD = (Av == Dv);\n\
		bool compBC = (Bv == Cv);\n\
		\n\
		vec3 outA = ( (compBC && !compAD && (Hv == Cv) && (Cv != Fv)) || ((Gv == Cv) && (Dv == Cv) && (Hv != Av) && (Cv != Iv)) ) ? mix(Ac, Cc, 0.5) : Ac;\n\
		vec3 outB = Bc;\n\
		vec3 outC = ( (compAD && !compBC && (Gv == Av) && (Av != Ov)) || ((Av == Hv) && (Av == Bv) && (Gv != Cv) && (Av != Mv)) ) ? mix(Ac, Cc, 0.5) : Cc;\n\
		vec3 outD = Dc;\n\
		\n\
		if (compBC && !compAD)\n\
		{\n\
			outB = outD = Cc;\n\
		}\n\
		else if (!compBC && compAD)\n\
		{\n\
			outB = outD = Ac;\n\
		}\n\
		else if (compBC && compAD)\n\
		{\n\
			float r = GetResult(Bv, Av, Hv, Nv) + GetResult(Bv, Av, Gv, Ev) + GetResult(Bv, Av, Ov, Lv) + GetResult(Bv, Av, Fv, Kv);\n\
			outB = outD = (r > 0.0) ? Bc : ( (r < 0.0) ? Ac : mix(Ac, Bc, 0.5) );\n\
		}\n\
		else\n\
		{\n\
			outB = ( (Bv == Dv) && (Bv == Ev) && (Av != Fv) && (Bv != Iv) ) ? mix(Ac, Bc, 0.75) : ( ( (Av == Cv) && (Av == Fv) && (Ev != Bv) && (Av != Jv) ) ? mix(Ac, Bc, 0.25) : mix(Ac, Bc, 0.5) );\n\
			outD = ( (Bv == Dv) && (Dv == Nv) && (Cv != Ov) && (Dv != Mv) ) ? mix(Cc, Dc, 0.75) : ( ( (Av == Cv) && (Cv == Ov) && (Nv != Dv) && (Cv != Pv) ) ? mix(Cc, Dc, 0.25) : mix(Cc, Dc, 0.5) );\n\
		}\n\
		\n\
		gl_FragColor.rgb = mix( mix(outA, outB, f.x), mix(outC, outD, f.x), f.y );\n\
		gl_FragColor.a = 1.0;\n\
	}\n\
"};

static const char *ScalarSuperEagle2xFragShader_110 = {"\
	#version 110\n\
	#extension GL_ARB_texture_rectangle : require\n\
	\n\
	varying vec2 texCoord[16];\n\
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
	// Output Pixel Mapping:     A|B\n\
	//                           C|D\n\
	\n\
	//---------------------------------------\n\
	// SEagle Pixel Mapping:   -|E|F|-\n\
	//                         G|A|B|K\n\
	//                         H|C|D|L\n\
	//                         -|N|O|-\n\
	\n\
	void main()\n\
	{\n\
		vec2 f = step(0.5, fract(texCoord[0]));\n\
		\n\
		float Ev = reduce( texture2DRect(tex, texCoord[7]).rgb );\n\
		float Fv = reduce( texture2DRect(tex, texCoord[8]).rgb );\n\
		\n\
		float Gv = reduce( texture2DRect(tex, texCoord[5]).rgb );\n\
		vec3 Ac = texture2DRect(tex, texCoord[0]).rgb; float Av = reduce(Ac);\n\
		vec3 Bc = texture2DRect(tex, texCoord[1]).rgb; float Bv = reduce(Bc);\n\
		float Kv = reduce( texture2DRect(tex, texCoord[10]).rgb );\n\
		\n\
		float Hv = reduce( texture2DRect(tex, texCoord[4]).rgb );\n\
		vec3 Cc = texture2DRect(tex, texCoord[3]).rgb; float Cv = reduce(Cc);\n\
		vec3 Dc = texture2DRect(tex, texCoord[2]).rgb; float Dv = reduce(Dc);\n\
		float Lv = reduce( texture2DRect(tex, texCoord[11]).rgb );\n\
		\n\
		float Nv = reduce( texture2DRect(tex, texCoord[14]).rgb );\n\
		float Ov = reduce( texture2DRect(tex, texCoord[13]).rgb );\n\
		\n\
		vec3 outA = Ac;\n\
		vec3 outB = Bc;\n\
		vec3 outC = Cc;\n\
		vec3 outD = Dc;\n\
		bool compAD = (Av == Dv);\n\
		bool compBC = (Bv == Cv);\n\
		\n\
		if (compBC && !compAD)\n\
		{\n\
			outA = (Cv == Hv || Bv == Fv) ? mix(Ac, Cc, 0.75) : mix(Ac, Bc, 0.5);\n\
			//outA = mix( mix(Ac, Bc, 0.5), mix(Ac, Cc, 0.75), float(Cv == Hv || Bv == Fv) );\n\
			outB = Cc;\n\
			//outC = Cc;\n\
			outD = mix( mix(Dc, Cc, 0.5), mix(Dc, Cc, 0.75), float(Bv == Kv || Cv == Nv) );\n\
		}\n\
		else if (!compBC && compAD)\n\
		{\n\
			//outA = Ac;\n\
			outB = mix( mix(Ac, Bc, 0.5), mix(Ac, Bc, 0.25), float(Av == Ev || Dv == Lv) );\n\
			outC = mix( mix(Cc, Dc, 0.5), mix(Ac, Cc, 0.25), float(Dv == Ov || Av == Gv) );\n\
			outD = Ac;\n\
		}\n\
		else if (compBC && compAD)\n\
		{\n\
			float r = GetResult(Bv, Av, Hv, Nv) + GetResult(Bv, Av, Gv, Ev) + GetResult(Bv, Av, Ov, Lv) + GetResult(Bv, Av, Fv, Kv);\n\
			if (r > 0.0)\n\
			{\n\
				outA = mix(Ac, Bc, 0.5);\n\
				outB = Cc;\n\
				//outC = Cc;\n\
				outD = outA;\n\
			}\n\
			else if (r < 0.0)\n\
			{\n\
				//outA = Ac;\n\
				outB = mix(Ac, Bc, 0.5);\n\
				outC = outB;\n\
				outD = Ac;\n\
			}\n\
			else\n\
			{\n\
				//outA = Ac;\n\
				outB = Cc;\n\
				//outC = Cc;\n\
				outD = Ac;\n\
			}\n\
		}\n\
		else\n\
		{\n\
			outA = mix(mix(Bc, Cc, 0.5), Ac, 0.75);\n\
			outB = mix(mix(Ac, Dc, 0.5), Bc, 0.75);\n\
			outC = mix(mix(Ac, Dc, 0.5), Cc, 0.75);\n\
			outD = mix(mix(Bc, Cc, 0.5), Dc, 0.75);\n\
		}\n\
		\n\
		gl_FragColor.rgb = mix( mix(outA, outB, f.x), mix(outC, outD, f.x), f.y );\n\
		gl_FragColor.a = 1.0;\n\
	}\n\
"};

enum OGLVertexAttributeID
{
	OGLVertexAttributeID_Position = 0,
	OGLVertexAttributeID_TexCoord0 = 8
};

static const GLint filterVtxBuffer[8] = {-1, -1, 1, -1, 1, 1, -1, 1};
static const GLubyte filterElementBuffer[6] = {0, 1, 2, 2, 3, 0};
static const GLubyte outputElementBuffer[12] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

OGLInfo::OGLInfo()
{
	_versionMajor = 0;
	_versionMinor = 0;
	_versionRevision = 0;
	
	_isVBOSupported = false;
	_isPBOSupported = false;
	_isShaderSupported = false;
	_isFBOSupported = false;
}

OGLInfo* OGLInfo::GetVersionedObjectOGL()
{
	OGLInfo *oglInfoObject = NULL;
	
	const char *oglVersionString = (const char *)glGetString(GL_VERSION);
	if (oglVersionString == NULL)
	{
		return oglInfoObject;
	}
	
	size_t versionStringLength = 0;
	
	// First, check for the dot in the revision string. There should be at
	// least one present.
	const char *versionStrEnd = strstr(oglVersionString, ".");
	if (versionStrEnd == NULL)
	{
		return oglInfoObject;
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
	
	// We finally have the version read. Now create an OGLInfo
	// object based on the OpenGL version.
	if ((major >= 4) ||
		(major >= 3 && minor >= 2))
	{
		oglInfoObject = new OGLInfo_3_2;
	}
	else if ((major >= 3) ||
			 (major >= 2 && minor >= 1))
	{
		oglInfoObject = new OGLInfo_2_1;
	}
	else if (major >= 2)
	{
		oglInfoObject = new OGLInfo_2_0;
	}
	else if (major >= 1 && minor >= 5)
	{
		oglInfoObject = new OGLInfo_1_2;
	}
	
	return oglInfoObject;
}

bool OGLInfo::IsVBOSupported()
{
	return this->_isVBOSupported;
}

bool OGLInfo::IsPBOSupported()
{
	return this->_isPBOSupported;
}

bool OGLInfo::IsShaderSupported()
{
	return this->_isShaderSupported;
}

bool OGLInfo::IsFBOSupported()
{
	return this->_isFBOSupported;
}

OGLInfo_1_2::OGLInfo_1_2()
{
	_versionMajor = 1;
	_versionMinor = 2;
	_versionRevision = 0;
	
	// Check the OpenGL capabilities for this renderer
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSetOGL(&oglExtensionSet);
	
	_isVBOSupported = this->IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_buffer_object");
	
#if !defined(GL_ARB_shader_objects)		|| \
    !defined(GL_ARB_vertex_shader)		|| \
    !defined(GL_ARB_fragment_shader)	|| \
    !defined(GL_ARB_vertex_program)
	
	_isShaderSupported	= false;
#else
	_isShaderSupported	= this->IsExtensionPresent(oglExtensionSet, "GL_ARB_shader_objects") &&
						  this->IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_shader") &&
						  this->IsExtensionPresent(oglExtensionSet, "GL_ARB_fragment_shader") &&
						  this->IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_program");
#endif
	
#if	!defined(GL_ARB_pixel_buffer_object) && !defined(GL_EXT_pixel_buffer_object)
	_isPBOSupported = false;
#else
	_isPBOSupported	= this->IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_buffer_object") &&
					 (this->IsExtensionPresent(oglExtensionSet, "GL_ARB_pixel_buffer_object") ||
					  this->IsExtensionPresent(oglExtensionSet, "GL_EXT_pixel_buffer_object"));
#endif
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)
	_isFBOSupported = false;
#else
	_isFBOSupported	= this->IsExtensionPresent(oglExtensionSet, "GL_EXT_framebuffer_object");
#endif
}

void OGLInfo_1_2::GetExtensionSetOGL(std::set<std::string> *oglExtensionSet)
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

bool OGLInfo_1_2::IsExtensionPresent(const std::set<std::string> &oglExtensionSet, const std::string &extensionName) const
{
	if (oglExtensionSet.size() == 0)
	{
		return false;
	}
	
	return (oglExtensionSet.find(extensionName) != oglExtensionSet.end());
}

OGLInfo_2_0::OGLInfo_2_0()
{
	_versionMajor = 2;
	_versionMinor = 0;
	_versionRevision = 0;
	_isVBOSupported = true;
	_isShaderSupported = true;
}

OGLInfo_2_1::OGLInfo_2_1()
{
	_versionMajor = 2;
	_versionMinor = 1;
	_versionRevision = 0;
	_isVBOSupported = true;
	_isPBOSupported = true;
	_isShaderSupported = true;
}

OGLInfo_3_2::OGLInfo_3_2()
{
	_versionMajor = 3;
	_versionMinor = 2;
	_versionRevision = 0;
	_isVBOSupported = true;
	_isPBOSupported = true;
	_isShaderSupported = true;
	_isFBOSupported = true;
}

void OGLInfo_3_2::GetExtensionSetOGL(std::set<std::string> *oglExtensionSet)
{
#ifdef GL_VERSION_3_2
	GLint extensionCount = 0;
	
	glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
	for (unsigned int i = 0; i < extensionCount; i++)
	{
		std::string extensionName = std::string((const char *)glGetStringi(GL_EXTENSIONS, i));
		oglExtensionSet->insert(extensionName);
	}
#endif
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

GLuint OGLShaderProgram::LoadShaderOGL(GLenum shaderType, const char *shaderProgram)
{
	GLint shaderStatus = GL_TRUE;
	
	GLuint shaderID = glCreateShader(shaderType);
	if (shaderID == 0)
	{
		printf("OpenGL Error - Failed to create %s.\n",
			   (shaderType == GL_VERTEX_SHADER) ? "GL_VERTEX_SHADER" : ((shaderType == GL_FRAGMENT_SHADER) ? "GL_FRAGMENT_SHADER" : "OTHER SHADER TYPE"));
		return shaderID;
	}
	
	glShaderSource(shaderID, 1, (const GLchar **)&shaderProgram, NULL);
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

GLuint OGLShaderProgram::GetVertexShaderID()
{
	return this->_vertexID;
}

void OGLShaderProgram::SetVertexShaderOGL(const char *shaderProgram)
{
	if (this->_vertexID != 0)
	{
		glDetachShader(this->_programID, this->_vertexID);
		glDeleteShader(this->_vertexID);
	}
	
	this->_vertexID = this->LoadShaderOGL(GL_VERTEX_SHADER, shaderProgram);
	
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

void OGLShaderProgram::SetFragmentShaderOGL(const char *shaderProgram)
{
	if (this->_fragmentID != 0)
	{
		glDetachShader(this->_programID, this->_fragmentID);
		glDeleteShader(this->_fragmentID);
	}
	
	this->_fragmentID = this->LoadShaderOGL(GL_FRAGMENT_SHADER, shaderProgram);
	
	if (this->_fragmentID != 0)
	{
		glAttachShader(this->_programID, this->_fragmentID);
	}
	
	if (this->_vertexID != 0 && this->_fragmentID != 0)
	{
		this->LinkOGL();
	}
}

void OGLShaderProgram::SetVertexAndFragmentShaderOGL(const char *vertShaderProgram, const char *fragShaderProgram)
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
	
	this->_vertexID = this->LoadShaderOGL(GL_VERTEX_SHADER, vertShaderProgram);
	this->_fragmentID = this->LoadShaderOGL(GL_FRAGMENT_SHADER, fragShaderProgram);
	
	if (this->_vertexID != 0)
	{
		glAttachShader(this->_programID, this->_vertexID);
		glBindAttribLocation(this->_programID, OGLVertexAttributeID_Position, "inPosition");
		glBindAttribLocation(this->_programID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	}
	
	if (this->_fragmentID != 0)
	{
		glAttachShader(this->_programID, this->_fragmentID);
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
	_info = OGLInfo::GetVersionedObjectOGL();
	_layerList = new std::vector<OGLVideoLayer *>;
	_layerList->reserve(8);
	
	// Render State Setup (common to both shaders and fixed-function pipeline)
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);
	glDisable(GL_STENCIL_TEST);
	
	// Set up fixed-function pipeline render states.
	if (!this->_info->IsShaderSupported())
	{
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_LIGHTING);
		glDisable(GL_FOG);
		glEnable(GL_TEXTURE_2D);
	}
	
	// Set up clear attributes
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

OGLVideoOutput::~OGLVideoOutput()
{
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		delete (*_layerList)[i];
	}
	
	delete _layerList;
	delete _info;
}

void OGLVideoOutput::InitLayers()
{
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		delete (*_layerList)[i];
	}
	
	this->_layerList->clear();
	this->_layerList->push_back(new OGLDisplayLayer(this));
}

OGLInfo* OGLVideoOutput::GetInfo()
{
	return this->_info;
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

void OGLVideoOutput::SetViewportSizeOGL(GLsizei w, GLsizei h)
{
	this->_viewportWidth = w;
	this->_viewportHeight = h;
	glViewport(0, 0, w, h);
	
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		(*_layerList)[i]->SetViewportSizeOGL(w, h);
	}
}

void OGLVideoOutput::ProcessOGL(const uint16_t *videoData, GLsizei w, GLsizei h)
{
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		(*_layerList)[i]->ProcessOGL(videoData, w, h);
	}
}

void OGLVideoOutput::RenderOGL()
{
	for (size_t i = 0; i < _layerList->size(); i++)
	{
		(*_layerList)[i]->RenderOGL();
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
	glDeleteFramebuffersEXT(1, &this->_fboID);
	glDeleteTextures(1, &this->_texDstID);
	glDeleteVertexArraysAPPLE(1, &this->_vaoID);
	glDeleteBuffers(1, &this->_vboVtxID);
	glDeleteBuffers(1, &this->_vboTexCoordID);
	glDeleteBuffers(1, &this->_vboElementID);
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
	_texCoordBuffer[4] = _srcWidth;
	_texCoordBuffer[5] = _srcHeight;
	_texCoordBuffer[6] = 0;
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
	glGenBuffers(1, &_vboElementID);
	
	glBindBuffer(GL_ARRAY_BUFFER, _vboVtxID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(filterVtxBuffer), filterVtxBuffer, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, _vboTexCoordID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(_texCoordBuffer), _texCoordBuffer, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vboElementID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(filterElementBuffer), filterElementBuffer, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glGenVertexArraysAPPLE(1, &_vaoID);
	glBindVertexArrayAPPLE(_vaoID);
	
	glBindBuffer(GL_ARRAY_BUFFER, _vboVtxID);
	glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_INT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, _vboTexCoordID);
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_INT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vboElementID);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	
	glBindVertexArrayAPPLE(0);
}

OGLShaderProgram* OGLFilter::GetProgram()
{
	return this->_program;
}

GLuint OGLFilter::GetDstTexID()
{
	return this->_texDstID;
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

GLuint OGLFilter::RunFilterOGL(GLuint srcTexID, GLsizei viewportWidth, GLsizei viewportHeight)
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, this->_fboID);
	glBindVertexArrayAPPLE(this->_vaoID);
	glUseProgram(this->_program->GetProgramID());
	glViewport(0, 0, this->_dstWidth, this->_dstHeight);
	
	glClear(GL_COLOR_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTexID);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	glBindVertexArrayAPPLE(0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glViewport(0, 0, viewportWidth, viewportHeight);
	
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

OGLFilterDeposterize::OGLFilterDeposterize(GLsizei srcWidth, GLsizei srcHeight)
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
	
	_program->SetVertexShaderOGL(Sample3x3_VertShader_110);
	_program->SetFragmentShaderOGL(FilterDeposterizeFragShader_110);
}

OGLFilterDeposterize::~OGLFilterDeposterize()
{
	glDeleteTextures(1, &this->_texIntermediateID);
}

GLuint OGLFilterDeposterize::RunFilterOGL(GLuint srcTexID, GLsizei viewportWidth, GLsizei viewportHeight)
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, this->_fboID);
	glBindVertexArrayAPPLE(this->_vaoID);
	glUseProgram(this->_program->GetProgramID());
	glViewport(0, 0, this->_dstWidth, this->_dstHeight);
	
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, this->_texIntermediateID, 0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTexID);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, this->_texDstID, 0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texIntermediateID);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	glBindVertexArrayAPPLE(0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glViewport(0, 0, viewportWidth, viewportHeight);
	
	return this->GetDstTexID();
}

#pragma mark -

OGLDisplayLayer::OGLDisplayLayer(OGLVideoOutput *oglVO)
{
	_output = oglVO;
	_needUploadVertices = true;
	_useDeposterize = false;
	
	_displayMode = DS_DISPLAY_TYPE_DUAL;
	_displayOrder = DS_DISPLAY_ORDER_MAIN_FIRST;
	_displayOrientation = DS_DISPLAY_ORIENTATION_VERTICAL;
	
	_normalWidth = GPU_DISPLAY_WIDTH;
	_normalHeight = GPU_DISPLAY_HEIGHT*2.0 + (DS_DISPLAY_GAP*_gapScalar);
	_rotation = 0.0f;
	
	_vfSingle = new VideoFilter(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT, VideoFilterTypeID_None, 0);
	_vfDual = new VideoFilter(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT*2, VideoFilterTypeID_None, 2);
	_vf = _vfDual;
	
	_vfMasterDstBuffer = (uint32_t *)calloc(_vfDual->GetDstWidth() * _vfDual->GetDstHeight(), sizeof(uint32_t));
	_vfSingle->SetDstBufferPtr(_vfMasterDstBuffer);
	_vfDual->SetDstBufferPtr(_vfMasterDstBuffer);
	
	_displayTexFilter = GL_NEAREST;
	
	_vtxBufferOffset = 0;
	UpdateVertices();
	UpdateTexCoords(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT * 2);
	
	// Set up textures
	glGenTextures(1, &_texCPUFilterDstID);
	glGenTextures(1, &_texInputVideoDataID);
	_texOutputVideoDataID = _texInputVideoDataID;
	_texPrevOutputVideoDataID = _texInputVideoDataID;
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texCPUFilterDstID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);
	glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_ARB, _vfDual->GetDstWidth() * _vfDual->GetDstHeight() * sizeof(uint32_t), _vfMasterDstBuffer);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texInputVideoDataID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, _vfDual->GetSrcWidth(), _vfDual->GetSrcHeight(), 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, _vfDual->GetSrcBufferPtr());
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	// Set up VBOs
	glGenBuffersARB(1, &this->_vboVertexID);
	glGenBuffersARB(1, &this->_vboTexCoordID);
	glGenBuffersARB(1, &this->_vboElementID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLint) * (2 * 8), this->vtxBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLfloat) * (2 * 8), this->texCoordBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, this->_vboElementID);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(GLubyte) * 12, outputElementBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	
	// Set up VAO
	glGenVertexArraysAPPLE(1, &this->_vaoMainStatesID);
	glBindVertexArrayAPPLE(this->_vaoMainStatesID);
	
	if (this->_output->GetInfo()->IsShaderSupported())
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_INT, GL_FALSE, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, this->_vboElementID);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	else
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
		glVertexPointer(2, GL_INT, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
		glTexCoordPointer(2, GL_FLOAT, 0, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, this->_vboElementID);
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	
	glBindVertexArrayAPPLE(0);
	
	_canUseShaderOutput = this->_output->GetInfo()->IsShaderSupported();
	if (_canUseShaderOutput)
	{
		_finalOutputProgram = new OGLShaderProgram;
		_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110);
		
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
	
	_canUseShaderBasedFilters = (_canUseShaderOutput && _output->GetInfo()->IsFBOSupported());
	if (_canUseShaderBasedFilters)
	{
		_filterDeposterize = new OGLFilterDeposterize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT * 2);
		
		_shaderFilter = new OGLFilter(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT * 2, 1);
		OGLShaderProgram *shaderFilterProgram = _shaderFilter->GetProgram();
		shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110);
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

OGLDisplayLayer::~OGLDisplayLayer()
{
	glDeleteVertexArraysAPPLE(1, &this->_vaoMainStatesID);
	glDeleteBuffersARB(1, &this->_vboVertexID);
	glDeleteBuffersARB(1, &this->_vboTexCoordID);
	glDeleteBuffersARB(1, &this->_vboElementID);
	glDeleteTextures(1, &this->_texCPUFilterDstID);
	glDeleteTextures(1, &this->_texInputVideoDataID);
	
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
	
	delete this->_vfSingle;
	delete this->_vfDual;
	free(_vfMasterDstBuffer);
}

bool OGLDisplayLayer::GetFiltersPreferGPU()
{
	return this->_filtersPreferGPU;
}

void OGLDisplayLayer::SetFiltersPreferGPUOGL(bool preferGPU)
{
	this->_filtersPreferGPU = preferGPU;
}

int OGLDisplayLayer::GetMode()
{
	return this->_displayMode;
}

void OGLDisplayLayer::SetMode(int dispMode)
{
	this->_displayMode = dispMode;
	
	switch (dispMode)
	{
		case DS_DISPLAY_TYPE_MAIN:
			this->_vfSingle->SetDstBufferPtr(_vfMasterDstBuffer);
			this->_vf = this->_vfSingle;
			break;
			
		case DS_DISPLAY_TYPE_TOUCH:
			this->_vfSingle->SetDstBufferPtr(_vfMasterDstBuffer + (this->_vfSingle->GetDstWidth() * this->_vfSingle->GetDstHeight()) );
			this->_vf = this->_vfSingle;
			break;
			
		case DS_DISPLAY_TYPE_DUAL:
			this->_vf = this->_vfDual;
			break;
			
		default:
			break;
	}
	
	this->GetNormalSize(&this->_normalWidth, &this->_normalHeight);
	this->UpdateVertices();
}

int OGLDisplayLayer::GetOrientation()
{
	return this->_displayOrientation;
}

void OGLDisplayLayer::SetOrientation(int dispOrientation)
{
	this->_displayOrientation = dispOrientation;
	this->GetNormalSize(&this->_normalWidth, &this->_normalHeight);
	this->UpdateVertices();
}

GLfloat OGLDisplayLayer::GetGapScalar()
{
	return this->_gapScalar;
}

void OGLDisplayLayer::SetGapScalar(GLfloat theScalar)
{
	this->_gapScalar = theScalar;
	this->GetNormalSize(&this->_normalWidth, &this->_normalHeight);
	this->UpdateVertices();
}

GLfloat OGLDisplayLayer::GetRotation()
{
	return this->_rotation;
}

void OGLDisplayLayer::SetRotation(GLfloat theRotation)
{
	this->_rotation = theRotation;
}

bool OGLDisplayLayer::GetBilinear()
{
	return (this->_displayTexFilter == GL_LINEAR);
}

void OGLDisplayLayer::SetBilinear(bool useBilinear)
{
	this->_displayTexFilter = (useBilinear) ? GL_LINEAR : GL_NEAREST;
}

bool OGLDisplayLayer::GetSourceDeposterize()
{
	return this->_useDeposterize;
}

void OGLDisplayLayer::SetSourceDeposterize(bool useDeposterize)
{
	this->_useDeposterize = (this->_canUseShaderBasedFilters) ? useDeposterize : false;
}

int OGLDisplayLayer::GetOrder()
{
	return this->_displayOrder;
}

void OGLDisplayLayer::SetOrder(int dispOrder)
{
	this->_displayOrder = dispOrder;
	
	if (this->_displayOrder == DS_DISPLAY_ORDER_MAIN_FIRST)
	{
		this->_vtxBufferOffset = 0;
	}
	else // dispOrder == DS_DISPLAY_ORDER_TOUCH_FIRST
	{
		this->_vtxBufferOffset = (2 * 8);
	}
	
	this->_needUploadVertices = true;
}

void OGLDisplayLayer::UpdateVertices()
{
	const GLfloat w = GPU_DISPLAY_WIDTH;
	const GLfloat h = GPU_DISPLAY_HEIGHT;
	const GLfloat gap = DS_DISPLAY_GAP * this->_gapScalar / 2.0;
	
	if (this->_displayMode == DS_DISPLAY_TYPE_DUAL)
	{
		// displayOrder == DS_DISPLAY_ORDER_MAIN_FIRST
		if (this->_displayOrientation == DS_DISPLAY_ORIENTATION_VERTICAL)
		{
			vtxBuffer[0]	= -w/2;			vtxBuffer[1]		= h+gap;	// Top display, top left
			vtxBuffer[2]	= w/2;			vtxBuffer[3]		= h+gap;	// Top display, top right
			vtxBuffer[4]	= w/2;			vtxBuffer[5]		= gap;		// Top display, bottom right
			vtxBuffer[6]	= -w/2;			vtxBuffer[7]		= gap;		// Top display, bottom left
			
			vtxBuffer[8]	= -w/2;			vtxBuffer[9]		= -gap;		// Bottom display, top left
			vtxBuffer[10]	= w/2;			vtxBuffer[11]		= -gap;		// Bottom display, top right
			vtxBuffer[12]	= w/2;			vtxBuffer[13]		= -(h+gap);	// Bottom display, bottom right
			vtxBuffer[14]	= -w/2;			vtxBuffer[15]		= -(h+gap);	// Bottom display, bottom left
		}
		else // displayOrientationID == DS_DISPLAY_ORIENTATION_HORIZONTAL
		{
			vtxBuffer[0]	= -(w+gap);		vtxBuffer[1]		= h/2;		// Left display, top left
			vtxBuffer[2]	= -gap;			vtxBuffer[3]		= h/2;		// Left display, top right
			vtxBuffer[4]	= -gap;			vtxBuffer[5]		= -h/2;		// Left display, bottom right
			vtxBuffer[6]	= -(w+gap);		vtxBuffer[7]		= -h/2;		// Left display, bottom left
			
			vtxBuffer[8]	= gap;			vtxBuffer[9]		= h/2;		// Right display, top left
			vtxBuffer[10]	= w+gap;		vtxBuffer[11]		= h/2;		// Right display, top right
			vtxBuffer[12]	= w+gap;		vtxBuffer[13]		= -h/2;		// Right display, bottom right
			vtxBuffer[14]	= gap;			vtxBuffer[15]		= -h/2;		// Right display, bottom left
		}
		
		// displayOrder == DS_DISPLAY_ORDER_TOUCH_FIRST
		memcpy(vtxBuffer + (2 * 8), vtxBuffer + (1 * 8), sizeof(GLint) * (1 * 8));
		memcpy(vtxBuffer + (3 * 8), vtxBuffer + (0 * 8), sizeof(GLint) * (1 * 8));
	}
	else // displayModeID == DS_DISPLAY_TYPE_MAIN || displayModeID == DS_DISPLAY_TYPE_TOUCH
	{
		vtxBuffer[0]	= -w/2;		vtxBuffer[1]		= h/2;		// First display, top left
		vtxBuffer[2]	= w/2;		vtxBuffer[3]		= h/2;		// First display, top right
		vtxBuffer[4]	= w/2;		vtxBuffer[5]		= -h/2;		// First display, bottom right
		vtxBuffer[6]	= -w/2;		vtxBuffer[7]		= -h/2;		// First display, bottom left
		
		memcpy(vtxBuffer + (1 * 8), vtxBuffer + (0 * 8), sizeof(GLint) * (1 * 8));	// Second display
		memcpy(vtxBuffer + (2 * 8), vtxBuffer + (0 * 8), sizeof(GLint) * (2 * 8));	// Second display
	}
	
	this->_needUploadVertices = true;
}

void OGLDisplayLayer::UpdateTexCoords(GLfloat s, GLfloat t)
{
	texCoordBuffer[0]	= 0.0f;		texCoordBuffer[1]	=   0.0f;
	texCoordBuffer[2]	=    s;		texCoordBuffer[3]	=   0.0f;
	texCoordBuffer[4]	=    s;		texCoordBuffer[5]	= t/2.0f;
	texCoordBuffer[6]	= 0.0f;		texCoordBuffer[7]	= t/2.0f;
	
	texCoordBuffer[8]	= 0.0f;		texCoordBuffer[9]	= t/2.0f;
	texCoordBuffer[10]	=    s;		texCoordBuffer[11]	= t/2.0f;
	texCoordBuffer[12]	=    s;		texCoordBuffer[13]	=      t;
	texCoordBuffer[14]	= 0.0f;		texCoordBuffer[15]	=      t;
}

bool OGLDisplayLayer::CanUseShaderBasedFilters()
{
	return this->_canUseShaderBasedFilters;
}

void OGLDisplayLayer::GetNormalSize(double *w, double *h)
{
	if (w == NULL || h == NULL)
	{
		return;
	}
	
	if (this->_displayMode != DS_DISPLAY_TYPE_DUAL)
	{
		*w = GPU_DISPLAY_WIDTH;
		*h = GPU_DISPLAY_HEIGHT;
		return;
	}
	
	if (this->_displayOrientation == DS_DISPLAY_ORIENTATION_VERTICAL)
	{
		*w = GPU_DISPLAY_WIDTH;
		*h = GPU_DISPLAY_HEIGHT * 2.0 + (DS_DISPLAY_GAP * this->_gapScalar);
	}
	else
	{
		*w = GPU_DISPLAY_WIDTH * 2.0 + (DS_DISPLAY_GAP * this->_gapScalar);
		*h = GPU_DISPLAY_HEIGHT;
	}
}

void OGLDisplayLayer::UploadVerticesOGL()
{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(GLint) * (2 * 8), this->vtxBuffer + this->_vtxBufferOffset);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	this->_needUploadVertices = false;
}

void OGLDisplayLayer::UploadTexCoordsOGL()
{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(GLfloat) * (2 * 8), this->texCoordBuffer);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void OGLDisplayLayer::UploadTransformationOGL()
{
	const double w = this->_viewportWidth;
	const double h = this->_viewportHeight;
	const CGSize checkSize = GetTransformedBounds(this->_normalWidth, this->_normalHeight, 1.0, this->_rotation);
	const GLdouble s = GetMaxScalarInBounds(checkSize.width, checkSize.height, w, h);
	
	if (this->_output->GetInfo()->IsShaderSupported())
	{
		glUniform2f(this->_uniformFinalOutputViewSize, w, h);
		glUniform1f(this->_uniformFinalOutputAngleDegrees, this->_rotation);
		glUniform1f(this->_uniformFinalOutputScalar, s);
	}
	else
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-w/2.0, -w/2.0 + w, -h/2.0, -h/2.0 + h, -1.0, 1.0);
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(CLOCKWISE_DEGREES(this->_rotation), 0.0f, 0.0f, 1.0f);
		glScalef(s, s, 1.0f);
	}
}

int OGLDisplayLayer::GetOutputFilter()
{
	return this->_outputFilter;
}

void OGLDisplayLayer::SetOutputFilterOGL(const int filterID)
{
	this->_displayTexFilter = GL_NEAREST;
	
	if (this->_canUseShaderBasedFilters)
	{
		this->_outputFilter = filterID;
		
		switch (filterID)
		{
			case OutputFilterTypeID_NearestNeighbor:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110);
				break;
				
			case OutputFilterTypeID_Bilinear:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110);
				this->_displayTexFilter = GL_LINEAR;
				break;
				
			case OutputFilterTypeID_BicubicBSpline:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterBicubicBSplineFragShader_110);
				break;
				
			case OutputFilterTypeID_BicubicMitchell:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterBicubicMitchellNetravaliFragShader_110);
				break;
				
			case OutputFilterTypeID_Lanczos2:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(BicubicSample4x4Output_VertShader_110, FilterLanczos2FragShader_110);
				break;
				
			case OutputFilterTypeID_Lanczos3:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, FilterLanczos3FragShader_110);
				break;
				
			default:
				this->_finalOutputProgram->SetVertexAndFragmentShaderOGL(Sample1x1OutputVertShader_100, PassthroughOutputFragShader_110);
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

int OGLDisplayLayer::GetPixelScaler()
{
	return this->_pixelScaler;
}

void OGLDisplayLayer::SetPixelScalerOGL(const int filterID)
{
	const char *cpuTypeIDString = VideoFilter::GetTypeStringByID((VideoFilterTypeID)filterID);
	if (strstr(cpuTypeIDString, VIDEOFILTERTYPE_UNKNOWN_STRING) == NULL)
	{
		this->SetCPUFilterOGL((VideoFilterTypeID)filterID);
		this->_pixelScaler = filterID;
		
		if (this->_canUseShaderBasedFilters)
		{
			OGLShaderProgram *shaderFilterProgram = _shaderFilter->GetProgram();
			this->_useShaderBasedPixelScaler = true;
			
			VideoFilterAttributes vfAttr = VideoFilter::GetAttributesByID((VideoFilterTypeID)filterID);
			GLfloat vfScale = (GLfloat)vfAttr.scaleMultiply / (GLfloat)vfAttr.scaleDivide;
			
			switch (filterID)
			{
				case VideoFilterTypeID_Nearest1_5X:
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110);
					break;
					
				case VideoFilterTypeID_Nearest2X:
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, PassthroughFragShader_110);
					break;
					
				case VideoFilterTypeID_Scanline:
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample1x1_VertShader_110, Scalar2xScanlineFragShader_110);
					break;
					
				case VideoFilterTypeID_EPX:
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, Scalar2xEPXFragShader_110);
					break;
					
				case VideoFilterTypeID_EPXPlus:
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample3x3_VertShader_110, Scalar2xEPXPlusFragShader_110);
					break;
					
				case VideoFilterTypeID_2xSaI:
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, Scalar2xSaIFragShader_110);
					break;
					
				case VideoFilterTypeID_Super2xSaI:
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, ScalarSuper2xSaIFragShader_110);
					break;
					
				case VideoFilterTypeID_SuperEagle:
					shaderFilterProgram->SetVertexAndFragmentShaderOGL(Sample4x4_VertShader_110, ScalarSuperEagle2xFragShader_110);
					break;
					
				default:
					this->_useShaderBasedPixelScaler = false;
					break;
			}
			
			if (this->_useShaderBasedPixelScaler)
			{
				_shaderFilter->SetScaleOGL(vfScale);
			}
		}
	}
	else
	{
		this->SetCPUFilterOGL(VideoFilterTypeID_None);
		this->_pixelScaler = VideoFilterTypeID_None;
		this->_useShaderBasedPixelScaler = false;
	}
}

void OGLDisplayLayer::SetCPUFilterOGL(const VideoFilterTypeID videoFilterTypeID)
{
	bool needResizeTexture = false;
	const VideoFilterAttributes newFilterAttr = VideoFilter::GetAttributesByID(videoFilterTypeID);
	const size_t oldDstBufferWidth = this->_vfDual->GetDstWidth();
	const size_t oldDstBufferHeight = this->_vfDual->GetDstHeight();
	const GLsizei newDstBufferWidth = GPU_DISPLAY_WIDTH * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	const GLsizei newDstBufferHeight = GPU_DISPLAY_HEIGHT * 2 * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	
	if (oldDstBufferWidth != newDstBufferWidth || oldDstBufferHeight != newDstBufferHeight)
	{
		needResizeTexture = true;
	}
	
	if (needResizeTexture)
	{
		uint32_t *oldMasterBuffer = _vfMasterDstBuffer;
		uint32_t *newMasterBuffer = (uint32_t *)calloc(newDstBufferWidth * newDstBufferHeight, sizeof(uint32_t));
		
		this->_vfDual->SetDstBufferPtr(newMasterBuffer);
		
		switch (this->_displayMode)
		{
			case DS_DISPLAY_TYPE_MAIN:
			case DS_DISPLAY_TYPE_DUAL:
				this->_vfSingle->SetDstBufferPtr(newMasterBuffer);
				break;
				
			case DS_DISPLAY_TYPE_TOUCH:
				this->_vfSingle->SetDstBufferPtr(newMasterBuffer + (this->_vfSingle->GetDstWidth() * this->_vfSingle->GetDstHeight()) );
				break;
				
			default:
				break;
		}
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID);
		glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_ARB, newDstBufferWidth * newDstBufferHeight * sizeof(uint32_t), newMasterBuffer);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);
		
		glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, newDstBufferWidth, newDstBufferHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, newMasterBuffer);
		glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
		
		_vfMasterDstBuffer = newMasterBuffer;
		free(oldMasterBuffer);
	}
	
	this->_vfSingle->ChangeFilterByID(videoFilterTypeID);
	this->_vfDual->ChangeFilterByID(videoFilterTypeID);
}

void OGLDisplayLayer::ProcessOGL(const uint16_t *videoData, GLsizei w, GLsizei h)
{
	VideoFilter *currentFilter = this->_vf;
	GLint lineOffset = (this->_displayMode == DS_DISPLAY_TYPE_TOUCH) ? h : 0;
	
	// Determine whether we should take CPU-based path or a GPU-based path
	if	(this->_pixelScaler != VideoFilterTypeID_None && !(this->_useShaderBasedPixelScaler && this->_filtersPreferGPU) )
	{
		if (!this->_useDeposterize) // Pure CPU-based path
		{
			RGB555ToBGRA8888Buffer((const uint16_t *)videoData, (uint32_t *)currentFilter->GetSrcBufferPtr(), w * h);
		}
		else // Hybrid CPU/GPU-based path (may cause a performance hit on pixel download)
		{
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texInputVideoDataID);
			glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, lineOffset, w, h, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, videoData);
			
			this->_filterDeposterize->RunFilterOGL(this->_texInputVideoDataID, this->_viewportWidth, this->_viewportHeight);
			
			const GLsizei readLineCount = (this->_displayMode == DS_DISPLAY_TYPE_DUAL) ? GPU_DISPLAY_HEIGHT * 2 : GPU_DISPLAY_HEIGHT;
			this->_filterDeposterize->DownloadDstBufferOGL(currentFilter->GetSrcBufferPtr(), lineOffset, readLineCount);
		}
		
		uint32_t *texData = currentFilter->RunFilter();
		w = currentFilter->GetDstWidth();
		h = currentFilter->GetDstHeight();
		lineOffset = (this->_displayMode == DS_DISPLAY_TYPE_TOUCH) ? h : 0;
		
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texCPUFilterDstID);
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, lineOffset, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, texData);
		
		this->_texOutputVideoDataID = this->_texCPUFilterDstID;
		this->UpdateTexCoords(w, (this->_displayMode == DS_DISPLAY_TYPE_DUAL) ? h : h*2);
	}
	else // Pure GPU-based path
	{
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texInputVideoDataID);
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, lineOffset, w, h, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, videoData);
		
		if (this->_useDeposterize)
		{
			this->_texOutputVideoDataID = this->_filterDeposterize->RunFilterOGL(this->_texInputVideoDataID, this->_viewportWidth, this->_viewportHeight);
		}
		else
		{
			this->_texOutputVideoDataID = this->_texInputVideoDataID;
		}
		
		if (this->_displayMode != DS_DISPLAY_TYPE_DUAL)
		{
			h *= 2;
		}
		
		if (this->_useShaderBasedPixelScaler)
		{
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texOutputVideoDataID);
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			
			this->_texOutputVideoDataID = this->_shaderFilter->RunFilterOGL(this->_texOutputVideoDataID, this->_viewportWidth, this->_viewportHeight);
			w = this->_shaderFilter->GetDstWidth();
			h = this->_shaderFilter->GetDstHeight();
		}
		
		this->UpdateTexCoords(w, h);
	}
	
	this->UploadTexCoordsOGL();
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
}

void OGLDisplayLayer::RenderOGL()
{
	glUseProgram(this->_finalOutputProgram->GetProgramID());
	this->UploadTransformationOGL();
	
	if (_needUploadVertices)
	{
		this->UploadVerticesOGL();
	}
	
	// Enable vertex attributes
	glBindVertexArrayAPPLE(this->_vaoMainStatesID);
	
	const GLsizei vtxElementCount = (this->_displayMode == DS_DISPLAY_TYPE_DUAL) ? 12 : 6;
	const GLubyte *elementPointer = !(this->_displayMode == DS_DISPLAY_TYPE_TOUCH) ? 0 : (GLubyte *)(vtxElementCount * sizeof(GLubyte));
	
	glClear(GL_COLOR_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->_texOutputVideoDataID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter);
	glDrawElements(GL_TRIANGLES, vtxElementCount, GL_UNSIGNED_BYTE, elementPointer);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	
	// Disable vertex attributes
	glBindVertexArrayAPPLE(0);
}
