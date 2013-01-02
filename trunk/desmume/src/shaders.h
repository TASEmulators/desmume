/*
	Copyright (C) 2008-2012 DeSmuME team

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

/* Predefined OpenGL shaders */

/* Vertex shader */
const char *vertexShader = {"\
	attribute vec4 inPosition; \n\
	attribute vec2 inTexCoord0; \n\
	attribute vec3 inColor; \n\
	\n\
	uniform float polyAlpha; \n\
	uniform vec2 texScale; \n\
	\n\
	varying vec4 vtxPosition; \n\
	varying vec2 vtxTexCoord; \n\
	varying vec4 vtxColor; \n\
	\n\
	void main() \n\
	{ \n\
		// Keep the projection matrix as a placeholder in case we need to use one in the future. \n\
		mat4 projectionMtx	= mat4(	vec4(1.0, 0.0, 0.0, 0.0), \n\
									vec4(0.0, 1.0, 0.0, 0.0), \n\
									vec4(0.0, 0.0, 1.0, 0.0), \n\
									vec4(0.0, 0.0, 0.0, 1.0));\n\
		\n\
		mat2 texScaleMtx	= mat2(	vec2(texScale.x,        0.0), \n\
									vec2(       0.0, texScale.y)); \n\
		\n\
		vtxPosition = projectionMtx * inPosition; \n\
		vtxTexCoord = texScaleMtx * inTexCoord0; \n\
		vtxColor = vec4(inColor * 4.0, polyAlpha); \n\
		\n\
		gl_Position = vtxPosition; \n\
	} \n\
"};

/* Fragment shader */
const char *fragmentShader = {"\
	uniform sampler1D toonTable; \n\
	uniform sampler2D tex2d; \n\
	uniform bool hasTexture; \n\
	uniform int texBlending; \n\
	uniform int oglWBuffer; \n\
	uniform bool enableAlphaTest; \n\
	uniform float alphaTestRef; \n\
	\n\
	varying vec4 vtxPosition; \n\
	varying vec2 vtxTexCoord; \n\
	varying vec4 vtxColor; \n\
	\n\
	void main() \n\
	{ \n\
		vec4 texColor = vec4(1.0, 1.0, 1.0, 1.0); \n\
		vec4 flagColor; \n\
		\n\
		if(hasTexture) \n\
		{ \n\
			texColor = texture2D(tex2d, vtxTexCoord); \n\
		} \n\
		\n\
		flagColor = texColor; \n\
		\n\
		if(texBlending == 0) \n\
		{ \n\
			flagColor = vtxColor * texColor; \n\
		} \n\
		else if(texBlending == 1) \n\
		{ \n\
			if (texColor.a == 0.0 || !hasTexture) \n\
			{ \n\
				flagColor.rgb = vtxColor.rgb; \n\
			} \n\
			else if (texColor.a == 1.0) \n\
			{ \n\
				flagColor.rgb = texColor.rgb; \n\
			} \n\
			else \n\
			{ \n\
				flagColor.rgb = texColor.rgb * (1.0-texColor.a) + vtxColor.rgb * texColor.a;\n\
			} \n\
			\n\
			flagColor.a = vtxColor.a; \n\
		} \n\
		else if(texBlending == 2) \n\
		{ \n\
			vec3 toonColor = vec3(texture1D(toonTable, vtxColor.r).rgb); \n\
			flagColor.rgb = texColor.rgb * toonColor.rgb;\n\
			flagColor.a = texColor.a * vtxColor.a;\n\
		} \n\
		else if(texBlending == 3) \n\
		{ \n\
			vec3 toonColor = vec3(texture1D(toonTable, vtxColor.r).rgb); \n\
			flagColor.rgb = texColor.rgb * vtxColor.rgb + toonColor.rgb; \n\
			flagColor.a = texColor.a * vtxColor.a; \n\
		} \n\
		\n\
		if (flagColor.a == 0.0 || (enableAlphaTest && flagColor.a < alphaTestRef)) \n\
		{ \n\
			discard; \n\
		} \n\
		\n\
		if (oglWBuffer == 1) \n\
		{ \n\
			// TODO \n\
			gl_FragDepth = (vtxPosition.z / vtxPosition.w) * 0.5 + 0.5; \n\
		} \n\
		else \n\
		{ \n\
			gl_FragDepth = (vtxPosition.z / vtxPosition.w) * 0.5 + 0.5; \n\
		} \n\
		\n\
		gl_FragColor = flagColor; \n\
	} \n\
"};

