/*
 * Copyright (C) 2007 Google (Evan Stade)
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wingdi.h"

#define COBJMACROS
#include "objbase.h"
#include "olectl.h"
#include "ole2.h"

#include "gdiplus.h"
#include "gdiplus_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdiplus);

typedef void ImageItemData;

#define PIXELFORMATBPP(x) ((x) ? ((x) >> 8) & 255 : 24)

static INT ipicture_pixel_height(IPicture *pic)
{
    HDC hdcref;
    OLE_YSIZE_HIMETRIC y;

    IPicture_get_Height(pic, &y);

    hdcref = GetDC(0);

    y = (UINT)(((REAL)y) * ((REAL)GetDeviceCaps(hdcref, LOGPIXELSY)) /
              ((REAL)INCH_HIMETRIC));
    ReleaseDC(0, hdcref);

    return y;
}

static INT ipicture_pixel_width(IPicture *pic)
{
    HDC hdcref;
    OLE_XSIZE_HIMETRIC x;

    IPicture_get_Width(pic, &x);

    hdcref = GetDC(0);

    x = (UINT)(((REAL)x) * ((REAL)GetDeviceCaps(hdcref, LOGPIXELSX)) /
              ((REAL)INCH_HIMETRIC));

    ReleaseDC(0, hdcref);

    return x;
}

GpStatus WINGDIPAPI GdipBitmapGetPixel(GpBitmap* bitmap, INT x, INT y,
    ARGB *color)
{
    static int calls;
    TRACE("%p %d %d %p\n", bitmap, x, y, color);

    if(!bitmap || !color)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    *color = 0xdeadbeef;

    return NotImplemented;
}

/* This function returns a pointer to an array of pixels that represents the
 * bitmap. The *entire* bitmap is locked according to the lock mode specified by
 * flags.  It is correct behavior that a user who calls this function with write
 * privileges can write to the whole bitmap (not just the area in rect).
 *
 * FIXME: only used portion of format is bits per pixel. */
GpStatus WINGDIPAPI GdipBitmapLockBits(GpBitmap* bitmap, GDIPCONST GpRect* rect,
    UINT flags, PixelFormat format, BitmapData* lockeddata)
{
    BOOL bm_is_selected;
    INT stride, bitspp = PIXELFORMATBPP(format);
    OLE_HANDLE hbm;
    HDC hdc;
    HBITMAP old = NULL;
    BITMAPINFO bmi;
    BYTE *buff = NULL;
    UINT abs_height;

    TRACE("%p %p %d %d %p\n", bitmap, rect, flags, format, lockeddata);

    if(!lockeddata || !bitmap || !rect)
        return InvalidParameter;

    if(rect->X < 0 || rect->Y < 0 || (rect->X + rect->Width > bitmap->width) ||
       (rect->Y + rect->Height > bitmap->height) || !flags)
        return InvalidParameter;

    if(flags & ImageLockModeUserInputBuf)
        return NotImplemented;

    if((bitmap->lockmode & ImageLockModeWrite) || (bitmap->lockmode &&
        (flags & ImageLockModeWrite)))
        return WrongState;

    IPicture_get_Handle(bitmap->image.picture, &hbm);
    IPicture_get_CurDC(bitmap->image.picture, &hdc);
    bm_is_selected = (hdc != 0);

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biBitCount = 0;

    if(!bm_is_selected){
        hdc = GetDC(0);
        old = SelectObject(hdc, (HBITMAP)hbm);
    }

    /* fill out bmi */
    GetDIBits(hdc, (HBITMAP)hbm, 0, 0, NULL, &bmi, DIB_RGB_COLORS);

    abs_height = abs(bmi.bmiHeader.biHeight);
    stride = bmi.bmiHeader.biWidth * bitspp / 8;
    stride = (stride + 3) & ~3;

    buff = GdipAlloc(stride * abs_height);
    if(!buff)   return OutOfMemory;

    bmi.bmiHeader.biBitCount = bitspp;
    GetDIBits(hdc, (HBITMAP)hbm, 0, abs_height, buff, &bmi, DIB_RGB_COLORS);

    if(!bm_is_selected){
        SelectObject(hdc, old);
        ReleaseDC(0, hdc);
    }

    lockeddata->Width = rect->Width;
    lockeddata->Height = rect->Height;
    lockeddata->PixelFormat = format;
    lockeddata->Reserved = flags;

    if(bmi.bmiHeader.biHeight > 0){
        lockeddata->Stride = -stride;
        lockeddata->Scan0 = buff + (bitspp / 8) * rect->X +
                            stride * (abs_height - 1 - rect->Y);
    }
    else{
        lockeddata->Stride = stride;
        lockeddata->Scan0 = buff + (bitspp / 8) * rect->X + stride * rect->Y;
    }

    bitmap->lockmode = flags;
    bitmap->numlocks++;

    if(flags & ImageLockModeWrite)
        bitmap->bitmapbits = buff;

    return Ok;
}

GpStatus WINGDIPAPI GdipBitmapUnlockBits(GpBitmap* bitmap,
    BitmapData* lockeddata)
{
    OLE_HANDLE hbm;
    HDC hdc;
    HBITMAP old = NULL;
    BOOL bm_is_selected;
    BITMAPINFO bmi;

    if(!bitmap || !lockeddata)
        return InvalidParameter;

    if(!bitmap->lockmode)
        return WrongState;

    if(lockeddata->Reserved & ImageLockModeUserInputBuf)
        return NotImplemented;

    if(lockeddata->Reserved & ImageLockModeRead){
        if(!(--bitmap->numlocks))
            bitmap->lockmode = 0;

        GdipFree(lockeddata->Scan0);
        return Ok;
    }

    IPicture_get_Handle(bitmap->image.picture, &hbm);
    IPicture_get_CurDC(bitmap->image.picture, &hdc);
    bm_is_selected = (hdc != 0);

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biBitCount = 0;

    if(!bm_is_selected){
        hdc = GetDC(0);
        old = SelectObject(hdc, (HBITMAP)hbm);
    }

    GetDIBits(hdc, (HBITMAP)hbm, 0, 0, NULL, &bmi, DIB_RGB_COLORS);
    bmi.bmiHeader.biBitCount = PIXELFORMATBPP(lockeddata->PixelFormat);
    SetDIBits(hdc, (HBITMAP)hbm, 0, abs(bmi.bmiHeader.biHeight),
              bitmap->bitmapbits, &bmi, DIB_RGB_COLORS);

    if(!bm_is_selected){
        SelectObject(hdc, old);
        ReleaseDC(0, hdc);
    }

    GdipFree(bitmap->bitmapbits);

    return Ok;
}

GpStatus WINGDIPAPI GdipCreateBitmapFromScan0(INT width, INT height, INT stride,
    PixelFormat format, BYTE* scan0, GpBitmap** bitmap)
{
    BITMAPFILEHEADER *bmfh;
    BITMAPINFOHEADER *bmih;
    BYTE *buff;
    INT datalen = stride * height, size;
    IStream *stream;

    TRACE("%d %d %d %d %p %p\n", width, height, stride, format, scan0, bitmap);

    if(!scan0 || !bitmap)
        return InvalidParameter;

    *bitmap = GdipAlloc(sizeof(GpBitmap));
    if(!*bitmap)    return OutOfMemory;

    size = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + datalen;
    buff = GdipAlloc(size);
    if(!buff){
        GdipFree(*bitmap);
        return OutOfMemory;
    }

    bmfh = (BITMAPFILEHEADER*) buff;
    bmih = (BITMAPINFOHEADER*) (bmfh + 1);

    bmfh->bfType    = (((WORD)'M') << 8) + (WORD)'B';
    bmfh->bfSize    = size;
    bmfh->bfOffBits = size - datalen;

    bmih->biSize            = sizeof(BITMAPINFOHEADER);
    bmih->biWidth           = width;
    bmih->biHeight          = height;
    /* FIXME: use the rest of the data from format */
    bmih->biBitCount        = PIXELFORMATBPP(format);
    bmih->biCompression     = BI_RGB;

    memcpy(bmih + 1, scan0, datalen);

    if(CreateStreamOnHGlobal(buff, TRUE, &stream) != S_OK){
        ERR("could not make stream\n");
        GdipFree(*bitmap);
        GdipFree(buff);
        return GenericError;
    }

    if(OleLoadPicture(stream, 0, FALSE, &IID_IPicture,
        (LPVOID*) &((*bitmap)->image.picture)) != S_OK){
        TRACE("Could not load picture\n");
        IStream_Release(stream);
        GdipFree(*bitmap);
        GdipFree(buff);
        return GenericError;
    }

    (*bitmap)->image.type = ImageTypeBitmap;
    (*bitmap)->width = width;
    (*bitmap)->height = height;
    (*bitmap)->format = format;

    return Ok;
}

GpStatus WINGDIPAPI GdipCreateBitmapFromStream(IStream* stream,
    GpBitmap **bitmap)
{
    BITMAPINFO bmi;
    BITMAPCOREHEADER* bmch;
    OLE_HANDLE hbm;
    HDC hdc;
    GpStatus stat;

    stat = GdipLoadImageFromStream(stream, (GpImage**) bitmap);

    if(stat != Ok)
        return stat;

    /* FIXME: make sure it's actually a bitmap */
    (*bitmap)->image.type = ImageTypeBitmap;
    (*bitmap)->width = ipicture_pixel_width((*bitmap)->image.picture);
    (*bitmap)->height = ipicture_pixel_height((*bitmap)->image.picture);

    /* get the pixel format */
    IPicture_get_Handle((*bitmap)->image.picture, &hbm);
    IPicture_get_CurDC((*bitmap)->image.picture, &hdc);

    bmch = (BITMAPCOREHEADER*) (&bmi.bmiHeader);
    bmch->bcSize = sizeof(BITMAPCOREHEADER);

    if(!hdc){
        HBITMAP old;
        hdc = GetDC(0);
        old = SelectObject(hdc, (HBITMAP)hbm);
        GetDIBits(hdc, (HBITMAP)hbm, 0, 0, NULL, &bmi, DIB_RGB_COLORS);
        SelectObject(hdc, old);
        ReleaseDC(0, hdc);
    }
    else
        GetDIBits(hdc, (HBITMAP)hbm, 0, 0, NULL, &bmi, DIB_RGB_COLORS);

    (*bitmap)->format = (bmch->bcBitCount << 8) | PixelFormatGDI;

    return Ok;
}

/* FIXME: no icm */
GpStatus WINGDIPAPI GdipCreateBitmapFromStreamICM(IStream* stream,
    GpBitmap **bitmap)
{
    return GdipCreateBitmapFromStream(stream, bitmap);
}

GpStatus WINGDIPAPI GdipDisposeImage(GpImage *image)
{
    if(!image)
        return InvalidParameter;

    IPicture_Release(image->picture);
    GdipFree(image);

    return Ok;
}

GpStatus WINGDIPAPI GdipFindFirstImageItem(GpImage *image, ImageItemData* item)
{
    if(!image || !item)
        return InvalidParameter;

    return NotImplemented;
}

GpStatus WINGDIPAPI GdipGetImageBounds(GpImage *image, GpRectF *srcRect,
    GpUnit *srcUnit)
{
    if(!image || !srcRect || !srcUnit)
        return InvalidParameter;
    if(image->type == ImageTypeMetafile){
        memcpy(srcRect, &((GpMetafile*)image)->bounds, sizeof(GpRectF));
        *srcUnit = ((GpMetafile*)image)->unit;
    }
    else if(image->type == ImageTypeBitmap){
        srcRect->X = srcRect->Y = 0.0;
        srcRect->Width = (REAL) ((GpBitmap*)image)->width;
        srcRect->Height = (REAL) ((GpBitmap*)image)->height;
        *srcUnit = UnitPixel;
    }
    else{
        srcRect->X = srcRect->Y = 0.0;
        srcRect->Width = ipicture_pixel_width(image->picture);
        srcRect->Height = ipicture_pixel_height(image->picture);
        *srcUnit = UnitPixel;
    }

    TRACE("returning (%f, %f) (%f, %f) unit type %d\n", srcRect->X, srcRect->Y,
          srcRect->Width, srcRect->Height, *srcUnit);

    return Ok;
}

GpStatus WINGDIPAPI GdipGetImageHeight(GpImage *image, UINT *height)
{
    if(!image || !height)
        return InvalidParameter;

    if(image->type == ImageTypeMetafile){
        FIXME("not implemented for metafiles\n");
        return NotImplemented;
    }
    else if(image->type == ImageTypeBitmap)
        *height = ((GpBitmap*)image)->height;
    else
        *height = ipicture_pixel_height(image->picture);

    TRACE("returning %d\n", *height);

    return Ok;
}

GpStatus WINGDIPAPI GdipGetImageHorizontalResolution(GpImage *image, REAL *res)
{
    static int calls;

    if(!image || !res)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    return NotImplemented;
}

/* FIXME: test this function for non-bitmap types */
GpStatus WINGDIPAPI GdipGetImagePixelFormat(GpImage *image, PixelFormat *format)
{
    if(!image || !format)
        return InvalidParameter;

    if(image->type != ImageTypeBitmap)
        *format = PixelFormat24bppRGB;
    else
        *format = ((GpBitmap*) image)->format;

    return Ok;
}

GpStatus WINGDIPAPI GdipGetImageRawFormat(GpImage *image, GUID *format)
{
    static int calls;

    if(!image || !format)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    return NotImplemented;
}

GpStatus WINGDIPAPI GdipGetImageType(GpImage *image, ImageType *type)
{
    if(!image || !type)
        return InvalidParameter;

    *type = image->type;

    return Ok;
}

GpStatus WINGDIPAPI GdipGetImageVerticalResolution(GpImage *image, REAL *res)
{
    static int calls;

    if(!image || !res)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    return NotImplemented;
}

GpStatus WINGDIPAPI GdipGetImageWidth(GpImage *image, UINT *width)
{
    if(!image || !width)
        return InvalidParameter;

    if(image->type == ImageTypeMetafile){
        FIXME("not implemented for metafiles\n");
        return NotImplemented;
    }
    else if(image->type == ImageTypeBitmap)
        *width = ((GpBitmap*)image)->width;
    else
        *width = ipicture_pixel_width(image->picture);

    TRACE("returning %d\n", *width);

    return Ok;
}

GpStatus WINGDIPAPI GdipGetMetafileHeaderFromMetafile(GpMetafile * metafile,
    MetafileHeader * header)
{
    static int calls;

    if(!metafile || !header)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    return NotImplemented;
}

GpStatus WINGDIPAPI GdipGetPropertyItemSize(GpImage *image, PROPID pid,
    UINT* size)
{
    static int calls;

    TRACE("%p %x %p\n", image, pid, size);

    if(!size || !image)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    return NotImplemented;
}

GpStatus WINGDIPAPI GdipImageGetFrameCount(GpImage *image,
    GDIPCONST GUID* dimensionID, UINT* count)
{
    static int calls;

    if(!image || !dimensionID || !count)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    return NotImplemented;
}

GpStatus WINGDIPAPI GdipImageGetFrameDimensionsList(GpImage* image,
    GUID* dimensionIDs, UINT count)
{
    static int calls;

    if(!image || !dimensionIDs)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    return Ok;
}

GpStatus WINGDIPAPI GdipImageSelectActiveFrame(GpImage *image,
    GDIPCONST GUID* dimensionID, UINT frameidx)
{
    static int calls;

    if(!image || !dimensionID)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    return Ok;
}

GpStatus WINGDIPAPI GdipLoadImageFromStream(IStream* stream, GpImage **image)
{
    if(!stream || !image)
        return InvalidParameter;

    *image = GdipAlloc(sizeof(GpImage));
    if(!*image) return OutOfMemory;

    if(OleLoadPicture(stream, 0, FALSE, &IID_IPicture,
        (LPVOID*) &((*image)->picture)) != S_OK){
        TRACE("Could not load picture\n");
        GdipFree(*image);
        return GenericError;
    }

    /* FIXME: use IPicture_get_Type to get image type? */
    (*image)->type = ImageTypeUnknown;

    return Ok;
}

/* FIXME: no ICM */
GpStatus WINGDIPAPI GdipLoadImageFromStreamICM(IStream* stream, GpImage **image)
{
    return GdipLoadImageFromStream(stream, image);
}

GpStatus WINGDIPAPI GdipRemovePropertyItem(GpImage *image, PROPID propId)
{
    static int calls;

    if(!image)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    return NotImplemented;
}

GpStatus WINGDIPAPI GdipSaveImageToStream(GpImage *image, IStream* stream,
    GDIPCONST CLSID* clsid, GDIPCONST EncoderParameters* params)
{
    if(!image || !stream)
        return InvalidParameter;

    /* FIXME: CLSID, EncoderParameters not used */

    IPicture_SaveAsFile(image->picture, stream, FALSE, NULL);

    return Ok;
}
