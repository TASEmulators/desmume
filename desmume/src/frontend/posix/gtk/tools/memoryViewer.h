/* memoryViewer.h - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2026 DeSmuME Team
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __MEMORYVIEWER_GTK_H__
#define __MEMORYVIEWER_GTK_H__

#include "../dTool.h"

// Memory Viewer tool window. The region model, reads/writes and breakpoint
// lists live in shared/memory_viewer_core.cpp, so this file is UI only,
// exactly like the Windows, Cocoa and GTK4 hosts of the same core.
extern dTool_t dTool_memoryViewer;

#endif /*__MEMORYVIEWER_GTK_H__*/
