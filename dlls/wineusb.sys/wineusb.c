/*
 * USB root device enumerator using libusb
 *
 * Copyright 2020 Zebediah Figura
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

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <libusb.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winioctl.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "ddk/usb.h"
#include "ddk/usbioctl.h"
#include "wine/asm.h"
#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unicode.h"

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(wineusb);

#ifdef __ASM_USE_FASTCALL_WRAPPER

extern void * WINAPI wrap_fastcall_func1( void *func, const void *a );
__ASM_STDCALL_FUNC( wrap_fastcall_func1, 8,
                   "popl %ecx\n\t"
                   "popl %eax\n\t"
                   "xchgl (%esp),%ecx\n\t"
                   "jmp *%eax" );

#define call_fastcall_func1(func,a) wrap_fastcall_func1(func,a)

#else

#define call_fastcall_func1(func,a) func(a)

#endif

#define DECLARE_CRITICAL_SECTION(cs) \
    static CRITICAL_SECTION cs; \
    static CRITICAL_SECTION_DEBUG cs##_debug = \
    { 0, 0, &cs, { &cs##_debug.ProcessLocksList, &cs##_debug.ProcessLocksList }, \
      0, 0, { (DWORD_PTR)(__FILE__ ": " # cs) }}; \
    static CRITICAL_SECTION cs = { &cs##_debug, -1, 0, 0, 0, 0 };

DECLARE_CRITICAL_SECTION(wineusb_cs);

static unixlib_handle_t unix_handle;

static pthread_mutex_t unix_device_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct list unix_device_list = LIST_INIT(unix_device_list);
static struct list device_list = LIST_INIT(device_list);

struct unix_device
{
    struct list entry;

    libusb_device_handle *handle;
};

struct usb_device
{
    struct list entry;
    BOOL removed;

    DEVICE_OBJECT *device_obj;

    /* Points to the parent USB device if this is a USB interface; otherwise
     * NULL. */
    struct usb_device *parent;
    uint8_t interface_index;

    uint8_t class, subclass, protocol;

    uint16_t vendor, product, revision;

    struct unix_device *unix_device;

    LIST_ENTRY irp_list;
};

static DRIVER_OBJECT *driver_obj;
static DEVICE_OBJECT *bus_fdo, *bus_pdo;

static libusb_hotplug_callback_handle hotplug_cb_handle;

static pthread_cond_t event_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;

enum usb_event_type
{
    USB_EVENT_ADD_DEVICE,
    USB_EVENT_REMOVE_DEVICE,
    USB_EVENT_TRANSFER_COMPLETE,
    USB_EVENT_SHUTDOWN,
};

struct usb_event
{
    enum usb_event_type type;

    union
    {
        struct unix_device *added_device;
        struct unix_device *removed_device;
        IRP *completed_irp;
    } u;
};

static struct usb_event *usb_events;
static size_t usb_event_count, usb_events_capacity;

static bool array_reserve(void **elements, size_t *capacity, size_t count, size_t size)
{
    unsigned int new_capacity, max_capacity;
    void *new_elements;

    if (count <= *capacity)
        return true;

    max_capacity = ~(size_t)0 / size;
    if (count > max_capacity)
        return false;

    new_capacity = max(4, *capacity);
    while (new_capacity < count && new_capacity <= max_capacity / 2)
        new_capacity *= 2;
    if (new_capacity < count)
        new_capacity = max_capacity;

    if (!(new_elements = realloc(*elements, new_capacity * size)))
        return false;

    *elements = new_elements;
    *capacity = new_capacity;

    return true;
}

static void queue_event(const struct usb_event *event)
{
    pthread_mutex_lock(&event_mutex);
    if (array_reserve((void **)&usb_events, &usb_events_capacity, usb_event_count + 1, sizeof(*usb_events)))
        usb_events[usb_event_count++] = *event;
    else
        ERR("Failed to queue event.\n");
    pthread_mutex_unlock(&event_mutex);
    pthread_cond_signal(&event_cond);
}

static void get_event(struct usb_event *event)
{
    pthread_mutex_lock(&event_mutex);
    while (!usb_event_count)
        pthread_cond_wait(&event_cond, &event_mutex);
    *event = usb_events[0];
    if (--usb_event_count)
        memmove(usb_events, usb_events + 1, usb_event_count * sizeof(*usb_events));
    pthread_mutex_unlock(&event_mutex);
}

static void destroy_unix_device(struct unix_device *unix_device)
{
    pthread_mutex_lock(&unix_device_mutex);
    libusb_close(unix_device->handle);
    list_remove(&unix_device->entry);
    pthread_mutex_unlock(&unix_device_mutex);
    free(unix_device);
}

static void add_usb_interface(struct usb_device *parent, const struct libusb_interface_descriptor *desc)
{
    struct usb_device *device;
    DEVICE_OBJECT *device_obj;
    NTSTATUS status;

    if ((status = IoCreateDevice(driver_obj, sizeof(*device), NULL,
            FILE_DEVICE_USB, FILE_AUTOGENERATED_DEVICE_NAME, FALSE, &device_obj)))
    {
        ERR("Failed to create device, status %#x.\n", status);
        return;
    }

    device = device_obj->DeviceExtension;
    device->device_obj = device_obj;
    device->parent = parent;
    device->unix_device = parent->unix_device;
    device->interface_index = desc->bInterfaceNumber;
    device->class = desc->bInterfaceClass;
    device->subclass = desc->bInterfaceSubClass;
    device->protocol = desc->bInterfaceProtocol;
    device->vendor = parent->vendor;
    device->product = parent->product;
    device->revision = parent->revision;
    InitializeListHead(&device->irp_list);

    EnterCriticalSection(&wineusb_cs);
    list_add_tail(&device_list, &device->entry);
    LeaveCriticalSection(&wineusb_cs);
}

static void add_unix_device(struct unix_device *unix_device)
{
    static const WCHAR formatW[] = {'\\','D','e','v','i','c','e','\\','U','S','B','P','D','O','-','%','u',0};
    libusb_device *libusb_device = libusb_get_device(unix_device->handle);
    struct libusb_config_descriptor *config_desc;
    struct libusb_device_descriptor device_desc;
    static unsigned int name_index;
    struct usb_device *device;
    DEVICE_OBJECT *device_obj;
    UNICODE_STRING string;
    NTSTATUS status;
    WCHAR name[26];
    int ret;

    libusb_get_device_descriptor(libusb_device, &device_desc);

    TRACE("Adding new device %p, vendor %04x, product %04x.\n", unix_device,
            device_desc.idVendor, device_desc.idProduct);

    sprintfW(name, formatW, name_index++);
    RtlInitUnicodeString(&string, name);
    if ((status = IoCreateDevice(driver_obj, sizeof(*device), &string,
            FILE_DEVICE_USB, 0, FALSE, &device_obj)))
    {
        ERR("Failed to create device, status %#x.\n", status);
        return;
    }

    device = device_obj->DeviceExtension;
    device->device_obj = device_obj;
    device->unix_device = unix_device;
    InitializeListHead(&device->irp_list);
    device->removed = FALSE;

    device->class = device_desc.bDeviceClass;
    device->subclass = device_desc.bDeviceSubClass;
    device->protocol = device_desc.bDeviceProtocol;
    device->vendor = device_desc.idVendor;
    device->product = device_desc.idProduct;
    device->revision = device_desc.bcdDevice;

    EnterCriticalSection(&wineusb_cs);
    list_add_tail(&device_list, &device->entry);
    LeaveCriticalSection(&wineusb_cs);

    if (!(ret = libusb_get_active_config_descriptor(libusb_device, &config_desc)))
    {
        /* Create new devices for interfaces of composite devices.
         *
         * On Windows this is the job of usbccgp.sys, a separate driver that
         * layers on top of the base USB driver. While we could take this
         * approach as well, implementing usbccgp is a lot more work, whereas
         * interface support is effectively built into libusb.
         *
         * FIXME: usbccgp does not create separate interfaces in some cases,
         * e.g. when there is an interface association descriptor available.
         */
        if (config_desc->bNumInterfaces > 1)
        {
            uint8_t i;

            for (i = 0; i < config_desc->bNumInterfaces; ++i)
            {
                const struct libusb_interface *interface = &config_desc->interface[i];

                if (interface->num_altsetting != 1)
                    FIXME("Interface %u has %u alternate settings; using the first one.\n",
                            i, interface->num_altsetting);
                add_usb_interface(device, &interface->altsetting[0]);
            }
        }
        libusb_free_config_descriptor(config_desc);
    }
    else
    {
        ERR("Failed to get configuration descriptor: %s\n", libusb_strerror(ret));
    }

    IoInvalidateDeviceRelations(bus_pdo, BusRelations);
}

static void add_usb_device(libusb_device *libusb_device)
{
    struct libusb_device_descriptor device_desc;
    struct unix_device *unix_device;
    struct usb_event usb_event;
    int ret;

    libusb_get_device_descriptor(libusb_device, &device_desc);

    TRACE("Adding new device %p, vendor %04x, product %04x.\n", libusb_device,
            device_desc.idVendor, device_desc.idProduct);

    if (!(unix_device = calloc(1, sizeof(*unix_device))))
        return;

    if ((ret = libusb_open(libusb_device, &unix_device->handle)))
    {
        WARN("Failed to open device: %s\n", libusb_strerror(ret));
        free(unix_device);
        return;
    }
    pthread_mutex_lock(&unix_device_mutex);
    list_add_tail(&unix_device_list, &unix_device->entry);
    pthread_mutex_unlock(&unix_device_mutex);

    usb_event.type = USB_EVENT_ADD_DEVICE;
    usb_event.u.added_device = unix_device;
    queue_event(&usb_event);
}

static void remove_unix_device(struct unix_device *unix_device)
{
    struct usb_device *device;

    TRACE("Removing device %p.\n", unix_device);

    EnterCriticalSection(&wineusb_cs);
    LIST_FOR_EACH_ENTRY(device, &device_list, struct usb_device, entry)
    {
        if (device->unix_device == unix_device)
        {
            if (!device->removed)
            {
                device->removed = TRUE;
                list_remove(&device->entry);
            }
            break;
        }
    }
    LeaveCriticalSection(&wineusb_cs);

    IoInvalidateDeviceRelations(bus_pdo, BusRelations);
}

static void remove_usb_device(libusb_device *libusb_device)
{
    struct unix_device *unix_device;
    struct usb_event usb_event;

    TRACE("Removing device %p.\n", libusb_device);

    LIST_FOR_EACH_ENTRY(unix_device, &unix_device_list, struct unix_device, entry)
    {
        if (libusb_get_device(unix_device->handle) == libusb_device)
        {
            usb_event.type = USB_EVENT_REMOVE_DEVICE;
            usb_event.u.removed_device = unix_device;
            queue_event(&usb_event);
        }
    }
}

static BOOL thread_shutdown;
static HANDLE libusb_event_thread, event_thread;

static int LIBUSB_CALL hotplug_cb(libusb_context *context, libusb_device *device,
        libusb_hotplug_event event, void *user_data)
{
    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
        add_usb_device(device);
    else
        remove_usb_device(device);

    return 0;
}

static DWORD CALLBACK libusb_event_thread_proc(void *arg)
{
    static const struct usb_event shutdown_event = {.type = USB_EVENT_SHUTDOWN};
    int ret;

    TRACE("Starting libusb event thread.\n");

    while (!thread_shutdown)
    {
        if ((ret = libusb_handle_events(NULL)))
            ERR("Error handling events: %s\n", libusb_strerror(ret));
    }

    queue_event(&shutdown_event);

    TRACE("Shutting down libusb event thread.\n");
    return 0;
}

static void complete_irp(IRP *irp)
{
    EnterCriticalSection(&wineusb_cs);
    RemoveEntryList(&irp->Tail.Overlay.ListEntry);
    LeaveCriticalSection(&wineusb_cs);

    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
}

static DWORD CALLBACK event_thread_proc(void *arg)
{
    struct usb_event event;

    TRACE("Starting client event thread.\n");

    for (;;)
    {
        get_event(&event);

        switch (event.type)
        {
            case USB_EVENT_ADD_DEVICE:
                add_unix_device(event.u.added_device);
                break;

            case USB_EVENT_REMOVE_DEVICE:
                remove_unix_device(event.u.removed_device);
                break;

            case USB_EVENT_TRANSFER_COMPLETE:
                complete_irp(event.u.completed_irp);
                break;

            case USB_EVENT_SHUTDOWN:
                TRACE("Shutting down client event thread.\n");
                return 0;
        }
    }
}

static NTSTATUS fdo_pnp(IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS ret;

    TRACE("irp %p, minor function %#x.\n", irp, stack->MinorFunction);

    switch (stack->MinorFunction)
    {
        case IRP_MN_QUERY_DEVICE_RELATIONS:
        {
            struct usb_device *device;
            DEVICE_RELATIONS *devices;
            unsigned int i = 0;

            if (stack->Parameters.QueryDeviceRelations.Type != BusRelations)
            {
                FIXME("Unhandled device relations type %#x.\n", stack->Parameters.QueryDeviceRelations.Type);
                break;
            }

            EnterCriticalSection(&wineusb_cs);

            if (!(devices = ExAllocatePool(PagedPool,
                    offsetof(DEVICE_RELATIONS, Objects[list_count(&device_list)]))))
            {
                LeaveCriticalSection(&wineusb_cs);
                irp->IoStatus.Status = STATUS_NO_MEMORY;
                break;
            }

            LIST_FOR_EACH_ENTRY(device, &device_list, struct usb_device, entry)
            {
                devices->Objects[i++] = device->device_obj;
                call_fastcall_func1(ObfReferenceObject, device->device_obj);
            }

            LeaveCriticalSection(&wineusb_cs);

            devices->Count = i;
            irp->IoStatus.Information = (ULONG_PTR)devices;
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        }

        case IRP_MN_START_DEVICE:
            if ((ret = libusb_hotplug_register_callback(NULL,
                    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                    LIBUSB_HOTPLUG_ENUMERATE, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
                    LIBUSB_HOTPLUG_MATCH_ANY, hotplug_cb, NULL, &hotplug_cb_handle)))
            {
                ERR("Failed to register callback: %s\n", libusb_strerror(ret));
                irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
                break;
            }
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IRP_MN_SURPRISE_REMOVAL:
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IRP_MN_REMOVE_DEVICE:
        {
            struct usb_device *device, *cursor;

            libusb_hotplug_deregister_callback(NULL, hotplug_cb_handle);
            thread_shutdown = TRUE;
            libusb_interrupt_event_handler(NULL);
            WaitForSingleObject(libusb_event_thread, INFINITE);
            CloseHandle(libusb_event_thread);
            WaitForSingleObject(event_thread, INFINITE);
            CloseHandle(event_thread);

            EnterCriticalSection(&wineusb_cs);
            /* Normally we unlink all devices either:
             *
             * - as a result of hot-unplug, which unlinks the device, and causes
             *   a subsequent IRP_MN_REMOVE_DEVICE which will free it;
             *
             * - if the parent is deleted (at shutdown time), in which case
             *   ntoskrnl will send us IRP_MN_SURPRISE_REMOVAL and
             *   IRP_MN_REMOVE_DEVICE unprompted.
             *
             * But we can get devices hotplugged between when shutdown starts
             * and now, in which case they'll be stuck in this list and never
             * freed.
             *
             * FIXME: This is still broken, though. If a device is hotplugged
             * and then removed, it'll be unlinked and never freed. */
            LIST_FOR_EACH_ENTRY_SAFE(device, cursor, &device_list, struct usb_device, entry)
            {
                assert(!device->removed);
                if (!device->parent)
                    destroy_unix_device(device->unix_device);
                list_remove(&device->entry);
                IoDeleteDevice(device->device_obj);
            }
            LeaveCriticalSection(&wineusb_cs);

            irp->IoStatus.Status = STATUS_SUCCESS;
            IoSkipCurrentIrpStackLocation(irp);
            ret = IoCallDriver(bus_pdo, irp);
            IoDetachDevice(bus_pdo);
            IoDeleteDevice(bus_fdo);
            return ret;
        }

        default:
            FIXME("Unhandled minor function %#x.\n", stack->MinorFunction);
    }

    IoSkipCurrentIrpStackLocation(irp);
    return IoCallDriver(bus_pdo, irp);
}

struct string_buffer
{
    WCHAR *string;
    size_t len;
};

static void WINAPIV append_id(struct string_buffer *buffer, const WCHAR *format, ...)
{
    __ms_va_list args;
    WCHAR *string;
    int len;

    __ms_va_start(args, format);

    len = _vsnwprintf(NULL, 0, format, args) + 1;
    if (!(string = ExAllocatePool(PagedPool, (buffer->len + len) * sizeof(WCHAR))))
    {
        if (buffer->string)
            ExFreePool(buffer->string);
        buffer->string = NULL;
        return;
    }
    if (buffer->string)
    {
        memcpy(string, buffer->string, buffer->len * sizeof(WCHAR));
        ExFreePool(buffer->string);
    }
    _vsnwprintf(string + buffer->len, len, format, args);
    buffer->string = string;
    buffer->len += len;

    __ms_va_end(args);
}

static const WCHAR emptyW[] = {0};

static void get_device_id(const struct usb_device *device, struct string_buffer *buffer)
{
    static const WCHAR interface_formatW[] = {'U','S','B','\\','V','I','D','_','%','0','4','X',
            '&','P','I','D','_','%','0','4','X','&','M','I','_','%','0','2','X',0};
    static const WCHAR formatW[] = {'U','S','B','\\','V','I','D','_','%','0','4','X',
            '&','P','I','D','_','%','0','4','X',0};

    if (device->parent)
        append_id(buffer, interface_formatW, device->vendor, device->product, device->interface_index);
    else
        append_id(buffer, formatW, device->vendor, device->product);
}

static void get_hardware_ids(const struct usb_device *device, struct string_buffer *buffer)
{
    static const WCHAR interface_formatW[] = {'U','S','B','\\','V','I','D','_','%','0','4','X',
                '&','P','I','D','_','%','0','4','X','&','R','E','V','_','%','0','4','X','&','M','I','_','%','0','2','X',0};
    static const WCHAR formatW[] = {'U','S','B','\\','V','I','D','_','%','0','4','X',
                '&','P','I','D','_','%','0','4','X','&','R','E','V','_','%','0','4','X',0};

    if (device->parent)
        append_id(buffer, interface_formatW, device->vendor, device->product, device->revision, device->interface_index);
    else
        append_id(buffer, formatW, device->vendor, device->product, device->revision);
    get_device_id(device, buffer);
    append_id(buffer, emptyW);
}

static void get_compatible_ids(const struct usb_device *device, struct string_buffer *buffer)
{
    static const WCHAR prot_format[] = {'U','S','B','\\','C','l','a','s','s','_','%','0','2','x',
            '&','S','u','b','C','l','a','s','s','_','%','0','2','x',
            '&','P','r','o','t','_','%','0','2','x',0};
    static const WCHAR subclass_format[] = {'U','S','B','\\','C','l','a','s','s','_','%','0','2','x',
            '&','S','u','b','C','l','a','s','s','_','%','0','2','x',0};
    static const WCHAR class_format[] = {'U','S','B','\\','C','l','a','s','s','_','%','0','2','x',0};

    append_id(buffer, prot_format, device->class, device->subclass, device->protocol);
    append_id(buffer, subclass_format, device->class, device->subclass);
    append_id(buffer, class_format, device->class);
    append_id(buffer, emptyW);
}

static NTSTATUS query_id(struct usb_device *device, IRP *irp, BUS_QUERY_ID_TYPE type)
{
    static const WCHAR instance_idW[] = {'0',0};
    struct string_buffer buffer = {0};

    TRACE("type %#x.\n", type);

    switch (type)
    {
        case BusQueryDeviceID:
            get_device_id(device, &buffer);
            break;

        case BusQueryInstanceID:
            append_id(&buffer, instance_idW);
            break;

        case BusQueryHardwareIDs:
            get_hardware_ids(device, &buffer);
            break;

        case BusQueryCompatibleIDs:
            get_compatible_ids(device, &buffer);
            break;

        default:
            FIXME("Unhandled ID query type %#x.\n", type);
            return irp->IoStatus.Status;
    }

    if (!buffer.string)
        return STATUS_NO_MEMORY;

    irp->IoStatus.Information = (ULONG_PTR)buffer.string;
    return STATUS_SUCCESS;
}

static void remove_pending_irps(struct usb_device *device)
{
    LIST_ENTRY *entry;
    IRP *irp;

    while ((entry = RemoveHeadList(&device->irp_list)) != &device->irp_list)
    {
        irp = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);
        irp->IoStatus.Status = STATUS_DELETE_PENDING;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
}

static NTSTATUS pdo_pnp(DEVICE_OBJECT *device_obj, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct usb_device *device = device_obj->DeviceExtension;
    NTSTATUS ret = irp->IoStatus.Status;

    TRACE("device_obj %p, irp %p, minor function %#x.\n", device_obj, irp, stack->MinorFunction);

    switch (stack->MinorFunction)
    {
        case IRP_MN_QUERY_ID:
            ret = query_id(device, irp, stack->Parameters.QueryId.IdType);
            break;

        case IRP_MN_QUERY_CAPABILITIES:
        {
            DEVICE_CAPABILITIES *caps = stack->Parameters.DeviceCapabilities.Capabilities;

            caps->RawDeviceOK = 1;

            ret = STATUS_SUCCESS;
            break;
        }

        case IRP_MN_START_DEVICE:
            ret = STATUS_SUCCESS;
            break;

        case IRP_MN_SURPRISE_REMOVAL:
            EnterCriticalSection(&wineusb_cs);
            remove_pending_irps(device);
            if (!device->removed)
            {
                device->removed = TRUE;
                list_remove(&device->entry);
            }
            LeaveCriticalSection(&wineusb_cs);
            ret = STATUS_SUCCESS;
            break;

        case IRP_MN_REMOVE_DEVICE:
            assert(device->removed);
            remove_pending_irps(device);

            if (!device->parent)
                destroy_unix_device(device->unix_device);

            IoDeleteDevice(device->device_obj);
            ret = STATUS_SUCCESS;
            break;

        default:
            FIXME("Unhandled minor function %#x.\n", stack->MinorFunction);
    }

    irp->IoStatus.Status = ret;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return ret;
}

static NTSTATUS WINAPI driver_pnp(DEVICE_OBJECT *device, IRP *irp)
{
    if (device == bus_fdo)
        return fdo_pnp(irp);
    return pdo_pnp(device, irp);
}

static NTSTATUS usbd_status_from_libusb(enum libusb_transfer_status status)
{
    switch (status)
    {
        case LIBUSB_TRANSFER_CANCELLED:
            return USBD_STATUS_CANCELED;
        case LIBUSB_TRANSFER_COMPLETED:
            return USBD_STATUS_SUCCESS;
        case LIBUSB_TRANSFER_NO_DEVICE:
            return USBD_STATUS_DEVICE_GONE;
        case LIBUSB_TRANSFER_STALL:
            return USBD_STATUS_ENDPOINT_HALTED;
        case LIBUSB_TRANSFER_TIMED_OUT:
            return USBD_STATUS_TIMEOUT;
        default:
            FIXME("Unhandled status %#x.\n", status);
        case LIBUSB_TRANSFER_ERROR:
            return USBD_STATUS_REQUEST_FAILED;
    }
}

static void LIBUSB_CALL transfer_cb(struct libusb_transfer *transfer)
{
    IRP *irp = transfer->user_data;
    URB *urb = IoGetCurrentIrpStackLocation(irp)->Parameters.Others.Argument1;
    struct usb_event event;

    TRACE("Completing IRP %p, status %#x.\n", irp, transfer->status);

    urb->UrbHeader.Status = usbd_status_from_libusb(transfer->status);

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED)
    {
        switch (urb->UrbHeader.Function)
        {
            case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
                urb->UrbBulkOrInterruptTransfer.TransferBufferLength = transfer->actual_length;
                break;

            case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
            {
                struct _URB_CONTROL_DESCRIPTOR_REQUEST *req = &urb->UrbControlDescriptorRequest;
                req->TransferBufferLength = transfer->actual_length;
                memcpy(req->TransferBuffer, libusb_control_transfer_get_data(transfer), transfer->actual_length);
                break;
            }

            case URB_FUNCTION_VENDOR_INTERFACE:
            {
                struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST *req = &urb->UrbControlVendorClassRequest;
                req->TransferBufferLength = transfer->actual_length;
                if (req->TransferFlags & USBD_TRANSFER_DIRECTION_IN)
                    memcpy(req->TransferBuffer, libusb_control_transfer_get_data(transfer), transfer->actual_length);
                break;
            }

            default:
                ERR("Unexpected function %#x.\n", urb->UrbHeader.Function);
        }
    }

    event.type = USB_EVENT_TRANSFER_COMPLETE;
    event.u.completed_irp = irp;
    queue_event(&event);
}

struct pipe
{
    unsigned char endpoint;
    unsigned char type;
};

static HANDLE make_pipe_handle(unsigned char endpoint, USBD_PIPE_TYPE type)
{
    union
    {
        struct pipe pipe;
        HANDLE handle;
    } u;

    u.pipe.endpoint = endpoint;
    u.pipe.type = type;
    return u.handle;
}

static struct pipe get_pipe(HANDLE handle)
{
    union
    {
        struct pipe pipe;
        HANDLE handle;
    } u;

    u.handle = handle;
    return u.pipe;
}

static NTSTATUS unix_submit_urb(struct unix_device *device, IRP *irp)
{
    URB *urb = IoGetCurrentIrpStackLocation(irp)->Parameters.Others.Argument1;
    libusb_device_handle *handle = device->handle;
    struct libusb_transfer *transfer;
    int ret;

    TRACE("type %#x.\n", urb->UrbHeader.Function);

    switch (urb->UrbHeader.Function)
    {
        case URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR_STALL:
        {
            struct _URB_PIPE_REQUEST *req = &urb->UrbPipeRequest;
            struct pipe pipe = get_pipe(req->PipeHandle);

            if ((ret = libusb_clear_halt(handle, pipe.endpoint)) < 0)
                ERR("Failed to clear halt: %s\n", libusb_strerror(ret));

            return STATUS_SUCCESS;
        }

        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
        {
            struct _URB_BULK_OR_INTERRUPT_TRANSFER *req = &urb->UrbBulkOrInterruptTransfer;
            struct pipe pipe = get_pipe(req->PipeHandle);

            if (req->TransferBufferMDL)
                FIXME("Unhandled MDL output buffer.\n");

            if (!(transfer = libusb_alloc_transfer(0)))
                return STATUS_NO_MEMORY;
            irp->Tail.Overlay.DriverContext[0] = transfer;

            if (pipe.type == UsbdPipeTypeBulk)
            {
                libusb_fill_bulk_transfer(transfer, handle, pipe.endpoint,
                        req->TransferBuffer, req->TransferBufferLength, transfer_cb, irp, 0);
            }
            else if (pipe.type == UsbdPipeTypeInterrupt)
            {
                libusb_fill_interrupt_transfer(transfer, handle, pipe.endpoint,
                        req->TransferBuffer, req->TransferBufferLength, transfer_cb, irp, 0);
            }
            else
            {
                WARN("Invalid pipe type %#x.\n", pipe.type);
                libusb_free_transfer(transfer);
                return USBD_STATUS_INVALID_PIPE_HANDLE;
            }

            transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
            ret = libusb_submit_transfer(transfer);
            if (ret < 0)
                ERR("Failed to submit bulk transfer: %s\n", libusb_strerror(ret));

            return STATUS_PENDING;
        }

        case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
        {
            struct _URB_CONTROL_DESCRIPTOR_REQUEST *req = &urb->UrbControlDescriptorRequest;
            unsigned char *buffer;

            if (req->TransferBufferMDL)
                FIXME("Unhandled MDL output buffer.\n");

            if (!(transfer = libusb_alloc_transfer(0)))
                return STATUS_NO_MEMORY;
            irp->Tail.Overlay.DriverContext[0] = transfer;

            if (!(buffer = malloc(sizeof(struct libusb_control_setup) + req->TransferBufferLength)))
            {
                libusb_free_transfer(transfer);
                return STATUS_NO_MEMORY;
            }

            libusb_fill_control_setup(buffer,
                    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE,
                    LIBUSB_REQUEST_GET_DESCRIPTOR, (req->DescriptorType << 8) | req->Index,
                    req->LanguageId, req->TransferBufferLength);
            libusb_fill_control_transfer(transfer, handle, buffer, transfer_cb, irp, 0);
            transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;
            ret = libusb_submit_transfer(transfer);
            if (ret < 0)
                ERR("Failed to submit GET_DESCRIPTOR transfer: %s\n", libusb_strerror(ret));

            return STATUS_PENDING;
        }

        case URB_FUNCTION_SELECT_CONFIGURATION:
        {
            struct _URB_SELECT_CONFIGURATION *req = &urb->UrbSelectConfiguration;
            ULONG i;

            /* FIXME: In theory, we'd call libusb_set_configuration() here, but
             * the CASIO FX-9750GII (which has only one configuration) goes into
             * an error state if it receives a SET_CONFIGURATION request. Maybe
             * we should skip setting that if and only if the configuration is
             * already active? */

            for (i = 0; i < req->Interface.NumberOfPipes; ++i)
            {
                USBD_PIPE_INFORMATION *pipe = &req->Interface.Pipes[i];
                pipe->PipeHandle = make_pipe_handle(pipe->EndpointAddress, pipe->PipeType);
            }

            return STATUS_SUCCESS;
        }

        case URB_FUNCTION_VENDOR_INTERFACE:
        {
            struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST *req = &urb->UrbControlVendorClassRequest;
            uint8_t req_type = LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE;
            unsigned char *buffer;

            if (req->TransferFlags & USBD_TRANSFER_DIRECTION_IN)
                req_type |= LIBUSB_ENDPOINT_IN;
            if (req->TransferFlags & ~USBD_TRANSFER_DIRECTION_IN)
                FIXME("Unhandled flags %#x.\n", req->TransferFlags);

            if (req->TransferBufferMDL)
                FIXME("Unhandled MDL output buffer.\n");

            if (!(transfer = libusb_alloc_transfer(0)))
                return STATUS_NO_MEMORY;
            irp->Tail.Overlay.DriverContext[0] = transfer;

            if (!(buffer = malloc(sizeof(struct libusb_control_setup) + req->TransferBufferLength)))
            {
                libusb_free_transfer(transfer);
                return STATUS_NO_MEMORY;
            }

            libusb_fill_control_setup(buffer, req_type, req->Request,
                    req->Value, req->Index, req->TransferBufferLength);
            if (!(req->TransferFlags & USBD_TRANSFER_DIRECTION_IN))
                memcpy(buffer + LIBUSB_CONTROL_SETUP_SIZE, req->TransferBuffer, req->TransferBufferLength);
            libusb_fill_control_transfer(transfer, handle, buffer, transfer_cb, irp, 0);
            transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;
            ret = libusb_submit_transfer(transfer);
            if (ret < 0)
                ERR("Failed to submit vendor-specific interface transfer: %s\n", libusb_strerror(ret));

            return STATUS_PENDING;
        }

        default:
            FIXME("Unhandled function %#x.\n", urb->UrbHeader.Function);
    }

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS usb_submit_urb(struct usb_device *device, IRP *irp)
{
    URB *urb = IoGetCurrentIrpStackLocation(irp)->Parameters.Others.Argument1;
    NTSTATUS status;

    TRACE("type %#x.\n", urb->UrbHeader.Function);

    switch (urb->UrbHeader.Function)
    {
        case URB_FUNCTION_ABORT_PIPE:
        {
            LIST_ENTRY *entry, *mark;

            /* The documentation states that URB_FUNCTION_ABORT_PIPE may
             * complete before outstanding requests complete, so we don't need
             * to wait for them. */
            EnterCriticalSection(&wineusb_cs);
            mark = &device->irp_list;
            for (entry = mark->Flink; entry != mark; entry = entry->Flink)
            {
                IRP *queued_irp = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);
                struct usb_cancel_transfer_params params =
                {
                    .transfer = queued_irp->Tail.Overlay.DriverContext[0],
                };

                __wine_unix_call(unix_handle, unix_usb_cancel_transfer, &params);
            }
            LeaveCriticalSection(&wineusb_cs);

            return STATUS_SUCCESS;
        }

        case URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR_STALL:
        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
        case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
        case URB_FUNCTION_SELECT_CONFIGURATION:
        case URB_FUNCTION_VENDOR_INTERFACE:
        {
            /* Hold the wineusb lock while submitting and queuing, and
             * similarly hold it in complete_irp(). That way, if libusb reports
             * completion between submitting and queuing, we won't try to
             * dequeue the IRP until it's actually been queued. */
            EnterCriticalSection(&wineusb_cs);
            status = unix_submit_urb(device->unix_device, irp);
            if (status == STATUS_PENDING)
            {
                IoMarkIrpPending(irp);
                InsertTailList(&device->irp_list, &irp->Tail.Overlay.ListEntry);
            }
            LeaveCriticalSection(&wineusb_cs);

            return status;
        }

        default:
            FIXME("Unhandled function %#x.\n", urb->UrbHeader.Function);
    }

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS WINAPI driver_internal_ioctl(DEVICE_OBJECT *device_obj, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    struct usb_device *device = device_obj->DeviceExtension;
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;
    BOOL removed;

    TRACE("device_obj %p, irp %p, code %#x.\n", device_obj, irp, code);

    EnterCriticalSection(&wineusb_cs);
    removed = device->removed;
    LeaveCriticalSection(&wineusb_cs);

    if (removed)
    {
        irp->IoStatus.Status = STATUS_DELETE_PENDING;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_DELETE_PENDING;
    }

    switch (code)
    {
        case IOCTL_INTERNAL_USB_SUBMIT_URB:
            status = usb_submit_urb(device, irp);
            break;

        default:
            FIXME("Unhandled ioctl %#x (device %#x, access %#x, function %#x, method %#x).\n",
                    code, code >> 16, (code >> 14) & 3, (code >> 2) & 0xfff, code & 3);
    }

    if (status != STATUS_PENDING)
    {
        irp->IoStatus.Status = status;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
    return status;
}

static NTSTATUS WINAPI driver_add_device(DRIVER_OBJECT *driver, DEVICE_OBJECT *pdo)
{
    NTSTATUS ret;

    TRACE("driver %p, pdo %p.\n", driver, pdo);

    if ((ret = IoCreateDevice(driver, 0, NULL, FILE_DEVICE_BUS_EXTENDER, 0, FALSE, &bus_fdo)))
    {
        ERR("Failed to create FDO, status %#x.\n", ret);
        return ret;
    }

    IoAttachDeviceToDeviceStack(bus_fdo, pdo);
    bus_pdo = pdo;
    bus_fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

static void WINAPI driver_unload(DRIVER_OBJECT *driver)
{
    libusb_exit(NULL);
}

NTSTATUS WINAPI DriverEntry(DRIVER_OBJECT *driver, UNICODE_STRING *path)
{
    NTSTATUS status;
    void *instance;
    int err;

    TRACE("driver %p, path %s.\n", driver, debugstr_w(path->Buffer));

    RtlPcToFileHeader(DriverEntry, &instance);
    if ((status = NtQueryVirtualMemory(GetCurrentProcess(), instance,
            MemoryWineUnixFuncs, &unix_handle, sizeof(unix_handle), NULL)))
    {
        ERR("Failed to initialize Unix library, status %#x.\n", status);
        return status;
    }

    driver_obj = driver;

    if ((err = libusb_init(NULL)))
    {
        ERR("Failed to initialize libusb: %s\n", libusb_strerror(err));
        return STATUS_UNSUCCESSFUL;
    }

    libusb_event_thread = CreateThread(NULL, 0, libusb_event_thread_proc, NULL, 0, NULL);
    event_thread = CreateThread(NULL, 0, event_thread_proc, NULL, 0, NULL);

    driver->DriverExtension->AddDevice = driver_add_device;
    driver->DriverUnload = driver_unload;
    driver->MajorFunction[IRP_MJ_PNP] = driver_pnp;
    driver->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = driver_internal_ioctl;

    return STATUS_SUCCESS;
}
