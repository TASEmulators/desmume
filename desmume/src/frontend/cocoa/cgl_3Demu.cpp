/*
	Copyright (C) 2025 DeSmuME team

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

#include "cgl_3Demu.h"

#include <sys/types.h>
#include <sys/sysctl.h>

#include <CoreVideo/CoreVideo.h>
#include <OpenGL/OpenGL.h>

#ifdef MAC_OS_X_VERSION_10_7
	#include "../../OGLRender_3_2.h"
#else
	#include "../../OGLRender.h"
#endif

#include "utilities.h"

#if !defined(MAC_OS_X_VERSION_10_7)
	#define kCGLPFAOpenGLProfile         (CGLPixelFormatAttribute)99
	#define kCGLOGLPVersion_Legacy       0x1000
	#define kCGLOGLPVersion_3_2_Core     0x3200
	#define kCGLOGLPVersion_GL3_Core     0x3200
	#define kCGLRPVideoMemoryMegabytes   (CGLRendererProperty)131
	#define kCGLRPTextureMemoryMegabytes (CGLRendererProperty)132
#endif

#if !defined(MAC_OS_X_VERSION_10_9)
	#define kCGLOGLPVersion_GL4_Core 0x4100
#endif

#if !defined(MAC_OS_X_VERSION_10_13)
	#define kCGLRPRemovable (CGLRendererProperty)142
#endif


int __hostRendererID = -1;
char __hostRendererString[256] = {0};

CGLContextObj OSXOpenGLRendererContext = NULL;
CGLContextObj OSXOpenGLRendererContextPrev = NULL;
SILENCE_DEPRECATION_MACOS_10_7( CGLPBufferObj OSXOpenGLRendererPBuffer = NULL )

static bool __cgl_initOpenGL(const int requestedProfile)
{
	bool result = false;
	CACHE_ALIGN char ctxString[16] = {0};
	
	if (requestedProfile == kCGLOGLPVersion_GL4_Core)
	{
		strncpy(ctxString, "CGL 4.1", sizeof(ctxString));
	}
	else if (requestedProfile == kCGLOGLPVersion_3_2_Core)
	{
		strncpy(ctxString, "CGL 3.2", sizeof(ctxString));
	}
	else
	{
		strncpy(ctxString, "CGL Legacy", sizeof(ctxString));
	}
	
	if (OSXOpenGLRendererContext != NULL)
	{
		result = true;
		return result;
	}
	
	const bool isHighSierraSupported   = IsOSXVersionSupported(10, 13, 0);
	const bool isMavericksSupported    = (isHighSierraSupported   || IsOSXVersionSupported(10, 9, 0));
	const bool isMountainLionSupported = (isMavericksSupported    || IsOSXVersionSupported(10, 8, 0));
	const bool isLionSupported         = (isMountainLionSupported || IsOSXVersionSupported(10, 7, 0));
	const bool isLeopardSupported      = (isLionSupported         || IsOSXVersionSupported(10, 5, 0));
	
	CGLPixelFormatAttribute attrs[] = {
		kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
		kCGLPFAAlphaSize, (CGLPixelFormatAttribute)8,
		kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
		kCGLPFAStencilSize, (CGLPixelFormatAttribute)8,
		kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)0,
		kCGLPFAAllowOfflineRenderers,
		kCGLPFAAccelerated,
		kCGLPFANoRecovery,
		(CGLPixelFormatAttribute)0
	};
	
	if (requestedProfile == kCGLOGLPVersion_GL4_Core)
	{
		if (isMavericksSupported)
		{
			attrs[5] = (CGLPixelFormatAttribute)0; // We'll be using FBOs instead of the default framebuffer.
			attrs[7] = (CGLPixelFormatAttribute)0; // We'll be using FBOs instead of the default framebuffer.
			attrs[9] = (CGLPixelFormatAttribute)requestedProfile;
		}
		else
		{
			fprintf(stderr, "%s: Your version of OS X is too old to support 4.1 Core Profile.\n", ctxString);
			return result;
		}
	}
	else if (requestedProfile == kCGLOGLPVersion_3_2_Core)
	{
		// As of 2021/09/03, testing has shown that macOS v10.7's OpenGL 3.2 shader
		// compiler isn't very reliable, and so we're going to require macOS v10.8
		// instead, which at least has a working shader compiler for OpenGL 3.2.
		if (isMountainLionSupported)
		{
			attrs[5] = (CGLPixelFormatAttribute)0; // We'll be using FBOs instead of the default framebuffer.
			attrs[7] = (CGLPixelFormatAttribute)0; // We'll be using FBOs instead of the default framebuffer.
			attrs[9] = (CGLPixelFormatAttribute)requestedProfile;
		}
		else
		{
			fprintf(stderr, "%s: Your version of OS X is too old to support 3.2 Core Profile.\n", ctxString);
			return result;
		}
	}
	else if (isLionSupported)
	{
		attrs[9] = (CGLPixelFormatAttribute)kCGLOGLPVersion_Legacy;
	}
	else
	{
		attrs[8]  = (CGLPixelFormatAttribute)kCGLPFAAccelerated;
		attrs[9]  = (CGLPixelFormatAttribute)kCGLPFANoRecovery;
		attrs[11] = (CGLPixelFormatAttribute)0;
		attrs[12] = (CGLPixelFormatAttribute)0;
	}
	
	CGLError error = kCGLNoError;
	CGLPixelFormatObj cglPixFormat = NULL;
	CGLContextObj newContext = NULL;
	GLint virtualScreenCount = 0;
	
	CGLChoosePixelFormat(attrs, &cglPixFormat, &virtualScreenCount);
	if (cglPixFormat == NULL)
	{
		if (requestedProfile == kCGLOGLPVersion_GL4_Core)
		{
			// OpenGL 4.1 Core Profile requires hardware acceleration. Bail if we can't find a renderer that supports both.
			fprintf(stderr, "%s: This system has no HW-accelerated renderers that support 4.1 Core Profile.\n", ctxString);
			return result;
		}
		else if (requestedProfile == kCGLOGLPVersion_3_2_Core)
		{
			// OpenGL 3.2 Core Profile requires hardware acceleration. Bail if we can't find a renderer that supports both.
			fprintf(stderr, "%s: This system has no HW-accelerated renderers that support 3.2 Core Profile.\n", ctxString);
			return result;
		}
		
		// For Legacy OpenGL, we'll allow fallback to the Apple Software Renderer.
		// However, doing this will result in a substantial performance loss.
		if (attrs[8] == kCGLPFAAccelerated)
		{
			attrs[8]  = (CGLPixelFormatAttribute)0;
			attrs[9]  = (CGLPixelFormatAttribute)0;
			attrs[10] = (CGLPixelFormatAttribute)0;
		}
		else
		{
			attrs[10] = (CGLPixelFormatAttribute)0;
			attrs[11] = (CGLPixelFormatAttribute)0;
			attrs[12] = (CGLPixelFormatAttribute)0;
		}
		
		error = CGLChoosePixelFormat(attrs, &cglPixFormat, &virtualScreenCount);
		if (error != kCGLNoError)
		{
			// We shouldn't fail at this point, but we're including this to account for all code paths.
			fprintf(stderr, "%s: Failed to create the pixel format structure: %i\n", ctxString, (int)error);
			return result;
		}
		else
		{
			printf("WARNING: No HW-accelerated renderers were found -- falling back to Apple Software Renderer.\n         This will result in a substantial performance loss.");
		}
	}
	
	// Create the OpenGL context using our pixel format, and then save the default assigned virtual screen.
	error = CGLCreateContext(cglPixFormat, NULL, &newContext);
	CGLReleasePixelFormat(cglPixFormat);
	cglPixFormat = NULL;
	
	if (error != kCGLNoError)
	{
		fprintf(stderr, "%s: Failed to create an OpenGL context: %i\n", ctxString, (int)error);
		return result;
	}
	
	OSXOpenGLRendererContext = newContext;
	GLint defaultVirtualScreen = 0;
	CGLGetVirtualScreen(newContext, &defaultVirtualScreen);
	
	// Retrieve the properties of every renderer available on the system.
	CGLRendererInfoObj cglRendererInfo = NULL;
	GLint rendererCount = 0;
	CGLQueryRendererInfo(0xFFFFFFFF, &cglRendererInfo, &rendererCount);
	
	ClientRendererInfo *rendererInfo = (ClientRendererInfo *)malloc(sizeof(ClientRendererInfo) * rendererCount);
	memset(rendererInfo, 0, sizeof(ClientRendererInfo) * rendererCount);
	
	if (isLeopardSupported)
	{
		for (GLint i = 0; i < rendererCount; i++)
		{
			ClientRendererInfo &info = rendererInfo[i];
			
			CGLDescribeRenderer(cglRendererInfo, i, kCGLRPOnline, &(info.online));
			CGLDescribeRenderer(cglRendererInfo, i, kCGLRPDisplayMask, &(info.displayID));
			info.displayID = (GLint)CGOpenGLDisplayMaskToDisplayID(info.displayID);
			CGLDescribeRenderer(cglRendererInfo, i, kCGLRPAccelerated, &(info.accelerated));
			CGLDescribeRenderer(cglRendererInfo, i, kCGLRPRendererID,  &(info.rendererID));
			
			if (isLionSupported)
			{
				CGLDescribeRenderer(cglRendererInfo, i, kCGLRPVideoMemoryMegabytes, &(info.videoMemoryMB));
				CGLDescribeRenderer(cglRendererInfo, i, kCGLRPTextureMemoryMegabytes, &(info.textureMemoryMB));
			}
			else
			{
				CGLDescribeRenderer(cglRendererInfo, i, kCGLRPVideoMemory, &(info.videoMemoryMB));
				info.videoMemoryMB = (GLint)(((uint32_t)info.videoMemoryMB + 1) >> 20);
				CGLDescribeRenderer(cglRendererInfo, i, kCGLRPTextureMemory, &(info.textureMemoryMB));
				info.textureMemoryMB = (GLint)(((uint32_t)info.textureMemoryMB + 1) >> 20);
			}
			
			if (isHighSierraSupported)
			{
				CGLDescribeRenderer(cglRendererInfo, i, kCGLRPRemovable, &(info.removable));
			}
		}
	}
	else
	{
		CGLDestroyRendererInfo(cglRendererInfo);
		free(rendererInfo);
		fprintf(stderr, "%s: Failed to retrieve renderer info - requires Mac OS X v10.5 or later.\n", ctxString);
		return result;
	}
	
	CGLDestroyRendererInfo(cglRendererInfo);
	cglRendererInfo = NULL;
	
	// Retrieve the vendor and renderer info from OpenGL.
	cgl_beginOpenGL();
	
	for (GLint i = 0; i < virtualScreenCount; i++)
	{
		CGLSetVirtualScreen(newContext, i);
		GLint r;
		CGLGetParameter(newContext, kCGLCPCurrentRendererID, &r);

		for (int j = 0; j < rendererCount; j++)
		{
			ClientRendererInfo &info = rendererInfo[j];
			
			if (r == info.rendererID)
			{
				info.virtualScreen = i;
				
				info.vendorStr = (const char *)glGetString(GL_VENDOR);
				if (info.vendorStr != NULL)
				{
					strncpy(info.vendor, (const char *)info.vendorStr, sizeof(info.vendor));
				}
				else if (info.accelerated == 0)
				{
					strncpy(info.vendor, "Apple Inc.", sizeof(info.vendor));
				}
				else
				{
					strncpy(info.vendor, "UNKNOWN", sizeof(info.vendor));
				}
				
				info.nameStr = (const char *)glGetString(GL_RENDERER);
				if (info.nameStr != NULL)
				{
					strncpy(info.name, (const char *)info.nameStr, sizeof(info.name));
				}
				else if (info.accelerated == 0)
				{
					strncpy(info.name, "Apple Software Renderer", sizeof(info.name));
				}
				else
				{
					strncpy(info.name, "UNKNOWN", sizeof(info.name));
				}
				
			}
		}
	}
	
	cgl_endOpenGL();
	
	// Get the default virtual screen.
	strncpy(__hostRendererString, "UNKNOWN", sizeof(__hostRendererString));
	__hostRendererID = -1;
	
	ClientRendererInfo defaultRendererInfo = rendererInfo[0];
	for (int i = 0; i < rendererCount; i++)
	{
		if (defaultVirtualScreen == rendererInfo[i].virtualScreen)
		{
			defaultRendererInfo = rendererInfo[i];
			__hostRendererID = defaultRendererInfo.rendererID;
			strncpy(__hostRendererString, (const char *)defaultRendererInfo.name, sizeof(__hostRendererString));
			
			if ( (defaultRendererInfo.online == 1) && (defaultRendererInfo.vendorStr != NULL) && (defaultRendererInfo.nameStr != NULL) )
			{
				break;
			}
		}
	}
	
	printf("Default OpenGL Renderer: [0x%08X] %s\n", __hostRendererID, __hostRendererString);
	/*
	bool isDefaultRunningIntegratedGPU = false;
	if ( (defaultRendererInfo.online == 1) && (defaultRendererInfo.vendorStr != NULL) && (defaultRendererInfo.nameStr != NULL) )
	{
		const ClientRendererInfo &d = defaultRendererInfo;
		isDefaultRunningIntegratedGPU = (strstr(d.name, "GMA 950") != NULL) ||
		                                (strstr(d.name, "GMA X3100") != NULL) ||
		                                (strstr(d.name, "GeForce 9400M") != NULL) ||
		                                (strstr(d.name, "GeForce 320M") != NULL) ||
		                                (strstr(d.name, "HD Graphics") != NULL) ||
		                                (strstr(d.name, "Iris 5100") != NULL) ||
		                                (strstr(d.name, "Iris Plus") != NULL) ||
		                                (strstr(d.name, "Iris Pro") != NULL) ||
		                                (strstr(d.name, "Iris Graphics") != NULL) ||
		                                (strstr(d.name, "UHD Graphics") != NULL);
	}
	*/
#if defined(DEBUG) && (DEBUG == 1)
	// Report information on every renderer.
	if (!isLionSupported)
	{
		printf("WARNING: You are running a macOS version earlier than v10.7.\n         Video Memory and Texture Memory reporting is capped\n         at 2048 MB on older macOS.\n");
	}
	printf("CGL Renderer Count: %i\n", rendererCount);
	printf("  Virtual Screen Count: %i\n\n", virtualScreenCount);
	
	for (int i = 0; i < rendererCount; i++)
	{
		const ClientRendererInfo &info = rendererInfo[i];
		
		printf("Renderer Index: %i\n", i);
		printf("Virtual Screen: %i\n", info.virtualScreen);
		printf("Vendor: %s\n", info.vendor);
		printf("Renderer: %s\n", info.name);
		printf("Renderer ID: 0x%08X\n", info.rendererID);
		printf("Accelerated: %s\n", (info.accelerated == 1) ? "YES" : "NO");
		printf("Online: %s\n", (info.online == 1) ? "YES" : "NO");
		
		if (isHighSierraSupported)
		{
			printf("Removable: %s\n", (info.removable == 1) ? "YES" : "NO");
		}
		else
		{
			printf("Removable: UNSUPPORTED, Requires High Sierra\n");
		}
		
		printf("Display ID: 0x%08X\n", info.displayID);
		printf("Video Memory: %i MB\n", info.videoMemoryMB);
		printf("Texture Memory: %i MB\n\n", info.textureMemoryMB);
	}
#endif
	
	// Search for a better virtual screen that will suit our offscreen rendering better.
	//
	// At the moment, we are not supporting removable renderers such as eGPUs. Attempting
	// to support removable renderers would require a lot more code to handle dynamically
	// changing display<-->renderer associations. - rogerman 2025/03/25
	bool wasBetterVirtualScreenFound = false;
	
	char *modelCString = NULL;
	size_t modelStringLen = 0;
	
	sysctlbyname("hw.model", NULL, &modelStringLen, NULL, 0);
	if (modelStringLen > 0)
	{
		modelCString = (char *)malloc(modelStringLen * sizeof(char));
		sysctlbyname("hw.model", modelCString, &modelStringLen, NULL, 0);
	}
	
	for (int i = 0; i < rendererCount; i++)
	{
		const ClientRendererInfo &info = rendererInfo[i];
		
		if ( (defaultRendererInfo.vendorStr == NULL) || (defaultRendererInfo.nameStr == NULL) || (info.vendorStr == NULL) || (info.nameStr == NULL) )
		{
			continue;
		}
		
		wasBetterVirtualScreenFound = (info.accelerated == 1) &&
		(    (        (modelCString != NULL) && (strstr((const char *)modelCString, "MacBookPro") != NULL) &&
		         (  ( (strstr(defaultRendererInfo.name, "GeForce 9400M") != NULL) &&
		              (strstr(info.name, "GeForce 9600M GT") != NULL) ) ||
		            ( (strstr(defaultRendererInfo.name, "HD Graphics") != NULL) &&
		             ((strstr(info.name, "GeForce GT 330M") != NULL) ||
		              (strstr(info.name, "Radeon HD 6490M") != NULL) ||
		              (strstr(info.name, "Radeon HD 6750M") != NULL) ||
		              (strstr(info.name, "Radeon HD 6770M") != NULL) ||
		              (strstr(info.name, "GeForce GT 650M") != NULL) ||
		              (strstr(info.name, "Radeon Pro 450") != NULL) ||
		              (strstr(info.name, "Radeon Pro 455") != NULL) ||
		              (strstr(info.name, "Radeon Pro 555") != NULL) ||
		              (strstr(info.name, "Radeon Pro 560") != NULL)) ) ||
		            ( (strstr(defaultRendererInfo.name, "Iris Pro") != NULL) &&
		             ((strstr(info.name, "GeForce GT 750M") != NULL) ||
		              (strstr(info.name, "Radeon R9 M370X") != NULL)) ) ||
		            ( (strstr(defaultRendererInfo.name, "UHD Graphics") != NULL) &&
		             ((strstr(info.name, "Radeon Pro 555X") != NULL) ||
		              (strstr(info.name, "Radeon Pro 560X") != NULL) ||
		              (strstr(info.name, "Radeon Pro Vega 16") != NULL) ||
		              (strstr(info.name, "Radeon Pro Vega 20") != NULL) ||
		              (strstr(info.name, "Radeon Pro 5300M") != NULL) ||
		              (strstr(info.name, "Radeon Pro 5500M") != NULL) ||
		              (strstr(info.name, "Radeon Pro 5600M") != NULL)) )  )   ) ||
		     (        (modelCString != NULL) && (strstr((const char *)modelCString, "MacPro6,1") != NULL) && (info.online == 0) &&
		             ((strstr(info.name, "FirePro D300") != NULL) ||
		              (strstr(info.name, "FirePro D500") != NULL) ||
		              (strstr(info.name, "FirePro D700") != NULL))   )    );
		
		if (wasBetterVirtualScreenFound)
		{
			CGLSetVirtualScreen(newContext, info.virtualScreen);
			__hostRendererID = info.rendererID;
			strncpy(__hostRendererString, (const char *)info.name, sizeof(__hostRendererString));
			printf("Found Better OpenGL Renderer: [0x%08X] %s\n", __hostRendererID, __hostRendererString);
			break;
		}
	}
	
	// If we couldn't find a better virtual screen for our rendering, then just revert to the default one.
	if (!wasBetterVirtualScreenFound)
	{
		CGLSetVirtualScreen(newContext, defaultVirtualScreen);
	}
	
	// We're done! Report success and return.
	printf("%s: OpenGL context creation successful.\n\n", ctxString);
	free(rendererInfo);
	free(modelCString);
	
	result = true;
	return result;
}

bool cgl_initOpenGL_StandardAuto()
{
	bool isContextCreated = __cgl_initOpenGL(kCGLOGLPVersion_GL4_Core);
	
	if (!isContextCreated)
	{
		isContextCreated = __cgl_initOpenGL(kCGLOGLPVersion_3_2_Core);
	}
	
	if (!isContextCreated)
	{
		isContextCreated = __cgl_initOpenGL(kCGLOGLPVersion_Legacy);
	}
	
	return isContextCreated;
}

bool cgl_initOpenGL_LegacyAuto()
{
	return __cgl_initOpenGL(kCGLOGLPVersion_Legacy);
}

bool cgl_initOpenGL_3_2_CoreProfile()
{
	return __cgl_initOpenGL(kCGLOGLPVersion_3_2_Core);
}

void cgl_deinitOpenGL()
{
	if (OSXOpenGLRendererContext == NULL)
	{
		return;
	}
	
	CGLSetCurrentContext(NULL);
	SILENCE_DEPRECATION_MACOS_10_7( CGLReleasePBuffer(OSXOpenGLRendererPBuffer) );
	OSXOpenGLRendererPBuffer = NULL;
	
	CGLReleaseContext(OSXOpenGLRendererContext);
	OSXOpenGLRendererContext = NULL;
	OSXOpenGLRendererContextPrev = NULL;
}

bool cgl_beginOpenGL()
{
	OSXOpenGLRendererContextPrev = CGLGetCurrentContext();
	CGLSetCurrentContext(OSXOpenGLRendererContext);
	
	return true;
}

void cgl_endOpenGL()
{
#ifndef PORT_VERSION_OS_X_APP
	// The OpenEmu plug-in needs the context reset after 3D rendering since OpenEmu's context
	// is assumed to be the default context. However, resetting the context for our standalone
	// app can cause problems since the core emulator's context is assumed to be the default
	// context. So reset the context for OpenEmu and skip resetting for us.
	CGLSetCurrentContext(OSXOpenGLRendererContextPrev);
#endif
}

bool cgl_framebufferDidResizeCallback(const bool isFBOSupported, size_t w, size_t h)
{
	bool result = false;
	
	if (isFBOSupported)
	{
		result = true;
		return result;
	}
	
	if (IsOSXVersionSupported(10, 13, 0))
	{
		printf("Mac OpenGL: P-Buffers cannot be created on macOS v10.13 High Sierra and later.\n");
		return result;
	}
	
	// Create a PBuffer if FBOs are not supported.
	SILENCE_DEPRECATION_MACOS_10_7( CGLPBufferObj newPBuffer = NULL );
	SILENCE_DEPRECATION_MACOS_10_7( CGLError error = CGLCreatePBuffer((GLsizei)w, (GLsizei)h, GL_TEXTURE_2D, GL_RGBA, 0, &newPBuffer) );
	
	if ( (newPBuffer == NULL) || (error != kCGLNoError) )
	{
		printf("Mac OpenGL: ERROR - Could not create the P-Buffer: %s\n", CGLErrorString(error));
		return result;
	}
	else
	{
		GLint virtualScreenID = 0;
		CGLGetVirtualScreen(OSXOpenGLRendererContext, &virtualScreenID);
		SILENCE_DEPRECATION_MACOS_10_7( CGLSetPBuffer(OSXOpenGLRendererContext, newPBuffer, 0, 0, virtualScreenID) );
	}
	
	SILENCE_DEPRECATION_MACOS_10_7( CGLPBufferObj oldPBuffer = OSXOpenGLRendererPBuffer );
	OSXOpenGLRendererPBuffer = newPBuffer;
	SILENCE_DEPRECATION_MACOS_10_7( CGLReleasePBuffer(oldPBuffer) );
	
	result = true;
	return result;
}

int cgl_getHostRendererID()
{
	return __hostRendererID;
}

const char* cgl_getHostRendererString()
{
	return __hostRendererString;
}
