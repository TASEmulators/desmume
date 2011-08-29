/* Predefined OpenGL shaders */

/* Vertex shader */
const char *vertexShader = {"\
	varying vec4 pos; \n\
	void main() \n\
	{ \n\
		gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex; \n\
		gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0; \n\
		gl_FrontColor = gl_Color; \n\
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
			flagColor = gl_Color * texColor; \n\
		} \n\
		else \n\
			if(texBlending == 1) \n\
			{ \n\
				if (texColor.a == 0.0 || hasTexture == 0) \n\
					flagColor.rgb = gl_Color.rgb;\n\
				else \n\
					if (texColor.a == 1.0) \n\
						flagColor.rgb = texColor.rgb;\n\
					else \n\
					flagColor.rgb = texColor.rgb * (1.0-texColor.a) + gl_Color.rgb * texColor.a;\n\
				flagColor.a = gl_Color.a; \n\
			} \n\
			else \n\
				if(texBlending == 2) \n\
				{ \n\
					vec3 toonColor = vec3(texture1D(toonTable, gl_Color.r).rgb); \n\
					flagColor.rgb = texColor.rgb * toonColor.rgb;\n\
					flagColor.a = texColor.a * gl_Color.a;\n\
				} \n\
				else \n\
					if(texBlending == 3) \n\
					{ \n\
						vec3 toonColor = vec3(texture1D(toonTable, gl_Color.r).rgb); \n\
						flagColor.rgb = texColor.rgb * gl_Color.rgb + toonColor.rgb; \n\
						flagColor.a = texColor.a * gl_Color.a; \n\
					} \n\
		if (oglWBuffer == 1) \n\
			// TODO \n\
			gl_FragDepth = (pos.z / pos.w) * 0.5 + 0.5; \n\
		else \n\
			gl_FragDepth = (pos.z / pos.w) * 0.5 + 0.5; \n\
		gl_FragColor = flagColor; \n\
	} \n\
"};

