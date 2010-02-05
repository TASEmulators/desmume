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
	\
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
				flagColor.rgb = gl_Color.rgb * (1.0-texColor.a) + texColor.rgb * texColor.a;\n\
				flagColor.a = texColor.a;\n\
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
		gl_FragColor = flagColor; \n\
	} \n\
"};
