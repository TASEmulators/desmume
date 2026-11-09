/*
	Copyright (C) 2026 DeSmuME team

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

#include "wgpu_upscaler_linux.h"

#if defined(ENABLE_WGPU)

#include <gio/gio.h>
#include <stdio.h>

#include <map>
#include <string>

#include "../../shared/wgpu_present.h"

namespace
{

// Reads the pass sources out of the GResource bundle. The aliases in
// gtk/desmume.gresource.xml are the names the chain table uses, so the path of
// a pass is simply the resource prefix plus its name.
class LinuxUpscalerShaderSource : public WGPUPresentShaderSource
{
public:
	virtual ~LinuxUpscalerShaderSource()
	{
		for (std::map<std::string, GBytes *>::iterator it = _cache.begin(); it != _cache.end(); ++it)
		{
			g_bytes_unref(it->second);
		}
	}

	virtual bool getShader(const char *name, const char **source, size_t *length)
	{
		*source = NULL;
		*length = 0;

		if (name == NULL)
		{
			return false;
		}

		std::map<std::string, GBytes *>::iterator it = _cache.find(name);
		GBytes *bytes;

		if (it != _cache.end())
		{
			bytes = it->second;
		}
		else
		{
			const std::string path = std::string("/org/desmume/DeSmuME/") + name;
			GError *error = NULL;
			bytes = g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

			if (bytes == NULL)
			{
				if (error != NULL)
				{
					fprintf(stderr, "WGPU present: the %s shader is missing: %s\n", name, error->message);
					g_error_free(error);
				}

				return false;
			}

			// Keep the bytes alive for as long as this object, because the
			// text handed out points into them.
			_cache[name] = bytes;
		}

		gsize size = 0;
		const char *data = (const char *)g_bytes_get_data(bytes, &size);

		if ( (data == NULL) || (size == 0) )
		{
			return false;
		}

		*source = data;
		*length = (size_t)size;

		return true;
	}

private:
	std::map<std::string, GBytes *> _cache;
};

} // namespace

WGPUPresentShaderSource *WGPUCreateLinuxUpscalerShaderSource()
{
	// The resources stay loaded for the life of the process, so the text this
	// hands out stays valid for as long as the present object lives.
	static LinuxUpscalerShaderSource source;

	return &source;
}

#else

WGPUPresentShaderSource *WGPUCreateLinuxUpscalerShaderSource()
{
	return NULL;
}

#endif
