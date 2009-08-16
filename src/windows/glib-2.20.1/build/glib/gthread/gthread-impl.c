/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: thread related functions
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe
 */

#include "config.h"

#include "glib.h"
#include "gthreadprivate.h"

#ifdef G_THREADS_ENABLED

static GSystemThread zero_thread; /* This is initialized to all zero */
static gboolean thread_system_already_initialized = FALSE;
static gint g_thread_priority_map [G_THREAD_PRIORITY_URGENT + 1];

#include G_THREAD_SOURCE

#ifndef PRIORITY_LOW_VALUE
# define PRIORITY_LOW_VALUE 0
#endif

#ifndef PRIORITY_URGENT_VALUE
# define PRIORITY_URGENT_VALUE 0
#endif

#ifndef PRIORITY_NORMAL_VALUE
# define PRIORITY_NORMAL_VALUE						\
  ((PRIORITY_LOW_VALUE * 6 + PRIORITY_URGENT_VALUE * 4) / 10)
#endif /* PRIORITY_NORMAL_VALUE */

#ifndef PRIORITY_HIGH_VALUE
# define PRIORITY_HIGH_VALUE						\
  ((PRIORITY_NORMAL_VALUE + PRIORITY_URGENT_VALUE * 2) / 3)
#endif /* PRIORITY_HIGH_VALUE */

void g_mutex_init (void);
void g_mem_init (void);
void g_messages_init (void);
void g_convert_init (void);
void g_rand_init (void);
void g_main_thread_init (void);

typedef struct _GMutexDebugInfo GMutexDebugInfo;
struct _GMutexDebugInfo
{
  gchar *location;
  GSystemThread owner;
};

#define G_MUTEX_DEBUG_INFO(mutex) 					\
  (((GMutexDebugInfo*)(((char*)mutex)+G_MUTEX_SIZE)))

static GMutex *
g_mutex_new_errorcheck_impl (void)
{
  GMutex *retval = g_thread_functions_for_glib_use_default.mutex_new ();
  GMutexDebugInfo *info;
  retval = g_realloc (retval, G_MUTEX_SIZE + sizeof (GMutexDebugInfo));

  info = G_MUTEX_DEBUG_INFO (retval);
  g_system_thread_assign (info->owner, zero_thread);
  info->location = "invalid";

  return retval;
}

static void
g_mutex_lock_errorcheck_impl (GMutex *mutex,
			      const gulong magic,
			      gchar * const location)
{
  GMutexDebugInfo *info = G_MUTEX_DEBUG_INFO (mutex);
  gchar *loc = (magic == G_MUTEX_DEBUG_MAGIC) ? location : "unknown";

  GSystemThread self;
  g_thread_functions_for_glib_use.thread_self (&self);

  if (g_system_thread_equal (info->owner, self))
    g_error ("Trying to recursivly lock a mutex at '%s', "
	     "previously locked at '%s'",
	     loc, info->location);

  g_thread_functions_for_glib_use_default.mutex_lock (mutex);

  g_system_thread_assign (info->owner, self);
  info->location = loc;
}

static gboolean
g_mutex_trylock_errorcheck_impl (GMutex *mutex,
				 const gulong magic,
				 gchar * const location)
{
  GMutexDebugInfo *info = G_MUTEX_DEBUG_INFO (mutex);
  gchar *loc = (magic == G_MUTEX_DEBUG_MAGIC) ? location : "unknown";

  GSystemThread self;
  g_thread_functions_for_glib_use.thread_self (&self);

  if (g_system_thread_equal (info->owner, self))
    g_error ("Trying to recursivly lock a mutex at '%s', "
	     "previously locked at '%s'",
	     loc, info->location);

  if (!g_thread_functions_for_glib_use_default.mutex_trylock (mutex))
    return FALSE;

  g_system_thread_assign (info->owner, self);
  info->location = loc;

  return TRUE;
}

static void
g_mutex_unlock_errorcheck_impl (GMutex *mutex,
				const gulong magic,
				gchar * const location)
{
  GMutexDebugInfo *info = G_MUTEX_DEBUG_INFO (mutex);
  gchar *loc = (magic == G_MUTEX_DEBUG_MAGIC) ? location : "unknown";

  GSystemThread self;
  g_thread_functions_for_glib_use.thread_self (&self);

  if (g_system_thread_equal (info->owner, zero_thread))
    g_error ("Trying to unlock an unlocked mutex at '%s'", loc);

  if (!g_system_thread_equal (info->owner, self))
    g_warning ("Trying to unlock a mutex at '%s', "
	       "previously locked by a different thread at '%s'",
	       loc, info->location);

  g_system_thread_assign (info->owner, zero_thread);
  info->location = NULL;

  g_thread_functions_for_glib_use_default.mutex_unlock (mutex);
}

static void
g_mutex_free_errorcheck_impl (GMutex *mutex,
			      const gulong magic,
			      gchar * const location)
{
  GMutexDebugInfo *info = G_MUTEX_DEBUG_INFO (mutex);
  gchar *loc = (magic == G_MUTEX_DEBUG_MAGIC) ? location : "unknown";

  if (info && !g_system_thread_equal (info->owner, zero_thread))
    g_error ("Trying to free a locked mutex at '%s', "
	     "which was previously locked at '%s'",
	     loc, info->location);

  g_thread_functions_for_glib_use_default.mutex_free (mutex);
}

static void
g_cond_wait_errorcheck_impl (GCond *cond,
			     GMutex *mutex,
			     const gulong magic,
			     gchar * const location)
{
  GMutexDebugInfo *info = G_MUTEX_DEBUG_INFO (mutex);
  gchar *loc = (magic == G_MUTEX_DEBUG_MAGIC) ? location : "unknown";

  GSystemThread self;
  g_thread_functions_for_glib_use.thread_self (&self);

  if (g_system_thread_equal (info->owner, zero_thread))
    g_error ("Trying to use an unlocked mutex in g_cond_wait() at '%s'", loc);

  if (!g_system_thread_equal (info->owner, self))
    g_error ("Trying to use a mutex locked by another thread in "
	     "g_cond_wait() at '%s'", loc);

  g_system_thread_assign (info->owner, zero_thread);
  loc = info->location;

  g_thread_functions_for_glib_use_default.cond_wait (cond, mutex);

  g_system_thread_assign (info->owner, self);
  info->location = loc;
}


static gboolean
g_cond_timed_wait_errorcheck_impl (GCond *cond,
                                   GMutex *mutex,
                                   GTimeVal *end_time,
				   const gulong magic,
				   gchar * const location)
{
  GMutexDebugInfo *info = G_MUTEX_DEBUG_INFO (mutex);
  gchar *loc = (magic == G_MUTEX_DEBUG_MAGIC) ? location : "unknown";
  gboolean retval;

  GSystemThread self;
  g_thread_functions_for_glib_use.thread_self (&self);

  if (g_system_thread_equal (info->owner, zero_thread))
    g_error ("Trying to use an unlocked mutex in g_cond_timed_wait() at '%s'",
	     loc);

  if (!g_system_thread_equal (info->owner, self))
    g_error ("Trying to use a mutex locked by another thread in "
	     "g_cond_timed_wait() at '%s'", loc);

  g_system_thread_assign (info->owner, zero_thread);
  loc = info->location;

  retval = g_thread_functions_for_glib_use_default.cond_timed_wait (cond,
								    mutex,
								    end_time);

  g_system_thread_assign (info->owner, self);
  info->location = loc;

  return retval;
}


/* unshadow function declaration. See gthread.h */
#undef g_thread_init

void
g_thread_init_with_errorcheck_mutexes (GThreadFunctions* init)
{
  GThreadFunctions errorcheck_functions;
  if (init)
    g_error ("Errorcheck mutexes can only be used for native "
	     "thread implementations. Sorry." );

#ifdef HAVE_G_THREAD_IMPL_INIT
  /* This isn't called in g_thread_init, as it doesn't think to get
   * the default implementation, so we have to call it on our own.
   *
   * We must call this before copying
   * g_thread_functions_for_glib_use_default as the
   * implementation-specific init function might modify the contents
   * of g_thread_functions_for_glib_use_default based on operating
   * system version, C library version, or whatever. */
  g_thread_impl_init();
#endif /* HAVE_G_THREAD_IMPL_INIT */

  errorcheck_functions = g_thread_functions_for_glib_use_default;
  errorcheck_functions.mutex_new = g_mutex_new_errorcheck_impl;
  errorcheck_functions.mutex_lock =
    (void (*)(GMutex *)) g_mutex_lock_errorcheck_impl;
  errorcheck_functions.mutex_trylock =
    (gboolean (*)(GMutex *)) g_mutex_trylock_errorcheck_impl;
  errorcheck_functions.mutex_unlock =
    (void (*)(GMutex *)) g_mutex_unlock_errorcheck_impl;
  errorcheck_functions.mutex_free =
    (void (*)(GMutex *)) g_mutex_free_errorcheck_impl;
  errorcheck_functions.cond_wait =
    (void (*)(GCond *, GMutex *)) g_cond_wait_errorcheck_impl;
  errorcheck_functions.cond_timed_wait =
    (gboolean (*)(GCond *, GMutex *, GTimeVal *))
    g_cond_timed_wait_errorcheck_impl;

  g_thread_init (&errorcheck_functions);
}

void
g_thread_init (GThreadFunctions* init)
{
  gboolean supported;

  if (thread_system_already_initialized)
    g_error ("GThread system may only be initialized once.");

  thread_system_already_initialized = TRUE;

  if (init == NULL)
    {
#ifdef HAVE_G_THREAD_IMPL_INIT
      /* now do any initialization stuff required by the
       * implementation, but only if called with a NULL argument, of
       * course. Otherwise it's up to the user to do so. */
      g_thread_impl_init();
#endif /* HAVE_G_THREAD_IMPL_INIT */
      init = &g_thread_functions_for_glib_use_default;
    }
  else
    g_thread_use_default_impl = FALSE;

  g_thread_functions_for_glib_use = *init;
  if (g_thread_gettime_impl)
    g_thread_gettime = g_thread_gettime_impl;

  supported = (init->mutex_new &&
	       init->mutex_lock &&
	       init->mutex_trylock &&
	       init->mutex_unlock &&
	       init->mutex_free &&
	       init->cond_new &&
	       init->cond_signal &&
	       init->cond_broadcast &&
	       init->cond_wait &&
	       init->cond_timed_wait &&
	       init->cond_free &&
	       init->private_new &&
	       init->private_get &&
	       init->private_set &&
	       init->thread_create &&
	       init->thread_yield &&
	       init->thread_join &&
	       init->thread_exit &&
	       init->thread_set_priority &&
	       init->thread_self);

  /* if somebody is calling g_thread_init (), it means that he wants to
   * have thread support, so check this
   */
  if (!supported)
    {
      if (g_thread_use_default_impl)
	g_error ("Threads are not supported on this platform.");
      else
	g_error ("The supplied thread function vector is invalid.");
    }

  g_thread_priority_map [G_THREAD_PRIORITY_LOW] = PRIORITY_LOW_VALUE;
  g_thread_priority_map [G_THREAD_PRIORITY_NORMAL] = PRIORITY_NORMAL_VALUE;
  g_thread_priority_map [G_THREAD_PRIORITY_HIGH] = PRIORITY_HIGH_VALUE;
  g_thread_priority_map [G_THREAD_PRIORITY_URGENT] = PRIORITY_URGENT_VALUE;

  g_thread_init_glib ();
}

#else /* !G_THREADS_ENABLED */

void
g_thread_init (GThreadFunctions* init)
{
  g_error ("GLib thread support is disabled.");
}

void
g_thread_init_with_errorcheck_mutexes (GThreadFunctions* init)
{
  g_error ("GLib thread support is disabled.");
}

#endif /* !G_THREADS_ENABLED */
