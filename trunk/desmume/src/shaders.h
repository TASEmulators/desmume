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
	uniform float polyAlpha; \n\
	uniform vec2 texScale; \n\
	varying vec4 pos; \n\
	varying vec4 vtxColor; \n\
	void main() \n\
	{ \n\
		mat4 projectionMtx	= mat4(	vec4(1.0, 0.0, 0.0, 0.0), \n\
									vec4(0.0, 1.0, 0.0, 0.0), \n\
									vec4(0.0, 0.0, 1.0, 0.0), \n\
									vec4(0.0, 0.0, 0.0, 1.0));\n\
		\n\
		mat4 texScaleMtx	= mat4(	vec4(texScale.x,        0.0, 0.0, 0.0), \n\
									vec4(       0.0, texScale.y, 0.0, 0.0), \n\
									vec4(       0.0,        0.0, 1.0, 0.0), \n\
									vec4(       0.0,        0.0, 0.0, 1.0)); \n\
		\n\
		vtxColor = vec4(gl_Color.rgb * 4.0, polyAlpha); \n\
		gl_Position = projectionMtx * gl_Vertex; \n\
		gl_TexCoord[0] = texScaleMtx * gl_MultiTexCoord0; \n\
		gl_FrontColor = vtxColor; \n\
		pos = gl_Position; \n\
	} \n\
"};

/* Fragment shader */
const char *fragmentShader = {"\
	uniform sampler1D toonTable; \n\
	uniform sampler2D tex2d; \n\
	uniform int hasTexture; \n\
	uniform int texBlending; \n\
	uniform int oglWBuffer; \n\
	varying vec4 pos; \n\
	varying vec4 vtxColor; \n\
	void main() \n\
	{ \n\
		vec4 texColor = vec4(1.0, 1.0, 1.0, 1.0); \n\
		vec4 flagColor; \n\
		\
		if(hasTexture != 0) \n\
		{ \n\
			texColor = texture2D(tex2d, gl_TexCoord[0].st); \n\
		} \n\
		flagColor = texColor; \n\
		if(texBlending == 0) \n\
		{ \n\
			flagColor = vtxColor * texColor; \n\
		} \n\
		else \n\
			if(texBlending == 1) \n\
			{ \n\
				if (texColor.a == 0.0 || hasTexture == 0) \n\
					flagColor.rgb = vtxColor.rgb;\n\
				else \n\
					if (texColor.a == 1.0) \n\
						flagColor.rgb = texColor.rgb;\n\
					else \n\
					flagColor.rgb = texColor.rgb * (1.0-texColor.a) + vtxColor.rgb * texColor.a;\n\
				flagColor.a = vtxColor.a; \n\
			} \n\
			else \n\
				if(texBlending == 2) \n\
				{ \n\
					vec3 toonColor = vec3(texture1D(toonTable, vtxColor.r).rgb); \n\
					flagColor.rgb = texColor.rgb * toonColor.rgb;\n\
					flagColor.a = texColor.a * vtxColor.a;\n\
				} \n\
				else \n\
					if(texBlending == 3) \n\
					{ \n\
						vec3 toonColor = vec3(texture1D(toonTable, vtxColor.r).rgb); \n\
						flagColor.rgb = texColor.rgb * vtxColor.rgb + toonColor.rgb; \n\
						flagColor.a = texColor.a * vtxColor.a; \n\
					} \n\
		if (oglWBuffer == 1) \n\
			// TODO \n\
			gl_FragDepth = (pos.z / pos.w) * 0.5 + 0.5; \n\
		else \n\
			gl_FragDepth = (pos.z / pos.w) * 0.5 + 0.5; \n\
		gl_FragColor = flagColor; \n\
	} \n\
"};

