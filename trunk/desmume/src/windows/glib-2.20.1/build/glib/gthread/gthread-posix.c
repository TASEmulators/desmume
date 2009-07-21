/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: posix thread system implementation
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
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

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#define posix_check_err(err, name) G_STMT_START{			\
  int error = (err); 							\
  if (error)	 		 		 			\
    g_error ("file %s: line %d (%s): error '%s' during '%s'",		\
           __FILE__, __LINE__, G_STRFUNC,				\
           g_strerror (error), name);					\
  }G_STMT_END

#define posix_check_cmd(cmd) posix_check_err (posix_error (cmd), #cmd)

#ifdef G_ENABLE_DEBUG
static gboolean posix_check_cmd_prio_warned = FALSE;
# define posix_check_cmd_prio(cmd) G_STMT_START{			\
    int err = posix_error (cmd);					\
    if (err == EPERM)		 		 			\
      { 	 			 				\
        if (!posix_check_cmd_prio_warned) 		 		\
          { 	 				 			\
            posix_check_cmd_prio_warned = TRUE;		 		\
            g_warning ("Priorities can only be changed " 		\
                        "(resp. increased) by root."); 			\
          }			 					\
      }			 						\
    else  		 						\
      posix_check_err (err, #cmd);					\
     }G_STMT_END
#else /* G_ENABLE_DEBUG */
# define posix_check_cmd_prio(cmd) G_STMT_START{			\
    int err = posix_error (cmd);					\
    if (err != EPERM)		 		 			\
      posix_check_err (err, #cmd);					\
     }G_STMT_END
#endif /* G_ENABLE_DEBUG */

#if defined(G_THREADS_IMPL_POSIX)
# define posix_error(what) (what)
# define mutexattr_default NULL
# define condattr_default NULL
#elif defined(G_THREADS_IMPL_DCE)
# define posix_error(what) ((what) == -1 ? errno : 0)
# define pthread_key_create(a, b) pthread_keycreate (a, b)
# define pthread_attr_init(a) pthread_attr_create (a)
# define pthread_attr_destroy(a) pthread_attr_delete (a)
# define pthread_create(a, b, c, d) pthread_create (a, *b, c, d)
# define mutexattr_default (pthread_mutexattr_default)
# define condattr_default (pthread_condattr_default)
#else /* neither G_THREADS_IMPL_POSIX nor G_THREADS_IMPL_DCE are defined */
# error This should not happen. Contact the GLib team.
#endif

#if defined (POSIX_MIN_PRIORITY) && defined (POSIX_MAX_PRIORITY)
# define HAVE_PRIORITIES 1
static gint priority_normal_value;
# ifdef __FreeBSD__
   /* FreeBSD threads use different priority values from the POSIX_
    * defines so we just set them here. The corresponding macros
    * PTHREAD_MIN_PRIORITY and PTHREAD_MAX_PRIORITY are implied to be
    * exported by the docs, but they aren't.
    */
#  define PRIORITY_LOW_VALUE      0
#  define PRIORITY_URGENT_VALUE   31
# else /* !__FreeBSD__ */
#  define PRIORITY_LOW_VALUE      POSIX_MIN_PRIORITY
#  define PRIORITY_URGENT_VALUE   POSIX_MAX_PRIORITY
# endif /* !__FreeBSD__ */
# define PRIORITY_NORMAL_VALUE    priority_normal_value
#endif /* POSIX_MIN_PRIORITY && POSIX_MAX_PRIORITY */

static gulong g_thread_min_stack_size = 0;

#define G_MUTEX_SIZE (sizeof (pthread_mutex_t))

#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_MONOTONIC_CLOCK) 
#define USE_CLOCK_GETTIME 1
static gint posix_clock = 0;
#endif

#if defined(_SC_THREAD_STACK_MIN) || defined (HAVE_PRIORITIES) || defined (USE_CLOCK_GETTIME)
#define HAVE_G_THREAD_IMPL_INIT
static void
g_thread_impl_init(void)
{
#ifdef _SC_THREAD_STACK_MIN
  g_thread_min_stack_size = MAX (sysconf (_SC_THREAD_STACK_MIN), 0);
#endif /* _SC_THREAD_STACK_MIN */
#ifdef HAVE_PRIORITIES
# ifdef G_THREADS_IMPL_POSIX
  {
    struct sched_param sched;
    int policy;
    posix_check_cmd (pthread_getschedparam (pthread_self(), &policy, &sched));
    priority_normal_value = sched.sched_priority;
  }
# else /* G_THREADS_IMPL_DCE */
  posix_check_cmd (priority_normal_value =
		   pthread_getprio (*(pthread_t*)thread,
				    g_thread_priority_map [priority]));
# endif
#endif /* HAVE_PRIORITIES */

#ifdef USE_CLOCK_GETTIME
 if (sysconf (_SC_MONOTONIC_CLOCK) >= 0)
   posix_clock = CLOCK_MONOTONIC;
 else
   posix_clock = CLOCK_REALTIME;
#endif
}
#endif /* _SC_THREAD_STACK_MIN || HAVE_PRIORITIES */

static GMutex *
g_mutex_new_posix_impl (void)
{
  GMutex *result = (GMutex *) g_new (pthread_mutex_t, 1);
  posix_check_cmd (pthread_mutex_init ((pthread_mutex_t *) result,
				       mutexattr_default));
  return result;
}

static void
g_mutex_free_posix_impl (GMutex * mutex)
{
  posix_check_cmd (pthread_mutex_destroy ((pthread_mutex_t *) mutex));
  g_free (mutex);
}

/* NOTE: the functions g_mutex_lock and g_mutex_unlock may not use
   functions from gmem.c and gmessages.c; */

/* pthread_mutex_lock, pthread_mutex_unlock can be taken directly, as
   signature and semantic are right, but without error check then!!!!,
   we might want to change this therefore. */

static gboolean
g_mutex_trylock_posix_impl (GMutex * mutex)
{
  int result;

  result = pthread_mutex_trylock ((pthread_mutex_t *) mutex);

#ifdef G_THREADS_IMPL_POSIX
  if (result == EBUSY)
    return FALSE;
#else /* G_THREADS_IMPL_DCE */
  if (result == 0)
    return FALSE;
#endif

  posix_check_err (posix_error (result), "pthread_mutex_trylock");
  return TRUE;
}

static GCond *
g_cond_new_posix_impl (void)
{
  GCond *result = (GCond *) g_new (pthread_cond_t, 1);
  posix_check_cmd (pthread_cond_init ((pthread_cond_t *) result,
				      condattr_default));
  return result;
}

/* pthread_cond_signal, pthread_cond_broadcast and pthread_cond_wait
   can be taken directly, as signature and semantic are right, but
   without error check then!!!!, we might want to change this
   therefore. */

#define G_NSEC_PER_SEC 1000000000

static gboolean
g_cond_timed_wait_posix_impl (GCond * cond,
			      GMutex * entered_mutex,
			      GTimeVal * abs_time)
{
  int result;
  struct timespec end_time;
  gboolean timed_out;

  g_return_val_if_fail (cond != NULL, FALSE);
  g_return_val_if_fail (entered_mutex != NULL, FALSE);

  if (!abs_time)
    {
      result = pthread_cond_wait ((pthread_cond_t *)cond,
                                  (pthread_mutex_t *) entered_mutex);
      timed_out = FALSE;
    }
  else
    {
      end_time.tv_sec = abs_time->tv_sec;
      end_time.tv_nsec = abs_time->tv_usec * (G_NSEC_PER_SEC / G_USEC_PER_SEC);

      g_return_val_if_fail (end_time.tv_nsec < G_NSEC_PER_SEC, TRUE);

      result = pthread_cond_timedwait ((pthread_cond_t *) cond,
                                       (pthread_mutex_t *) entered_mutex,
                                       &end_time);
#ifdef G_THREADS_IMPL_POSIX
      timed_out = (result == ETIMEDOUT);
#else /* G_THREADS_IMPL_DCE */
      timed_out = (result == -1) && (errno == EAGAIN);
#endif
    }

  if (!timed_out)
    posix_check_err (posix_error (result), "pthread_cond_timedwait");

  return !timed_out;
}

static void
g_cond_free_posix_impl (GCond * cond)
{
  posix_check_cmd (pthread_cond_destroy ((pthread_cond_t *) cond));
  g_free (cond);
}

static GPrivate *
g_private_new_posix_impl (GDestroyNotify destructor)
{
  GPrivate *result = (GPrivate *) g_new (pthread_key_t, 1);
  posix_check_cmd (pthread_key_create ((pthread_key_t *) result, destructor));
  return result;
}

/* NOTE: the functions g_private_get and g_private_set may not use
   functions from gmem.c and gmessages.c */

static void
g_private_set_posix_impl (GPrivate * private_key, gpointer value)
{
  if (!private_key)
    return;
  pthread_setspecific (*(pthread_key_t *) private_key, value);
}

static gpointer
g_private_get_posix_impl (GPrivate * private_key)
{
  if (!private_key)
    return NULL;
#ifdef G_THREADS_IMPL_POSIX
  return pthread_getspecific (*(pthread_key_t *) private_key);
#else /* G_THREADS_IMPL_DCE */
  {
    void* data;
    posix_check_cmd (pthread_getspecific (*(pthread_key_t *) private_key,
					  &data));
    return data;
  }
#endif
}

static void
g_thread_create_posix_impl (GThreadFunc thread_func,
			    gpointer arg,
			    gulong stack_size,
			    gboolean joinable,
			    gboolean bound,
			    GThreadPriority priority,
			    gpointer thread,
			    GError **error)
{
  pthread_attr_t attr;
  gint ret;

  g_return_if_fail (thread_func);
  g_return_if_fail (priority >= G_THREAD_PRIORITY_LOW);
  g_return_if_fail (priority <= G_THREAD_PRIORITY_URGENT);

  posix_check_cmd (pthread_attr_init (&attr));

#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
  if (stack_size)
    {
      stack_size = MAX (g_thread_min_stack_size, stack_size);
      /* No error check here, because some systems can't do it and
       * we simply don't want threads to fail because of that. */
      pthread_attr_setstacksize (&attr, stack_size);
    }
#endif /* HAVE_PTHREAD_ATTR_SETSTACKSIZE */

#ifdef PTHREAD_SCOPE_SYSTEM
  if (bound)
    /* No error check here, because some systems can't do it and we
     * simply don't want threads to fail because of that. */
    pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
#endif /* PTHREAD_SCOPE_SYSTEM */

#ifdef G_THREADS_IMPL_POSIX
  posix_check_cmd (pthread_attr_setdetachstate (&attr,
          joinable ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED));
#endif /* G_THREADS_IMPL_POSIX */

#ifdef HAVE_PRIORITIES
# ifdef G_THREADS_IMPL_POSIX
  {
    struct sched_param sched;
    posix_check_cmd (pthread_attr_getschedparam (&attr, &sched));
    sched.sched_priority = g_thread_priority_map [priority];
    posix_check_cmd_prio (pthread_attr_setschedparam (&attr, &sched));
  }
# else /* G_THREADS_IMPL_DCE */
  posix_check_cmd_prio
    (pthread_attr_setprio (&attr, g_thread_priority_map [priority]));
# endif /* G_THREADS_IMPL_DCE */
#endif /* HAVE_PRIORITIES */
  ret = posix_error (pthread_create (thread, &attr,
				     (void* (*)(void*))thread_func, arg));

  posix_check_cmd (pthread_attr_destroy (&attr));

  if (ret == EAGAIN)
    {
      g_set_error (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN, 
		   "Error creating thread: %s", g_strerror (ret));
      return;
    }

  posix_check_err (ret, "pthread_create");

#ifdef G_THREADS_IMPL_DCE
  if (!joinable)
    posix_check_cmd (pthread_detach (thread));
#endif /* G_THREADS_IMPL_DCE */
}

static void
g_thread_yield_posix_impl (void)
{
  POSIX_YIELD_FUNC;
}

static void
g_thread_join_posix_impl (gpointer thread)
{
  gpointer ignore;
  posix_check_cmd (pthread_join (*(pthread_t*)thread, &ignore));
}

static void
g_thread_exit_posix_impl (void)
{
  pthread_exit (NULL);
}

static void
g_thread_set_priority_posix_impl (gpointer thread, GThreadPriority priority)
{
  g_return_if_fail (priority >= G_THREAD_PRIORITY_LOW);
  g_return_if_fail (priority <= G_THREAD_PRIORITY_URGENT);
#ifdef HAVE_PRIORITIES
# ifdef G_THREADS_IMPL_POSIX
  {
    struct sched_param sched;
    int policy;
    posix_check_cmd (pthread_getschedparam (*(pthread_t*)thread, &policy,
					    &sched));
    sched.sched_priority = g_thread_priority_map [priority];
    posix_check_cmd_prio (pthread_setschedparam (*(pthread_t*)thread, policy,
						 &sched));
  }
# else /* G_THREADS_IMPL_DCE */
  posix_check_cmd_prio (pthread_setprio (*(pthread_t*)thread,
					 g_thread_priority_map [priority]));
# endif
#endif /* HAVE_PRIORITIES */
}

static void
g_thread_self_posix_impl (gpointer thread)
{
  *(pthread_t*)thread = pthread_self();
}

static gboolean
g_thread_equal_posix_impl (gpointer thread1, gpointer thread2)
{
  return (pthread_equal (*(pthread_t*)thread1, *(pthread_t*)thread2) != 0);
}

#ifdef USE_CLOCK_GETTIME 
static guint64
gettime (void)
{
  struct timespec tv;

  clock_gettime (posix_clock, &tv);

  return (guint64) tv.tv_sec * G_NSEC_PER_SEC + tv.tv_nsec;
}
static guint64 (*g_thread_gettime_impl)(void) = gettime;
#else
static guint64 (*g_thread_gettime_impl)(void) = 0;
#endif

static GThreadFunctions g_thread_functions_for_glib_use_default =
{
  g_mutex_new_posix_impl,
  (void (*)(GMutex *)) pthread_mutex_lock,
  g_mutex_trylock_posix_impl,
  (void (*)(GMutex *)) pthread_mutex_unlock,
  g_mutex_free_posix_impl,
  g_cond_new_posix_impl,
  (void (*)(GCond *)) pthread_cond_signal,
  (void (*)(GCond *)) pthread_cond_broadcast,
  (void (*)(GCond *, GMutex *)) pthread_cond_wait,
  g_cond_timed_wait_posix_impl,
  g_cond_free_posix_impl,
  g_private_new_posix_impl,
  g_private_get_posix_impl,
  g_private_set_posix_impl,
  g_thread_create_posix_impl,
  g_thread_yield_posix_impl,
  g_thread_join_posix_impl,
  g_thread_exit_posix_impl,
  g_thread_set_priority_posix_impl,
  g_thread_self_posix_impl,
  g_thread_equal_posix_impl
};
