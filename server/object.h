/*
 * Wine server objects
 *
 * Copyright (C) 1998 Alexandre Julliard
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __WINE_SERVER_OBJECT_H
#define __WINE_SERVER_OBJECT_H

#include <sys/poll.h>
#include <sys/time.h>
#include "wine/server_protocol.h"

#define DEBUG_OBJECTS

/* kernel objects */

struct object;
struct object_name;
struct thread;
struct process;
struct file;
struct wait_queue_entry;
struct async;
struct async_queue;

/* operations valid on all objects */
struct object_ops
{
    /* size of this object type */
    size_t size;
    /* dump the object (for debugging) */
    void (*dump)(struct object *,int);
    /* add a thread to the object wait queue */
    int  (*add_queue)(struct object *,struct wait_queue_entry *);
    /* remove a thread from the object wait queue */
    void (*remove_queue)(struct object *,struct wait_queue_entry *);
    /* is object signaled? */
    int  (*signaled)(struct object *,struct thread *);
    /* wait satisfied; return 1 if abandoned */
    int  (*satisfied)(struct object *,struct thread *);
    /* get the events we want to poll() for on this object */
    int  (*get_poll_events)(struct object *);
    /* a poll() event occured */
    void (*poll_event)(struct object *,int event);
    /* return a Unix fd that can be used to read/write from the object */
    int  (*get_fd)(struct object *);
    /* flush the object buffers */
    int  (*flush)(struct object *);
    /* get file information */
    int  (*get_file_info)(struct object *,struct get_file_info_reply *, int *flags);
    /* queue an async operation - see register_async handler in async.c*/
    void (*queue_async)(struct object *, void* ptr, unsigned int status, int type, int count);
    /* destroy on refcount == 0 */
    void (*destroy)(struct object *);
};

struct object
{
    unsigned int              refcount;    /* reference count */
    int                       fd;          /* file descriptor */
    int                       select;      /* select() user id */
    const struct object_ops  *ops;
    struct wait_queue_entry  *head;
    struct wait_queue_entry  *tail;
    struct object_name       *name;
#ifdef DEBUG_OBJECTS
    struct object            *prev;
    struct object            *next;
#endif
};

struct wait_queue_entry
{
    struct wait_queue_entry *next;
    struct wait_queue_entry *prev;
    struct object           *obj;
    struct thread           *thread;
};

extern void *mem_alloc( size_t size );  /* malloc wrapper */
extern void *memdup( const void *data, size_t len );
extern void *alloc_object( const struct object_ops *ops, int fd );
extern void dump_object_name( struct object *obj );
extern void *create_named_object( const struct object_ops *ops, const WCHAR *name, size_t len );
/* grab/release_object can take any pointer, but you better make sure */
/* that the thing pointed to starts with a struct object... */
extern struct object *grab_object( void *obj );
extern void release_object( void *obj );
extern struct object *find_object( const WCHAR *name, size_t len );
extern int no_add_queue( struct object *obj, struct wait_queue_entry *entry );
extern int no_satisfied( struct object *obj, struct thread *thread );
extern int no_get_fd( struct object *obj );
extern int no_flush( struct object *obj );
extern int no_get_file_info( struct object *obj, struct get_file_info_reply *info, int *flags );
extern void no_destroy( struct object *obj );
extern int default_poll_add_queue( struct object *obj, struct wait_queue_entry *entry );
extern void default_poll_remove_queue( struct object *obj, struct wait_queue_entry *entry );
extern int default_poll_signaled( struct object *obj, struct thread *thread );
extern void default_poll_event( struct object *obj, int event );
#ifdef DEBUG_OBJECTS
extern void dump_objects(void);
#endif

/* select functions */

extern int add_select_user( struct object *obj );
extern void remove_select_user( struct object *obj );
extern void change_select_fd( struct object *obj, int fd, int events );
extern void set_select_events( struct object *obj, int events );
extern int check_select_events( int fd, int events );
extern void select_loop(void);

/* timeout functions */

struct timeout_user;

typedef void (*timeout_callback)( void *private );

extern struct timeout_user *add_timeout_user( struct timeval *when,
                                              timeout_callback func, void *private );
extern void remove_timeout_user( struct timeout_user *user );
extern void add_timeout( struct timeval *when, int timeout );
/* return 1 if t1 is before t2 */
static inline int time_before( struct timeval *t1, struct timeval *t2 )
{
    return ((t1->tv_sec < t2->tv_sec) ||
            ((t1->tv_sec == t2->tv_sec) && (t1->tv_usec < t2->tv_usec)));
}

/* event functions */

struct event;

extern struct event *create_event( const WCHAR *name, size_t len,
                                   int manual_reset, int initial_state );
extern struct event *get_event_obj( struct process *process, handle_t handle, unsigned int access );
extern void pulse_event( struct event *event );
extern void set_event( struct event *event );
extern void reset_event( struct event *event );

/* mutex functions */

extern void abandon_mutexes( struct thread *thread );

/* file functions */

extern struct file *get_file_obj( struct process *process, handle_t handle,
                                  unsigned int access );
extern int is_same_file( struct file *file1, struct file *file2 );
extern int get_file_drive_type( struct file *file );
extern int grow_file( struct file *file, int size_high, int size_low );
extern int create_anonymous_file(void);
extern struct file *create_temp_file( int access );
extern void file_set_error(void);

/* serial functions */

int get_serial_async_timeout(struct object *obj, int type, int count);

/* debugger functions */

extern int set_process_debugger( struct process *process, struct thread *debugger );
extern void generate_debug_event( struct thread *thread, int code, void *arg );
extern void generate_startup_debug_events( struct process *process, void *entry );
extern void debug_exit_thread( struct thread *thread );

/* mapping functions */

extern int get_page_size(void);

/* registry functions */

extern void init_registry(void);
extern void close_registry(void);

/* atom functions */

extern void close_atom_table(void);
extern int grab_global_atom( atom_t atom );
extern void release_global_atom( atom_t atom );

/* global variables */

  /* command-line options */
extern int debug_level;
extern int master_socket_timeout;

  /* server start time used for GetTickCount() */
extern unsigned int server_start_ticks;

#endif  /* __WINE_SERVER_OBJECT_H */
