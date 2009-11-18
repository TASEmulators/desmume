/* Predefined OpenGL shaders */

/* Vertex shader */
const char *vertexShader = {"\
	void main() \n\
	{ \n\
		gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex; \n\
		gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0; \n\
		gl_FrontColor = gl_Color; \n\
	} \n\
"};

/* Fragment shader */
const char *fragmentShader = {"\
	uniform sampler1D toonTable; \n\
	uniform sampler2D tex2d; \n\
	uniform int hasTexture; \n\
	uniform int texBlending; \n\
	\n\
	vec4 float_to_6bit(in vec4 color) \n\
	{ \n\
		vec4 ret = color * vec4(31.0,31.0,31.0,31.0);\n\
		\n\
		if(ret.r > 0.0) ret.r = (ret.r * 2.0) + 1.0; \n\
		if(ret.g > 0.0) ret.g = (ret.g * 2.0) + 1.0; \n\
		if(ret.b > 0.0) ret.b = (ret.b * 2.0) + 1.0; \n\
		if(ret.a > 0.0) ret.a = (ret.a * 2.0) + 1.0; \n\
		\n\
		return ret; \n\
	} \n\
	\n\
	void main() \n\
	{ \n\
		vec4 vtxColor = float_to_6bit(gl_Color); \n\
		vec4 texColor = float_to_6bit(texture2D(tex2d, gl_TexCoord[0].st)); \n\
		vec3 toonColor = vec3(float_to_6bit(vec4(texture1D(toonTable, gl_Color.r).rgb, 0.0))); \n\
		vec4 fragColor = vec4(0.0, 0.0, 0.0, 0.0); \n\
		\n\
		if(hasTexture == 0) \n\
		{ \n\
			texColor = vec4(63.0, 63.0, 63.0, 63.0); \n\
		} \n\
		\n\
		if(texBlending == 0) \n\
		{ \n\
			fragColor = ((texColor + 1.0) * (vtxColor + 1.0) - 1.0) / 64.0; \n\
		} \n\
		else if(texBlending == 1) \n\
		{ \n\
			if(texColor.a == 0.0 || hasTexture == 0) \n\
			{ \n\
				fragColor.rgb = vtxColor.rgb; \n\
			} \n\
			else if(texColor.a == 63.0) \n\
			{ \n\
				fragColor.rgb = texColor.rgb; \n\
			} \n\
			else \n\
			{ \n\
				fragColor.rgb = ((texColor.rgb * texColor.a) + (vtxColor.rgb * (63.0 - texColor.a))) / 64.0; \n\
			} \n\
			\n\
			fragColor.a = vtxColor.a; \n\
		} \n\
		else if(texBlending == 2) \n\
		{ \n\
			fragColor.rgb = ((texColor.rgb + 1.0) * (toonColor + 1.0) - 1.0) / 64.0; \n\
			fragColor.a = ((texColor.a + 1.0) * (vtxColor.a + 1.0) - 1.0) / 64.0; \n\
		} \n\
		else if(texBlending == 3) \n\
		{ \n\
			fragColor.rgb = min((((texColor.rgb + 1.0) * (toonColor + 1.0) - 1.0) / 64.0) + toonColor, 63.0); \n\
			fragColor.a = ((texColor.a + 1.0) * (vtxColor.a + 1.0) - 1.0) / 64.0; \n\
		} \n\
		\n\
		gl_FragColor = ((fragColor - 1.0) / 2.0) / 31.0; \n\
	} \n\
"};
