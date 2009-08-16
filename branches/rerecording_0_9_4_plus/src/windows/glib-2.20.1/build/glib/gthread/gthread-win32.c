/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: solaris thread system implementation
 * Copyright 1998-2001 Sebastian Wilhelmi; University of Karlsruhe
 * Copyright 2001 Hans Breuer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#define STRICT
#define _WIN32_WINDOWS 0x0401 /* to get IsDebuggerPresent */
#include <windows.h>
#undef STRICT

#include <process.h>
#include <stdlib.h>
#include <stdio.h>

#define win32_check_for_error(what) G_STMT_START{			\
  if (!(what))								\
    g_error ("file %s: line %d (%s): error %s during %s",		\
	     __FILE__, __LINE__, G_STRFUNC,				\
	     g_win32_error_message (GetLastError ()), #what);		\
  }G_STMT_END

#define G_MUTEX_SIZE (sizeof (gpointer))

#define PRIORITY_LOW_VALUE    THREAD_PRIORITY_BELOW_NORMAL
#define PRIORITY_NORMAL_VALUE THREAD_PRIORITY_NORMAL
#define PRIORITY_HIGH_VALUE   THREAD_PRIORITY_ABOVE_NORMAL
#define PRIORITY_URGENT_VALUE THREAD_PRIORITY_HIGHEST

static DWORD g_thread_self_tls;
static DWORD g_private_tls;
static DWORD g_cond_event_tls;
static CRITICAL_SECTION g_thread_global_spinlock;

typedef BOOL (__stdcall *GTryEnterCriticalSectionFunc) (CRITICAL_SECTION *);

static GTryEnterCriticalSectionFunc try_enter_critical_section = NULL;

/* As noted in the docs, GPrivate is a limited resource, here we take
 * a rather low maximum to save memory, use GStaticPrivate instead. */
#define G_PRIVATE_MAX 100

static GDestroyNotify g_private_destructors[G_PRIVATE_MAX];

static guint g_private_next = 0;

/* A "forward" declaration of this structure */
static GThreadFunctions g_thread_functions_for_glib_use_default;

typedef struct _GThreadData GThreadData;
struct _GThreadData
{
  GThreadFunc func;
  gpointer data;
  HANDLE thread;
  gboolean joinable;
};

struct _GCond
{
  GPtrArray *array;
  CRITICAL_SECTION lock;
};

static GMutex *
g_mutex_new_win32_cs_impl (void)
{
  CRITICAL_SECTION *cs = g_new (CRITICAL_SECTION, 1);
  gpointer *retval = g_new (gpointer, 1);

  InitializeCriticalSection (cs);
  *retval = cs;
  return (GMutex *) retval;
}

static void
g_mutex_free_win32_cs_impl (GMutex *mutex)
{
  gpointer *ptr = (gpointer *) mutex;
  CRITICAL_SECTION *cs = (CRITICAL_SECTION *) *ptr;

  DeleteCriticalSection (cs);
  g_free (cs);
  g_free (mutex);
}

/* NOTE: the functions g_mutex_lock and g_mutex_unlock may not use
   functions from gmem.c and gmessages.c; */

static void
g_mutex_lock_win32_cs_impl (GMutex *mutex)
{
  EnterCriticalSection (*(CRITICAL_SECTION **)mutex);
}

static gboolean
g_mutex_trylock_win32_cs_impl (GMutex * mutex)
{
  return try_enter_critical_section (*(CRITICAL_SECTION **)mutex);
}

static void
g_mutex_unlock_win32_cs_impl (GMutex *mutex)
{
  LeaveCriticalSection (*(CRITICAL_SECTION **)mutex);
}

static GMutex *
g_mutex_new_win32_impl (void)
{
  HANDLE handle;
  HANDLE *retval;
  win32_check_for_error (handle = CreateMutex (NULL, FALSE, NULL));
  retval = g_new (HANDLE, 1);
  *retval = handle;
  return (GMutex *) retval;
}

static void
g_mutex_free_win32_impl (GMutex *mutex)
{
  win32_check_for_error (CloseHandle (*(HANDLE *) mutex));
  g_free (mutex);
}

/* NOTE: the functions g_mutex_lock and g_mutex_unlock may not use
   functions from gmem.c and gmessages.c; */

static void
g_mutex_lock_win32_impl (GMutex *mutex)
{
  WaitForSingleObject (*(HANDLE *) mutex, INFINITE);
}

static gboolean
g_mutex_trylock_win32_impl (GMutex * mutex)
{
  DWORD result;
  win32_check_for_error (WAIT_FAILED !=
			 (result = WaitForSingleObject (*(HANDLE *)mutex, 0)));
  return result != WAIT_TIMEOUT;
}

static void
g_mutex_unlock_win32_impl (GMutex *mutex)
{
  ReleaseMutex (*(HANDLE *) mutex);
}

static GCond *
g_cond_new_win32_impl (void)
{
  GCond *retval = g_new (GCond, 1);

  retval->array = g_ptr_array_new ();
  InitializeCriticalSection (&retval->lock);

  return retval;
}

static void
g_cond_signal_win32_impl (GCond * cond)
{
  EnterCriticalSection (&cond->lock);

  if (cond->array->len > 0)
    {
      SetEvent (g_ptr_array_index (cond->array, 0));
      g_ptr_array_remove_index (cond->array, 0);
    }

  LeaveCriticalSection (&cond->lock);
}

static void
g_cond_broadcast_win32_impl (GCond * cond)
{
  guint i;
  EnterCriticalSection (&cond->lock);

  for (i = 0; i < cond->array->len; i++)
    SetEvent (g_ptr_array_index (cond->array, i));

  g_ptr_array_set_size (cond->array, 0);
  LeaveCriticalSection (&cond->lock);
}

static gboolean
g_cond_wait_internal (GCond *cond,
		      GMutex *entered_mutex,
		      gulong milliseconds)
{
  gulong retval;
  HANDLE event = TlsGetValue (g_cond_event_tls);

  if (!event)
    {
      win32_check_for_error (event = CreateEvent (0, FALSE, FALSE, NULL));
      TlsSetValue (g_cond_event_tls, event);
    }

  EnterCriticalSection (&cond->lock);

  /* The event must not be signaled. Check this */
  g_assert (WaitForSingleObject (event, 0) == WAIT_TIMEOUT);

  g_ptr_array_add (cond->array, event);
  LeaveCriticalSection (&cond->lock);

  g_thread_functions_for_glib_use_default.mutex_unlock (entered_mutex);

  win32_check_for_error (WAIT_FAILED !=
			 (retval = WaitForSingleObject (event, milliseconds)));

  g_thread_functions_for_glib_use_default.mutex_lock (entered_mutex);

  if (retval == WAIT_TIMEOUT)
    {
      EnterCriticalSection (&cond->lock);
      g_ptr_array_remove (cond->array, event);

      /* In the meantime we could have been signaled, so we must again
       * wait for the signal, this time with no timeout, to reset
       * it. retval is set again to honour the late arrival of the
       * signal */
      win32_check_for_error (WAIT_FAILED !=
			     (retval = WaitForSingleObject (event, 0)));

      LeaveCriticalSection (&cond->lock);
    }

#ifndef G_DISABLE_ASSERT
  EnterCriticalSection (&cond->lock);

  /* Now event must not be inside the array, check this */
  g_assert (g_ptr_array_remove (cond->array, event) == FALSE);

  LeaveCriticalSection (&cond->lock);
#endif /* !G_DISABLE_ASSERT */

  return retval != WAIT_TIMEOUT;
}

static void
g_cond_wait_win32_impl (GCond *cond,
			GMutex *entered_mutex)
{
  g_return_if_fail (cond != NULL);
  g_return_if_fail (entered_mutex != NULL);

  g_cond_wait_internal (cond, entered_mutex, INFINITE);
}

static gboolean
g_cond_timed_wait_win32_impl (GCond *cond,
			      GMutex *entered_mutex,
			      GTimeVal *abs_time)
{
  GTimeVal current_time;
  gulong to_wait;

  g_return_val_if_fail (cond != NULL, FALSE);
  g_return_val_if_fail (entered_mutex != NULL, FALSE);

  if (!abs_time)
    to_wait = INFINITE;
  else
    {
      g_get_current_time (&current_time);
      if (abs_time->tv_sec < current_time.tv_sec ||
	  (abs_time->tv_sec == current_time.tv_sec &&
	   abs_time->tv_usec <= current_time.tv_usec))
	to_wait = 0;
      else
	to_wait = (abs_time->tv_sec - current_time.tv_sec) * 1000 +
	  (abs_time->tv_usec - current_time.tv_usec) / 1000;
    }

  return g_cond_wait_internal (cond, entered_mutex, to_wait);
}

static void
g_cond_free_win32_impl (GCond * cond)
{
  DeleteCriticalSection (&cond->lock);
  g_ptr_array_free (cond->array, TRUE);
  g_free (cond);
}

static GPrivate *
g_private_new_win32_impl (GDestroyNotify destructor)
{
  GPrivate *result;
  EnterCriticalSection (&g_thread_global_spinlock);
  if (g_private_next >= G_PRIVATE_MAX)
    {
      char buf[100];
      sprintf (buf,
	       "Too many GPrivate allocated. Their number is limited to %d.",
	       G_PRIVATE_MAX);
      MessageBox (NULL, buf, NULL, MB_ICONERROR|MB_SETFOREGROUND);
      if (IsDebuggerPresent ())
	G_BREAKPOINT ();
      abort ();
    }
  g_private_destructors[g_private_next] = destructor;
  result = GUINT_TO_POINTER (g_private_next);
  g_private_next++;
  LeaveCriticalSection (&g_thread_global_spinlock);

  return result;
}

/* NOTE: the functions g_private_get and g_private_set may not use
   functions from gmem.c and gmessages.c */

static void
g_private_set_win32_impl (GPrivate * private_key, gpointer value)
{
  gpointer* array = TlsGetValue (g_private_tls);
  guint index = GPOINTER_TO_UINT (private_key);

  if (index >= G_PRIVATE_MAX)
      return;

  if (!array)
    {
      array = (gpointer*) calloc (G_PRIVATE_MAX, sizeof (gpointer));
      TlsSetValue (g_private_tls, array);
    }

  array[index] = value;
}

static gpointer
g_private_get_win32_impl (GPrivate * private_key)
{
  gpointer* array = TlsGetValue (g_private_tls);
  guint index = GPOINTER_TO_UINT (private_key);

  if (index >= G_PRIVATE_MAX || !array)
    return NULL;

  return array[index];
}

static void
g_thread_set_priority_win32_impl (gpointer thread, GThreadPriority priority)
{
  GThreadData *target = *(GThreadData **)thread;

  g_return_if_fail (priority >= G_THREAD_PRIORITY_LOW);
  g_return_if_fail (priority <= G_THREAD_PRIORITY_URGENT);

  win32_check_for_error (SetThreadPriority (target->thread,
					    g_thread_priority_map [priority]));
}

static void
g_thread_self_win32_impl (gpointer thread)
{
  GThreadData *self = TlsGetValue (g_thread_self_tls);

  if (!self)
    {
      /* This should only happen for the main thread! */
      HANDLE handle = GetCurrentThread ();
      HANDLE process = GetCurrentProcess ();
      self = g_new (GThreadData, 1);
      win32_check_for_error (DuplicateHandle (process, handle, process,
					      &self->thread, 0, FALSE,
					      DUPLICATE_SAME_ACCESS));
      win32_check_for_error (TlsSetValue (g_thread_self_tls, self));
      self->func = NULL;
      self->data = NULL;
      self->joinable = FALSE;
    }

  *(GThreadData **)thread = self;
}

static void
g_thread_exit_win32_impl (void)
{
  GThreadData *self = TlsGetValue (g_thread_self_tls);
  guint i, private_max;
  gpointer *array = TlsGetValue (g_private_tls);
  HANDLE event = TlsGetValue (g_cond_event_tls);

  EnterCriticalSection (&g_thread_global_spinlock);
  private_max = g_private_next;
  LeaveCriticalSection (&g_thread_global_spinlock);

  if (array)
    {
      gboolean some_data_non_null;

      do {
	some_data_non_null = FALSE;
	for (i = 0; i < private_max; i++)
	  {
	    GDestroyNotify destructor = g_private_destructors[i];
	    GDestroyNotify data = array[i];

	    if (data)
	      some_data_non_null = TRUE;

	    array[i] = NULL;

	    if (destructor && data)
	      destructor (data);
	  }
      } while (some_data_non_null);

      free (array);

      win32_check_for_error (TlsSetValue (g_private_tls, NULL));
    }

  if (self)
    {
      if (!self->joinable)
	{
	  win32_check_for_error (CloseHandle (self->thread));
	  g_free (self);
	}
      win32_check_for_error (TlsSetValue (g_thread_self_tls, NULL));
    }

  if (event)
    {
      CloseHandle (event);
      win32_check_for_error (TlsSetValue (g_cond_event_tls, NULL));
    }

  _endthreadex (0);
}

static guint __stdcall
g_thread_proxy (gpointer data)
{
  GThreadData *self = (GThreadData*) data;

  win32_check_for_error (TlsSetValue (g_thread_self_tls, self));

  self->func (self->data);

  g_thread_exit_win32_impl ();

  g_assert_not_reached ();

  return 0;
}

static void
g_thread_create_win32_impl (GThreadFunc func,
			    gpointer data,
			    gulong stack_size,
			    gboolean joinable,
			    gboolean bound,
			    GThreadPriority priority,
			    gpointer thread,
			    GError **error)
{
  guint ignore;
  GThreadData *retval;

  g_return_if_fail (func);
  g_return_if_fail (priority >= G_THREAD_PRIORITY_LOW);
  g_return_if_fail (priority <= G_THREAD_PRIORITY_URGENT);

  retval = g_new(GThreadData, 1);
  retval->func = func;
  retval->data = data;

  retval->joinable = joinable;

  retval->thread = (HANDLE) _beginthreadex (NULL, stack_size, g_thread_proxy,
					    retval, 0, &ignore);

  if (retval->thread == NULL)
    {
      gchar *win_error = g_win32_error_message (GetLastError ());
      g_set_error (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN,
                   "Error creating thread: %s", win_error);
      g_free (retval);
      g_free (win_error);
      return;
    }

  *(GThreadData **)thread = retval;

  g_thread_set_priority_win32_impl (thread, priority);
}

static void
g_thread_yield_win32_impl (void)
{
  Sleep(0);
}

static void
g_thread_join_win32_impl (gpointer thread)
{
  GThreadData *target = *(GThreadData **)thread;

  g_return_if_fail (target->joinable);

  win32_check_for_error (WAIT_FAILED !=
			 WaitForSingleObject (target->thread, INFINITE));

  win32_check_for_error (CloseHandle (target->thread));
  g_free (target);
}

static guint64
g_thread_gettime_impl (void)
{
  guint64 v;

  /* Returns 100s of nanoseconds since start of 1601 */
  GetSystemTimeAsFileTime ((FILETIME *)&v);

  /* Offset to Unix epoch */
  v -= G_GINT64_CONSTANT (116444736000000000);
  /* Convert to nanoseconds */
  v *= 100;

  return v;
}

static GThreadFunctions g_thread_functions_for_glib_use_default =
{
  g_mutex_new_win32_impl,           /* mutex */
  g_mutex_lock_win32_impl,
  g_mutex_trylock_win32_impl,
  g_mutex_unlock_win32_impl,
  g_mutex_free_win32_impl,
  g_cond_new_win32_impl,            /* condition */
  g_cond_signal_win32_impl,
  g_cond_broadcast_win32_impl,
  g_cond_wait_win32_impl,
  g_cond_timed_wait_win32_impl,
  g_cond_free_win32_impl,
  g_private_new_win32_impl,         /* private thread data */
  g_private_get_win32_impl,
  g_private_set_win32_impl,
  g_thread_create_win32_impl,       /* thread */
  g_thread_yield_win32_impl,
  g_thread_join_win32_impl,
  g_thread_exit_win32_impl,
  g_thread_set_priority_win32_impl,
  g_thread_self_win32_impl,
  NULL                             /* no equal function necessary */
};

#define HAVE_G_THREAD_IMPL_INIT
static void
g_thread_impl_init ()
{
  static gboolean beenhere = FALSE;
  HMODULE kernel32;

  if (beenhere)
    return;

  beenhere = TRUE;

  win32_check_for_error (TLS_OUT_OF_INDEXES !=
			 (g_thread_self_tls = TlsAlloc ()));
  win32_check_for_error (TLS_OUT_OF_INDEXES !=
			 (g_private_tls = TlsAlloc ()));
  win32_check_for_error (TLS_OUT_OF_INDEXES !=
			 (g_cond_event_tls = TlsAlloc ()));
  InitializeCriticalSection (&g_thread_global_spinlock);

  /* Here we are looking for TryEnterCriticalSection in KERNEL32.DLL,
   * if it is found, we can use the in general faster critical
   * sections instead of mutexes. See
   * http://world.std.com/~jmhart/csmutx.htm for some discussion.
   */
  kernel32 = GetModuleHandle ("KERNEL32.DLL");
  if (kernel32)
    {
      try_enter_critical_section = (GTryEnterCriticalSectionFunc)
	GetProcAddress(kernel32, "TryEnterCriticalSection");

      /* Even if TryEnterCriticalSection is found, it is not
       * necessarily working..., we have to check it */
      if (try_enter_critical_section &&
	  try_enter_critical_section (&g_thread_global_spinlock))
	{
	  LeaveCriticalSection (&g_thread_global_spinlock);

	  g_thread_functions_for_glib_use_default.mutex_new =
	    g_mutex_new_win32_cs_impl;
	  g_thread_functions_for_glib_use_default.mutex_lock =
	    g_mutex_lock_win32_cs_impl;
	  g_thread_functions_for_glib_use_default.mutex_trylock =
	    g_mutex_trylock_win32_cs_impl;
	  g_thread_functions_for_glib_use_default.mutex_unlock =
	    g_mutex_unlock_win32_cs_impl;
	  g_thread_functions_for_glib_use_default.mutex_free =
	    g_mutex_free_win32_cs_impl;
	}
    }
}
