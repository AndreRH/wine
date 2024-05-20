/*
 * Fast synchronization primitives
 *
 * Copyright (C) 2021-2022 Elizabeth Figura for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"

#include "file.h"
#include "handle.h"
#include "request.h"
#include "thread.h"

#ifdef HAVE_LINUX_NTSYNC_H

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/ntsync.h>

struct linux_device
{
    struct object obj;      /* object header */
    struct fd *fd;          /* fd for unix fd */
};

static struct linux_device *linux_device_object;

static void linux_device_dump( struct object *obj, int verbose );
static struct fd *linux_device_get_fd( struct object *obj );
static void linux_device_destroy( struct object *obj );
static enum server_fd_type fast_sync_get_fd_type( struct fd *fd );

static const struct object_ops linux_device_ops =
{
    sizeof(struct linux_device),        /* size */
    &no_type,                           /* type */
    linux_device_dump,                  /* dump */
    no_add_queue,                       /* add_queue */
    NULL,                               /* remove_queue */
    NULL,                               /* signaled */
    NULL,                               /* satisfied */
    no_signal,                          /* signal */
    linux_device_get_fd,                /* get_fd */
    default_map_access,                 /* map_access */
    default_get_sd,                     /* get_sd */
    default_set_sd,                     /* set_sd */
    no_get_full_name,                   /* get_full_name */
    no_lookup_name,                     /* lookup_name */
    no_link_name,                       /* link_name */
    NULL,                               /* unlink_name */
    no_open_file,                       /* open_file */
    no_kernel_obj_list,                 /* get_kernel_obj_list */
    no_get_fast_sync,                   /* get_fast_sync */
    no_close_handle,                    /* close_handle */
    linux_device_destroy                /* destroy */
};

static const struct fd_ops fast_sync_fd_ops =
{
    default_fd_get_poll_events,     /* get_poll_events */
    default_poll_event,             /* poll_event */
    fast_sync_get_fd_type,          /* get_fd_type */
    no_fd_read,                     /* read */
    no_fd_write,                    /* write */
    no_fd_flush,                    /* flush */
    no_fd_get_file_info,            /* get_file_info */
    no_fd_get_volume_info,          /* get_volume_info */
    no_fd_ioctl,                    /* ioctl */
    default_fd_cancel_async,        /* cancel_async */
    no_fd_queue_async,              /* queue_async */
    default_fd_reselect_async       /* reselect_async */
};

static void linux_device_dump( struct object *obj, int verbose )
{
    struct linux_device *device = (struct linux_device *)obj;
    assert( obj->ops == &linux_device_ops );
    fprintf( stderr, "Fast synchronization device fd=%p\n", device->fd );
}

static struct fd *linux_device_get_fd( struct object *obj )
{
    struct linux_device *device = (struct linux_device *)obj;
    return (struct fd *)grab_object( device->fd );
}

static void linux_device_destroy( struct object *obj )
{
    struct linux_device *device = (struct linux_device *)obj;
    assert( obj->ops == &linux_device_ops );
    if (device->fd) release_object( device->fd );
    linux_device_object = NULL;
}

static enum server_fd_type fast_sync_get_fd_type( struct fd *fd )
{
    return FD_TYPE_FILE;
}

static struct linux_device *get_linux_device(void)
{
    struct linux_device *device;
    static int initialized;
    int unix_fd;

    if (initialized)
    {
        if (linux_device_object)
            grab_object( linux_device_object );
	else
	  set_error( STATUS_NOT_IMPLEMENTED );
        return linux_device_object;
    }

    if (getenv( "WINE_DISABLE_FAST_SYNC" ) && atoi( getenv( "WINE_DISABLE_FAST_SYNC" ) ))
    {
      static int once;
        set_error( STATUS_NOT_IMPLEMENTED );
	if (!once++) fprintf(stderr, "ntsync is explicitly disabled.\n");
	initialized = 1;
        return NULL;
    }

    unix_fd = open( "/dev/ntsync", O_CLOEXEC | O_RDONLY );
    if (unix_fd == -1)
    {
      static int once;
        file_set_error();
	if (!once++) fprintf(stderr, "Cannot open /dev/ntsync: %s\n", strerror(errno));
	initialized = 1;
        return NULL;
    }

    if (!(device = alloc_object( &linux_device_ops )))
    {
        close( unix_fd );
        set_error( STATUS_NO_MEMORY );
	initialized = 1;
        return NULL;
    }

    if (!(device->fd = create_anonymous_fd( &fast_sync_fd_ops, unix_fd, &device->obj, 0 )))
    {
        release_object( device );
	initialized = 1;
        return NULL;
    }

    fprintf( stderr, "wine: using fast synchronization.\n" );
    linux_device_object = device;
    initialized = 1;
    return device;
}

struct fast_sync
{
    struct object obj;
    enum fast_sync_type type;
    struct fd *fd;
};

static void linux_obj_dump( struct object *obj, int verbose );
static void linux_obj_destroy( struct object *obj );
static struct fd *linux_obj_get_fd( struct object *obj );

static const struct object_ops linux_obj_ops =
{
    sizeof(struct fast_sync),   /* size */
    &no_type,                   /* type */
    linux_obj_dump,             /* dump */
    no_add_queue,               /* add_queue */
    NULL,                       /* remove_queue */
    NULL,                       /* signaled */
    NULL,                       /* satisfied */
    no_signal,                  /* signal */
    linux_obj_get_fd,           /* get_fd */
    default_map_access,         /* map_access */
    default_get_sd,             /* get_sd */
    default_set_sd,             /* set_sd */
    no_get_full_name,           /* get_full_name */
    no_lookup_name,             /* lookup_name */
    no_link_name,               /* link_name */
    NULL,                       /* unlink_name */
    no_open_file,               /* open_file */
    no_kernel_obj_list,         /* get_kernel_obj_list */
    no_get_fast_sync,           /* get_fast_sync */
    no_close_handle,            /* close_handle */
    linux_obj_destroy           /* destroy */
};

static void linux_obj_dump( struct object *obj, int verbose )
{
    struct fast_sync *fast_sync = (struct fast_sync *)obj;
    assert( obj->ops == &linux_obj_ops );
    fprintf( stderr, "Fast synchronization object type=%u fd=%p\n", fast_sync->type, fast_sync->fd );
}

static void linux_obj_destroy( struct object *obj )
{
    struct fast_sync *fast_sync = (struct fast_sync *)obj;
    assert( obj->ops == &linux_obj_ops );
    if (fast_sync->fd) release_object( fast_sync->fd );
}

static struct fd *linux_obj_get_fd( struct object *obj )
{
    struct fast_sync *fast_sync = (struct fast_sync *)obj;
    assert( obj->ops == &linux_obj_ops );
    return (struct fd *)grab_object( fast_sync->fd );
}

static struct fast_sync *create_fast_sync( enum fast_sync_type type, int unix_fd )
{
    struct fast_sync *fast_sync;

    if (!(fast_sync = alloc_object( &linux_obj_ops )))
    {
        close( unix_fd );
        return NULL;
    }

    fast_sync->type = type;

    if (!(fast_sync->fd = create_anonymous_fd( &fast_sync_fd_ops, unix_fd, &fast_sync->obj, 0 )))
    {
        release_object( fast_sync );
        return NULL;
    }

    return fast_sync;
}

struct fast_sync *fast_create_event( enum fast_sync_type type, int signaled )
{
    struct ntsync_event_args args = {0};
    struct linux_device *device;

    if (!(device = get_linux_device())) return NULL;

    args.signaled = signaled;
    switch (type)
    {
        case FAST_SYNC_AUTO_EVENT:
        case FAST_SYNC_AUTO_SERVER:
            args.manual = 0;
            break;

        case FAST_SYNC_MANUAL_EVENT:
        case FAST_SYNC_MANUAL_SERVER:
        case FAST_SYNC_QUEUE:
            args.manual = 1;
            break;

        case FAST_SYNC_MUTEX:
        case FAST_SYNC_SEMAPHORE:
            assert(0);
            break;
    }
    if (ioctl( get_unix_fd( device->fd ), NTSYNC_IOC_CREATE_EVENT, &args ) < 0)
    {
        file_set_error();
        release_object( device );
        return NULL;
    }
    release_object( device );

    return create_fast_sync( type, args.event );
}

struct fast_sync *fast_create_semaphore( unsigned int count, unsigned int max )
{
    struct ntsync_sem_args args = {0};
    struct linux_device *device;

    if (!(device = get_linux_device())) return NULL;

    args.count = count;
    args.max = max;
    if (ioctl( get_unix_fd( device->fd ), NTSYNC_IOC_CREATE_SEM, &args ) < 0)
    {
        file_set_error();
        release_object( device );
        return NULL;
    }

    release_object( device );

    return create_fast_sync( FAST_SYNC_SEMAPHORE, args.sem );
}

struct fast_sync *fast_create_mutex( thread_id_t owner, unsigned int count )
{
    struct ntsync_mutex_args args = {0};
    struct linux_device *device;

    if (!(device = get_linux_device())) return NULL;

    args.owner = owner;
    args.count = count;
    if (ioctl( get_unix_fd( device->fd ), NTSYNC_IOC_CREATE_MUTEX, &args ) < 0)
    {
        file_set_error();
        release_object( device );
        return NULL;
    }

    release_object( device );

    return create_fast_sync( FAST_SYNC_MUTEX, args.mutex );
}

void fast_set_event( struct fast_sync *fast_sync )
{
    __u32 count;

    if (!fast_sync) return;

    if (debug_level) fprintf( stderr, "fast_set_event %p\n", fast_sync->fd );

    ioctl( get_unix_fd( fast_sync->fd ), NTSYNC_IOC_EVENT_SET, &count );
}

void fast_reset_event( struct fast_sync *fast_sync )
{
    __u32 count;

    if (!fast_sync) return;

    if (debug_level) fprintf( stderr, "fast_set_event %p\n", fast_sync->fd );

    ioctl( get_unix_fd( fast_sync->fd ), NTSYNC_IOC_EVENT_RESET, &count );
}

void fast_abandon_mutex( thread_id_t tid, struct fast_sync *fast_sync )
{
    ioctl( get_unix_fd( fast_sync->fd ), NTSYNC_IOC_MUTEX_KILL, &tid );
}

#else

struct fast_sync *fast_create_event( enum fast_sync_type type, int signaled )
{
    set_error( STATUS_NOT_IMPLEMENTED );
    return NULL;
}

struct fast_sync *fast_create_semaphore( unsigned int count, unsigned int max )
{
    set_error( STATUS_NOT_IMPLEMENTED );
    return NULL;
}

struct fast_sync *fast_create_mutex( thread_id_t owner, unsigned int count )
{
    set_error( STATUS_NOT_IMPLEMENTED );
    return NULL;
}

void fast_set_event( struct fast_sync *fast_sync )
{
}

void fast_reset_event( struct fast_sync *obj )
{
}

void fast_abandon_mutex( thread_id_t tid, struct fast_sync *fast_sync )
{
}

#endif

DECL_HANDLER(get_linux_sync_device)
{
#ifdef HAVE_LINUX_NTSYNC_H
    struct linux_device *device;

    if ((device = get_linux_device()))
    {
        reply->handle = alloc_handle( current->process, device, 0, 0 );
        release_object( device );
    }
#else
    set_error( STATUS_NOT_IMPLEMENTED );
#endif
}

DECL_HANDLER(get_linux_sync_obj)
{
#ifdef HAVE_LINUX_NTSYNC_H
    struct object *obj;

    if ((obj = get_handle_obj( current->process, req->handle, 0, NULL )))
    {
        struct fast_sync *fast_sync;

        if ((fast_sync = obj->ops->get_fast_sync( obj )))
        {
            reply->handle = alloc_handle( current->process, fast_sync, 0, 0 );
            reply->type = fast_sync->type;
            reply->access = get_handle_access( current->process, req->handle );
            release_object( fast_sync );
        }
        release_object( obj );
    }
#else
    set_error( STATUS_NOT_IMPLEMENTED );
#endif
}
