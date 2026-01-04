/*
    Copyright (C) 2022-2025, The AROS Development Team. All rights reserved.
*/

/**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dostags.h>
#include <graphics/gfxbase.h>
#include <graphics/rpattr.h>
#include <cybergraphx/cybergraphics.h>
#include <intuition/imageclass.h>
#include <intuition/icclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/iffparse.h>
#include <proto/datatypes.h>
#include <proto/tiff.h>

#include <tiffinline.h>
#include <tiffio.h>

#include <aros/symbolsets.h>

# include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>


#include "debug.h"

#include "methods.h"

ADD2LIBS("datatypes/picture.datatype", 0, struct Library *, PictureBase);

/**************************************************************************************************/

/* Dummy functions for the linker */
void abort(void)
{
  exit(1);
}

void exit(int bla)
{
  D(bug("[tiff.datatype] %s()\n", __func__));
  abort();
}

/**************************************************************************************************/

static void tiffConvert16to8(UWORD pi, UWORD sspp, ULONG pxfmt, ULONG width, ULONG height, const UBYTE *src, UBYTE *dst)
{
    const UWORD *wsrc = (const UWORD *)src; /* assume properly aligned */
    UBYTE *d = dst;
    ULONG npixels = width * height;
    ULONG i;

    D(bug("[tiff.datatype] %s(%04x, %04x, %08x, %u, %u, 0x%p, 0x%p)
", __func__, pi, sspp, pxfmt, width, height, src, dst));

    /* 1-sample (grayscale / palette) case */
    if (sspp == 1 || sspp == 2) {
        D(bug("[tiff.datatype] %s: greyscale/pallete
", __func__));
        for (i = 0; i < npixels; ++i) {
            ULONG idx = i * (ULONG)sspp;
            UWORD sample = wsrc[idx];             /* 16-bit sample */
            UBYTE v = (UBYTE)(sample >> 8);     /* take high byte */

            if (pi == PHOTOMETRIC_MINISWHITE) {
                D(bug("[tiff.datatype] %s: MINISWHITE
", __func__));
                v = (UBYTE)(255 - v);
            }

            if (pxfmt == PBPAFMT_RGB) {
                d[3 * i + 0] = v;
                d[3 * i + 1] = v;
                d[3 * i + 2] = v;
            } else if (pxfmt == PBPAFMT_RGBA) {
                d[4 * i + 0] = v;
                d[4 * i + 1] = v;
                d[4 * i + 2] = v;
                if (sspp == 2)
                    d[4 * i + 3] = (UBYTE)(wsrc[idx + 1] >> 8);
                else
                    d[4 * i + 3] = 0xFF;
            }
        }
        return;
    }

    /* interleaved RGB (3) or RGBA (4) */
    if (sspp == 3 || sspp == 4) {
        D(bug("[tiff.datatype] %s: RGB%s
", __func__, (sspp == 4) ? "A" : ""));
        ULONG idx;
        for (i = 0; i < npixels; ++i) {
            /* index into 16-bit words: pixel i starts at i * sspp */
            idx = i * (ULONG)sspp;

            UWORD s0 = wsrc[idx + 0];
            UWORD s1 = wsrc[idx + 1];
            UWORD s2 = wsrc[idx + 2];

            UBYTE r = (UBYTE)(s0 >> 8);
            UBYTE g = (UBYTE)(s1 >> 8);
            UBYTE b = (UBYTE)(s2 >> 8);

            if (pi == PHOTOMETRIC_MINISWHITE) {
                D(bug("[tiff.datatype] %s: MINISWHITE
", __func__));
                r = (UBYTE)(255 - r);
                g = (UBYTE)(255 - g);
                b = (UBYTE)(255 - b);
            }

            if (pxfmt == PBPAFMT_RGB) {
                d[3 * i + 0] = r;
                d[3 * i + 1] = g;
                d[3 * i + 2] = b;
            }
            else if (pxfmt == PBPAFMT_RGBA) {
                d[4 * i + 0] = r;
                d[4 * i + 1] = g;
                d[4 * i + 2] = b;
                if (sspp == 4) {
                    UWORD sa = wsrc[idx + 3];
                    d[4 * i + 3] = (UBYTE)(sa >> 8);
                } else
                    d[4 * i + 3] = 0xFF;

            }
        }
        return;
    }

    /* unsupported sspp: do nothing (could log) */
}

static void tiffConvert32to8(UWORD pi, UWORD sspp, ULONG pxfmt, ULONG width, ULONG height, const UBYTE *src, UBYTE *dst)
{
    const ULONG *lsrc = (const ULONG *)src;
    ULONG npixels = width * height;

    D(bug("[tiff.datatype] %s(%04x, %04x, %08x, %u, %u, 0x%p, 0x%p)
", __func__, pi, sspp, pxfmt, width, height, src, dst));

    for (ULONG i = 0; i < npixels; i++) {
        if (pi < PHOTOMETRIC_RGB && (sspp == 1 || sspp == 2)) {
            ULONG idx = i * (ULONG)sspp;
            UBYTE pixval = (UBYTE)((lsrc[idx] >> 24) & 0xFF);

            if (pi == PHOTOMETRIC_MINISWHITE) {
                pixval = 255 - pixval;
            }

            if (pxfmt == PBPAFMT_RGB) {
                dst[3 * i + 0] = pixval;
                dst[3 * i + 1] = pixval;
                dst[3 * i + 2] = pixval;
            }
            else if (pxfmt == PBPAFMT_RGBA) {
                dst[4 * i + 0] = pixval;
                dst[4 * i + 1] = pixval;
                dst[4 * i + 2] = pixval;
                if (sspp == 2)
                    dst[4 * i + 3] = (UBYTE)((lsrc[idx + 1] >> 24) & 0xFF);
                else
                    dst[4 * i + 3] = 0xFF;
            }
        }
        else if (sspp == 3) {
            const ULONG *long_ptr = &lsrc[i * 3];

            if (pxfmt == PBPAFMT_RGB) {
                dst[3 * i + 0] = (long_ptr[0] >> 24) & 0xFF;
                dst[3 * i + 1] = (long_ptr[1] >> 24) & 0xFF;
                dst[3 * i + 2] = (long_ptr[2] >> 24) & 0xFF;
            } else if (pxfmt == PBPAFMT_RGBA) {
                dst[4 * i + 0] = (long_ptr[0] >> 24) & 0xFF;
                dst[4 * i + 1] = (long_ptr[1] >> 24) & 0xFF;
                dst[4 * i + 2] = (long_ptr[2] >> 24) & 0xFF;
                dst[4 * i + 3] = 0xFF;
            }
        }
        else if (sspp == 4) {
            const ULONG *long_ptr = &lsrc[i * 4];

            if (pxfmt == PBPAFMT_RGB) {
                dst[3 * i + 0] = (long_ptr[0] >> 24) & 0xFF;
                dst[3 * i + 1] = (long_ptr[1] >> 24) & 0xFF;
                dst[3 * i + 2] = (long_ptr[2] >> 24) & 0xFF;
            }
            else if (pxfmt == PBPAFMT_RGBA) {
                dst[4 * i + 0] = (long_ptr[0] >> 24) & 0xFF;
                dst[4 * i + 1] = (long_ptr[1] >> 24) & 0xFF;
                dst[4 * i + 2] = (long_ptr[2] >> 24) & 0xFF;
                dst[4 * i + 3] = (long_ptr[3] >> 24) & 0xFF;
            }
        }
    }
}

static ULONG tiffRowBytes(ULONG width, ULONG pxfmt)
{
    if (pxfmt == PBPAFMT_RGBA) {
        return width * 4;
    }
    if (pxfmt == PBPAFMT_RGB) {
        return width * 3;
    }
    if (pxfmt == PBPAFMT_LUT8) {
        return width;
    }

    return width;
}

static BOOL tiffLoadRGBAFallback(struct IClass *cl, Object *o, TIFF *tif, ULONG width, ULONG height)
{
    ULONG pixelCount = width * height;
    ULONG *raster = AllocVec(pixelCount * sizeof(ULONG), MEMF_ANY);
    UBYTE *rgba = NULL;

    D(bug("[tiff.datatype] %s(%u,%u)
", __func__, width, height));

    if (!raster) {
        return FALSE;
    }

    if (!TIFFReadRGBAImageOriented(tif, width, height, raster, ORIENTATION_TOPLEFT, 0)) {
        FreeVec(raster);
        return FALSE;
    }

    rgba = AllocVec(pixelCount * 4, MEMF_ANY);
    if (!rgba) {
        FreeVec(raster);
        return FALSE;
    }

    for (ULONG i = 0; i < pixelCount; ++i) {
        ULONG pixel = raster[i];
        rgba[4 * i + 0] = TIFFGetR(pixel);
        rgba[4 * i + 1] = TIFFGetG(pixel);
        rgba[4 * i + 2] = TIFFGetB(pixel);
        rgba[4 * i + 3] = TIFFGetA(pixel);
    }

    if (!DoSuperMethod(cl, o,
                       PDTM_WRITEPIXELARRAY,
                       (IPTR)rgba,
                       PBPAFMT_RGBA,
                       width * 4,
                       0,
                       0,
                       width,
                       height)) {
        D(bug("[tiff.datatype] %s: DT object failed to render
", __func__));
        FreeVec(rgba);
        FreeVec(raster);
        return FALSE;
    }

    FreeVec(rgba);
    FreeVec(raster);
    return TRUE;
}


static void tiffYCbCr2RGB(UBYTE y, UBYTE cb, UBYTE cr, UBYTE *r, UBYTE *g, UBYTE *b)
{
    LONG c = (LONG)y - 16;
    LONG d = (LONG)cb - 128;
    LONG e = (LONG)cr - 128;

    LONG rr = (298 * c + 409 * e + 128) >> 8;
    LONG gg = (298 * c - 100 * d - 208 * e + 128) >> 8;
    LONG bb = (298 * c + 516 * d + 128) >> 8;

    if (rr < 0)
        rr = 0;
    else if (rr > 255)
        rr = 255;
    if (gg < 0)
        gg = 0;
    else if (gg > 255)
        gg = 255;
    if (bb < 0)
        bb = 0;
    else if (bb > 255)
        bb = 255;

    *r = (UBYTE)rr;
    *g = (UBYTE)gg;
    *b = (UBYTE)bb;
}

static void tiffConvertYCbCr(ULONG pxfmt, ULONG width, ULONG height,
                             UBYTE *src, UBYTE *dst)
{
    ULONG i;

    D(bug("[tiff.datatype] %s()\n", __func__));

    for (i = 0; i < width * height; i++)
    {
        UBYTE y  = src[i * 3 + 0];
        UBYTE cb = src[i * 3 + 1];
        UBYTE cr = src[i * 3 + 2];

        UBYTE r, g, b;
        tiffYCbCr2RGB(y, cb, cr, &r, &g, &b);

        if (pxfmt == PBPAFMT_RGB)
        {
            dst[i * 3 + 0] = r;
            dst[i * 3 + 1] = g;
            dst[i * 3 + 2] = b;
        }
        else if (pxfmt == PBPAFMT_RGBA)
        {
            dst[i * 4 + 0] = r;
            dst[i * 4 + 1] = g;
            dst[i * 4 + 2] = b;
            dst[i * 4 + 3] = 0xFF;
        }
    }
}

/**************************************************************************************************/

#if !defined(MIN)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static BOOL LoadTIFF(struct IClass *cl, Object *o)
{
    union {
        struct IFFHandle   *iff;
        BPTR                bptr;
    } filehandle;
    IPTR                    sourcetype;
    struct BitMapHeader     *bmhd;
    TIFF          *tif;
    char tiffFName[1024];

    D(bug("[tiff.datatype] %s()\n", __func__));

    if( GetDTAttrs(o,   DTA_SourceType    , (IPTR)&sourcetype ,
                        DTA_Handle        , (IPTR)&filehandle,
                        PDTA_BitMapHeader , (IPTR)&bmhd,
                        TAG_DONE) != 3 ) {
        return FALSE;
    }

    if ( sourcetype == DTST_RAM && filehandle.iff == NULL && bmhd ) {
        D(bug("[tiff.datatype] %s: Creating an empty object\n", __func__));
        return TRUE;
    }
    if ( sourcetype != DTST_FILE || !filehandle.bptr || !bmhd ) {
        D(bug("[tiff.datatype] %s: unsupported mode\n", __func__));
        return FALSE;
    }

    NameFromFH(filehandle.bptr, tiffFName, 1023);
    D(bug("[tiff.datatype] %s: opening '%s'\n", __func__, tiffFName));

    tif = TIFFOpen(tiffFName, "r");
    if (tif) {
        ULONG imageLength = 0, tileLength = 0;
        ULONG imageWidth = 0, tileWidth = 0;
        ULONG RowsPerStrip = 0;
        UWORD BitsPerSample = 0, samplesize;
        UWORD SamplesPerPixel = 0;
        UWORD PhotometricInterpretation = 0;
        UWORD compression = 0;
        UWORD planar = PLANARCONFIG_CONTIG;
        UWORD sampleFormat = SAMPLEFORMAT_UINT;
        STRPTR name = NULL;
        BOOL isTiled = FALSE, useYCbCr = TRUE, useFallback = FALSE;

        D(bug("[tiff.datatype] %s: tif @  0x%p\n", __func__, tif));

        if (TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression)) {
            if (compression == COMPRESSION_JPEG) {
                TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
        if (!(BitsPerSample == 1 || BitsPerSample == 2 || BitsPerSample == 4 || BitsPerSample == 8 || BitsPerSample == 16 || BitsPerSample == 32))
            useFallback = TRUE;
        if (SamplesPerPixel > 4)
            useFallback = TRUE;
        if (SamplesPerPixel > 1 && BitsPerSample < 8)
            useFallback = TRUE;
        if (PhotometricInterpretation == PHOTOMETRIC_SEPARATED ||
            PhotometricInterpretation == PHOTOMETRIC_CIELAB ||
            PhotometricInterpretation == PHOTOMETRIC_ICCLAB ||
            PhotometricInterpretation == PHOTOMETRIC_ITULAB ||
            PhotometricInterpretation == PHOTOMETRIC_LOGL ||
            PhotometricInterpretation == PHOTOMETRIC_LOGLUV)
            useFallback = TRUE;
        if (PhotometricInterpretation == PHOTOMETRIC_YCBCR && BitsPerSample != 8)
            useFallback = TRUE;
        if (PhotometricInterpretation == PHOTOMETRIC_PALETTE && BitsPerSample > 8)
            useFallback = TRUE;
        if (sampleFormat != SAMPLEFORMAT_UINT)
            useFallback = TRUE;
        if (planar == PLANARCONFIG_SEPARATE && (BitsPerSample < 8 || BitsPerSample > 8))
            useFallback = TRUE;

        if (useFallback)
            bmhd->bmh_Depth = 32;
        if (useFallback) {
            BOOL ok = tiffLoadRGBAFallback(cl, o, tif, imageWidth, imageLength);
            TIFFClose(tif);
            return ok;
        }

                    if (SamplesPerPixel == 2) {
                        tmp_buf = AllocVec(tileWidth * tileLength * 4, 0);
                        pformat = PBPAFMT_RGBA;
                        D(bug("[tiff.datatype] %s[8BPS]: Greyscale + Alpha image\n", __func__));
                    } else {
                        tmp_buf = AllocVec(tileWidth * tileLength * 3, 0);
                        pformat = PBPAFMT_RGB;
                        D(
                        if (BitsPerSample == 1)
                            bug("[tiff.datatype] %s[8BPS]: Black & White image\n", __func__);
                        else
                            bug("[tiff.datatype] %s[8BPS]: %ubit Greyscale image\n", __func__, BitsPerSample);
                        )
                    }

                    D(bug("[tiff.datatype] %s[8BPS]: %ubit Palette Mapped
", __func__, BitsPerSample));
                            D(bug("[tiff.datatype] %s[8BPS]: read %u palette entries
", __func__, 1 << BitsPerSample));
                D(bug("[tiff.datatype] %s[8BPS]: unhandled SamplesPerPixel (%u)
", __func__, SamplesPerPixel));
                            ULONG planeSize = tileWidth * tileLength;
                            int plane, planemax = SamplesPerPixel;
                                if (TIFFReadTile(tif, plnrbuf + plane * planeSize, x, y, 0, plane) < 0) {
                            ULONG planeSize = copyWidth * copyHeight;
                            int plane, planemax = SamplesPerPixel;
                                if (TIFFReadScanline(tif, plnrbuf + plane * planeSize, y, plane) != 1) {
                        ULONG planeRowStride = isTiled ? tileWidth : copyWidth;
                        ULONG outRowStride = isTiled ? tileWidth : copyWidth;
                        ULONG planeSize = planeRowStride * (isTiled ? tileLength : copyHeight);
                        if (SamplesPerPixel == 1) {
                            CopyMem(plnrbuf, buf, planeSize);
                        } else if (SamplesPerPixel == 2) {
                            UBYTE *gplane = plnrbuf + 0 * planeSize;
                            UBYTE *aplane = plnrbuf + 1 * planeSize;
                                for (pcol = 0; pcol < copyWidth; ++pcol) {
                                    int srcIndex = prow * planeRowStride + pcol;
                                    int outIndex = prow * outRowStride + pcol;
                                    buf[2 * outIndex + 0] = gplane[srcIndex];
                                    buf[2 * outIndex + 1] = aplane[srcIndex];
                                }
                            UBYTE *rplane = plnrbuf + 0 * planeSize;
                            UBYTE *gplane = plnrbuf + 1 * planeSize;
                            UBYTE *bplane = plnrbuf + 2 * planeSize;
                                    int srcIndex = prow * planeRowStride + pcol;
                                    int outIndex = prow * outRowStride + pcol;
                                    buf[3 * outIndex + 0] = rplane[srcIndex];
                                    buf[3 * outIndex + 1] = gplane[srcIndex];
                                    buf[3 * outIndex + 2] = bplane[srcIndex];
                                }
                            }
                        } else if (SamplesPerPixel == 4) {
                            UBYTE *rplane = plnrbuf + 0 * planeSize;
                            UBYTE *gplane = plnrbuf + 1 * planeSize;
                            UBYTE *bplane = plnrbuf + 2 * planeSize;
                            UBYTE *aplane = plnrbuf + 3 * planeSize;
                            for (prow = 0; prow < copyHeight; ++prow) {
                                for (pcol = 0; pcol < copyWidth; ++pcol) {
                                    int srcIndex = prow * planeRowStride + pcol;
                                    int outIndex = prow * outRowStride + pcol;
                                    buf[4 * outIndex + 0] = rplane[srcIndex];
                                    buf[4 * outIndex + 1] = gplane[srcIndex];
                                    buf[4 * outIndex + 2] = bplane[srcIndex];
                                    buf[4 * outIndex + 3] = aplane[srcIndex];
                                BOOL hasAlpha = (SamplesPerPixel == 2);
                                        pixval = buf[i * SamplesPerPixel];
                                    if (hasAlpha) {
                                        tmp_buf[4 * i + 0] = pixval;
                                        tmp_buf[4 * i + 1] = pixval;
                                        tmp_buf[4 * i + 2] = pixval;
                                        tmp_buf[4 * i + 3] = buf[i * SamplesPerPixel + 1];
                                    } else {
                                        tmp_buf[3 * i + 0] = pixval;
                                        tmp_buf[3 * i + 1] = pixval;
                                        tmp_buf[3 * i + 2] = pixval;
                                    }
                                bytesPerPixel = hasAlpha ? 4 : 3;
                tmp_buf = AllocVec(tileWidth * tileLength * 4, MEMF_ANY);
                pformat = PBPAFMT_RGBA;
            } else if (SamplesPerPixel == 2 && PhotometricInterpretation < PHOTOMETRIC_RGB) {
                tmp_buf = AllocVec(tileWidth * tileLength * 4, MEMF_ANY);
                D(bug("[tiff.datatype] %s[16BPS]: Greyscale + Alpha image\n", __func__));
            for (y = 0; !done && y < bmhd->bmh_Height; y += tileLength) {
                for (x = 0; !done && x < bmhd->bmh_Width; x += tileWidth) {
                        ULONG rowBytes = tmp_buf ? tiffRowBytes(tileWidth, pformat)
                                                   : (tileWidth * SamplesPerPixel * (BitsPerSample / 8));
                                          (IPTR)(tmp_buf ? tmp_buf : buf),
                                          rowBytes,
            } else if (SamplesPerPixel == 2 && PhotometricInterpretation < PHOTOMETRIC_RGB) {
                tmp_buf = AllocVec(tileWidth * tileLength * 4, 0);
                pformat = PBPAFMT_RGBA;
                D(bug("[tiff.datatype] %s[32BPS]: Greyscale + Alpha image\n", __func__));
                                PixelArrayMod = tiffRowBytes(tileWidth, pformat);
                                PixelArrayMod = tileWidth * SamplesPerPixel * (BitsPerSample / 8);
                        PixelArrayMod = tiffRowBytes(bmhd->bmh_Width, pformat);
                        PixelArrayMod = bmhd->bmh_Width * SamplesPerPixel * (BitsPerSample / 8);
        D(bug("[tiff.datatype] %s: done, cleaning up...
", __func__));

                    if ((BitsPerSample < 8) || (SamplesPerPixel > 1))
                        tmp_buf = AllocVec(tileWidth * tileLength, 0);

                    if (TIFFGetField(tif, TIFFTAG_COLORMAP, &red_colormap, &green_colormap, &blue_colormap)) {
                        struct ColorRegister    *colorregs = 0;
                        ULONG                   *cregs = 0;
                        SetDTAttrs(o, NULL, NULL, PDTA_NumColors, 1 << BitsPerSample, TAG_DONE);
                        if (GetDTAttrs(o, PDTA_ColorRegisters, (IPTR) &colorregs,
                                          PDTA_CRegs         , (IPTR) &cregs    ,
                                          TAG_DONE                              ) == 2) {
                            for(int i = 0; i < (1 << BitsPerSample); i++) {
                                colorregs->red   = red_colormap[i] >> 8;
                                colorregs->green = green_colormap[i] >> 8;
                                colorregs->blue  = blue_colormap[i] >> 8;

                                *cregs++ = ((ULONG)colorregs->red)   * 0x01010101;
                                *cregs++ = ((ULONG)colorregs->green) * 0x01010101;
                                *cregs++ = ((ULONG)colorregs->blue)  * 0x01010101;

                                colorregs++;
                            }
                            D(bug("[tiff.datatype] %s[8BPS]: read %u palette entries\n", __func__, 1 << BitsPerSample));
                        } /* if (GetDTAttrs(o, ... */
                    }
                    pformat = PBPAFMT_LUT8;
                }
            } else if (SamplesPerPixel == 3)
                pformat = PBPAFMT_RGB;
            else if (SamplesPerPixel == 4)
                pformat = PBPAFMT_RGBA;
            else {
                D(bug("[tiff.datatype] %s[8BPS]: unhandled SamplesPerPixel (%u)\n", __func__, SamplesPerPixel));
            }

            if (planar == PLANARCONFIG_SEPARATE) {
                D(bug("[tiff.datatype] %s[8BPS]: data stored in planes\n", __func__));
                plnrbuf = AllocVec(buffersize * SamplesPerPixel, MEMF_ANY);
                D(bug("[tiff.datatype] %s[8BPS]: allocated conversion buffer (%u x %u x %u = %ubytes) @ 0x%p\n", __func__, tileWidth, tileLength, SamplesPerPixel, buffersize * SamplesPerPixel, plnrbuf));
            } else if ((PhotometricInterpretation == PHOTOMETRIC_YCBCR) && (useYCbCr)) {
                D(bug("[tiff.datatype] %s[8BPS]: YCBCR data\n", __func__));
                ycbcrbuf = AllocVec(buffersize * SamplesPerPixel, MEMF_ANY);
                D(bug("[tiff.datatype] %s[8BPS]: allocated conversion buffer (%ubytes) @ 0x%p\n", __func__, buffersize * SamplesPerPixel, ycbcrbuf));
            }

            for (y = 0; !done && y < bmhd->bmh_Height; y += tileLength) {
                for (x = 0; !done && x < bmhd->bmh_Width; x += tileWidth) {
                    ULONG copyWidth  = MIN(tileWidth,  bmhd->bmh_Width  - x);
                    ULONG copyHeight = MIN(tileLength, bmhd->bmh_Height - y);

                    if (isTiled) {
                        D(bug("[tiff.datatype] %s[8BPS]: Tiled read %ux%u @ %u,%u...\n", __func__,
                              tileWidth, tileLength, x, y));
                        if (!(plnrbuf)) {
                            if (TIFFReadTile(tif, (ycbcrbuf) ? ycbcrbuf : buf, x, y, 0, 0) < 0) {
                                done = TRUE;
                                break;
                            }
                            if (ycbcrbuf) {
                                tiffConvertYCbCr(pformat, tileWidth, tileLength, ycbcrbuf, buf);
                            }
                        } else {
                            int plane, planemax = (SamplesPerPixel == 2) ? 1 : SamplesPerPixel;
                            for (plane = 0; plane < planemax; ++plane) {
                                if (TIFFReadTile(tif, plnrbuf + plane * copyWidth * copyHeight, x, y, 0, plane) < 0) {
                                    done = TRUE;
                                    break;
                                }
                            }
                        }
                    } else {
                        D(bug("[tiff.datatype] %s[8BPS]: Scanline read line %u...\n", __func__, y));
                        copyWidth  = bmhd->bmh_Width;
                        copyHeight = 1;
                        if (!(plnrbuf)) {
                            if (TIFFReadScanline(tif,  (ycbcrbuf) ? ycbcrbuf : buf, y, 0) != 1) {
                                done = TRUE;
                                break;
                            }
                            if (ycbcrbuf) {
                                tiffConvertYCbCr(pformat, copyWidth, copyHeight, ycbcrbuf, buf);
                            }
                        } else {
                            int plane, planemax = (SamplesPerPixel == 2) ? 1 : SamplesPerPixel;
                            for (plane = 0; plane < planemax; ++plane) {
                                if (TIFFReadScanline(tif, plnrbuf + plane * copyWidth * copyHeight, y, plane) != 1) {
                                    done = TRUE;
                                    break;
                                }
                            }
                        }
                    }
                    if (plnrbuf) {
                        int prow, pcol;
                        if (SamplesPerPixel == 2) {
                            for (prow = 0; prow < copyHeight; ++prow) {
                                CopyMem(plnrbuf, buf, copyWidth);
                            }
                        } else  if (SamplesPerPixel == 3) {
                            // src: plnrbuf contains R-plane then G-plane then B-plane, each plane = tileWidth*tileLength bytes
                            UBYTE *rplane = plnrbuf + 0 * copyWidth * copyHeight;
                            UBYTE *gplane = plnrbuf + 1 * copyWidth * copyHeight;
                            UBYTE *bplane = plnrbuf + 2 * copyWidth * copyHeight;
                            for (prow = 0; prow < copyHeight; ++prow) {
                                for (pcol = 0; pcol < copyWidth; ++pcol) {
                                    int bout = prow * copyWidth + pcol;
                                    buf[3 * bout + 0] = rplane[bout];
                                    buf[3 * bout + 1] = gplane[bout];
                                    buf[3 * bout + 2] = bplane[bout];
                                }
                            }
                        }
                    }

                    if (done) break;

                    UBYTE *pixelDataPtr = NULL;
                    ULONG bytesPerPixel;

                    if (tmp_buf) {
                        /* convert into tmp_buf.
                           For tiled reads tmp_buf must contain tileWidth*tileLength pixels (or 3* that for RGB),
                           for scanline reads convert only copyWidth pixels into the start of tmp_buf. */

                        if (isTiled) {
                            /* convert the full tile (tileWidth * tileLength pixels) */
                            const int pixels_in_tile = tileWidth * tileLength;

                            if (PhotometricInterpretation == PHOTOMETRIC_PALETTE) {
                                /* store 1 byte per pixel = palette indices */
                                for (int i = 0; i < pixels_in_tile; ++i) {
                                    ULONG bit_offset = i * BitsPerSample;
                                    UBYTE byte = buf[bit_offset / 8];
                                    UBYTE shift = 8 - BitsPerSample - (bit_offset % 8);
                                    UBYTE mask = (1 << BitsPerSample) - 1;
                                    tmp_buf[i] = (byte >> shift) & mask;
                                }
                                bytesPerPixel = 1;
                            } else {
                                /* produce RGB triples in tmp_buf: 3 bytes per pixel */
                                for (int i = 0; i < pixels_in_tile; ++i) {
                                    UBYTE pixval;
                                    if (BitsPerSample == 8)
                                        pixval = buf[i];
                                    else {
                                        ULONG bit_offset = i * BitsPerSample;
                                        UBYTE byte = buf[bit_offset / 8];
                                        UBYTE shift = 8 - BitsPerSample - (bit_offset % 8);
                                        UBYTE mask = (1 << BitsPerSample) - 1;
                                        pixval = (byte >> shift) & mask;
                                        pixval = (pixval * 255) / mask; /* scale to 8-bit */
                                    }

                                    if (PhotometricInterpretation == PHOTOMETRIC_MINISWHITE)
                                        pixval = 255 - pixval;

                                    tmp_buf[3 * i + 0] = pixval;
                                    tmp_buf[3 * i + 1] = pixval;
                                    tmp_buf[3 * i + 2] = pixval;
                                }
                                bytesPerPixel = 3;
                            }
                        } else {
                            /* scanline: convert only the single row (copyWidth pixels) into tmp_buf start */
                            if (PhotometricInterpretation == PHOTOMETRIC_PALETTE) {
                                for (ULONG i = 0; i < copyWidth; ++i) {
                                    if (BitsPerSample == 8) {
                                        tmp_buf[i] = buf[i];
                                    } else {
                                        ULONG bit_offset = i * BitsPerSample;
                                        UBYTE byte = buf[bit_offset / 8];
                                        UBYTE shift = 8 - BitsPerSample - (bit_offset % 8);
                                        UBYTE mask = (1 << BitsPerSample) - 1;
                                        tmp_buf[i] = (byte >> shift) & mask;
                                    }
                                }
                                bytesPerPixel = 1;
                            } else {
                                for (ULONG i = 0; i < copyWidth; ++i) {
                                    UBYTE pixval;
                                    if (BitsPerSample == 8)
                                        pixval = buf[i];
                                    else {
                                        ULONG bit_offset = i * BitsPerSample;
                                        UBYTE byte = buf[bit_offset / 8];
                                        UBYTE shift = 8 - BitsPerSample - (bit_offset % 8);
                                        UBYTE mask = (1 << BitsPerSample) - 1;
                                        pixval = (byte >> shift) & mask;
                                        pixval = (pixval * 255) / mask;
                                    }
                                    if (PhotometricInterpretation == PHOTOMETRIC_MINISWHITE)
                                        pixval = 255 - pixval;

                                    tmp_buf[3 * i + 0] = pixval;
                                    tmp_buf[3 * i + 1] = pixval;
                                    tmp_buf[3 * i + 2] = pixval;
                                }
                                bytesPerPixel = 3;
                            }
                        }

                        pixelDataPtr = tmp_buf;
                    } else {
                        bytesPerPixel = SamplesPerPixel;
                        pixelDataPtr = buf;
                    }

                    ULONG srcRowBytes;
                    if (isTiled)
                        srcRowBytes = tileWidth * bytesPerPixel;
                    else
                        srcRowBytes = copyWidth * bytesPerPixel;

                    D(bug("[tiff.datatype] %s[8BPS]: rendering %ux%u @ %u,%u, srcRowBytes=%u, bpp=%u\n", __func__,
                          copyWidth, copyHeight, x, y, srcRowBytes, bytesPerPixel));

                    if (!DoSuperMethod(cl, o,
                                       PDTM_WRITEPIXELARRAY,
                                       (IPTR) pixelDataPtr,
                                       pformat,
                                       srcRowBytes,
                                       x,
                                       y,
                                       copyWidth,
                                       copyHeight)) {
                        D(bug("[tiff.datatype] %s[8BPS]: DT object failed to render\n", __func__));
                        done = TRUE;
                        break;
                    }
                }
            }
            if ((planar == PLANARCONFIG_SEPARATE) && (SamplesPerPixel == 2)) {
                // TODO: Load the alpha channel.
            }
            if (ycbcrbuf)
                FreeVec(ycbcrbuf);
            if (plnrbuf)
                FreeVec(plnrbuf);
        } else if (BitsPerSample == 16) {
            BOOL done = FALSE;

            if (SamplesPerPixel == 4) {
                tmp_buf = AllocVec(tileWidth * tileLength * SamplesPerPixel, MEMF_ANY);
                pformat = PBPAFMT_RGBA;
            } else if (SamplesPerPixel < 4) {
                tmp_buf = AllocVec(tileWidth * tileLength * 3, MEMF_ANY);
                pformat = PBPAFMT_RGB;
                if (PhotometricInterpretation < PHOTOMETRIC_RGB) {
                    D(bug("[tiff.datatype] %s[16BPS]: %ubit Greyscale image\n", __func__, BitsPerSample);                    )
                }
            } else {
                D(bug("[tiff.datatype] %s[16BPS]: unhandled SamplesPerPixel\n", __func__));
            }

            for(y = 0; !done && y < bmhd->bmh_Height; y += tileLength) {
                for(x = 0; !done && x < bmhd->bmh_Width; x += tileWidth) {
                    if (isTiled) {
                        D(bug("[tiff.datatype] %s[16BPS]: Tiled read %ux%u @ %u,%u...\n", __func__, tileWidth, tileLength, x, y));
                        if (TIFFReadTile(tif, buf, x, y, 0, 0) < 0) {
                            done = TRUE;
                            break;
                        }
                    } else {
                        D(bug("[tiff.datatype] %s[16BPS]: Scanline read...\n", __func__));
                        if (TIFFReadScanline(tif, buf, y, 0) != 1) {
                            done = TRUE;
                            break;
                        }
                    }

                    if (!done) {
                        if (tmp_buf) {
                            D(bug("[tiff.datatype] %s[16BPS]: calling tiffConvert16to8\n", __func__));
                            tiffConvert16to8(PhotometricInterpretation, SamplesPerPixel, pformat, tileWidth, tileLength, buf, tmp_buf);
                        }
                        D(bug("[tiff.datatype] %s[16BPS]: rendering to datatype obj...\n", __func__));
                        if(!DoSuperMethod(cl, o,
                                          PDTM_WRITEPIXELARRAY,
                                          (IPTR) tmp_buf ? tmp_buf : buf,
                                          pformat,
                                          tmp_buf ? (tileWidth >> 3): 0,
                                          x,
                                          y,
                                          MIN(tileWidth, bmhd->bmh_Width - x),
                                          MIN(tileLength, bmhd->bmh_Height - y))) {
                            D(bug("[tiff.datatype] %s[16BPS]: DT object failed to render\n", __func__));
                            //png_error(png.png_ptr, "Out of memory!");
                            done = TRUE;
                            break;
                        }
                    }
                }
            }
        } else if (BitsPerSample == 32) {
            BOOL done = FALSE;

            if (SamplesPerPixel == 4) {
                tmp_buf = AllocVec(tileWidth * tileLength * 4, 0);
                pformat = PBPAFMT_RGBA;
            } else if (SamplesPerPixel < 4) {
                tmp_buf = AllocVec(tileWidth * tileLength * 3, 0);
                pformat = PBPAFMT_RGB;
                if (PhotometricInterpretation < PHOTOMETRIC_RGB) {
                    D(bug("[tiff.datatype] %s[32BPS]: %ubit Greyscale image\n", __func__, BitsPerSample);                    )
                }
            } else {
                D(bug("[tiff.datatype] %s[32BPS]: unhandled SamplesPerPixel\n", __func__));
            }

            for (y = 0; !done && y < bmhd->bmh_Height; ) {
                APTR PixelData;
                ULONG PixelArrayMod;

                if (isTiled) {
                    for (x = 0; !done && x < bmhd->bmh_Width; x += tileWidth) {
                        D(bug("[tiff.datatype] %s[32BPS]: Tiled read %ux%u @ %u,%u...\n", __func__,
                              tileWidth, tileLength, x, y));

                        if (TIFFReadTile(tif, buf, x, y, 0, 0) < 0) {
                            done = TRUE;
                            break;
                        }

                        if (!done) {
                            UWORD copyWidth  = MIN(tileWidth,  bmhd->bmh_Width  - x);
                            UWORD copyHeight = MIN(tileLength, bmhd->bmh_Height - y);

                            if (tmp_buf) {
                                tiffConvert32to8(PhotometricInterpretation, SamplesPerPixel,
                                                 pformat, tileWidth, tileLength, buf, tmp_buf);

                                PixelArrayMod = tileWidth;
                                PixelData     = tmp_buf;
                            } else {
                                PixelArrayMod = tileWidth * SamplesPerPixel;
                                PixelData     = buf;
                            }

                            if(!DoSuperMethod(cl, o,
                                              PDTM_WRITEPIXELARRAY,
                                              (IPTR)PixelData,
                                              pformat,
                                              PixelArrayMod,
                                              x, y,
                                              copyWidth, copyHeight)) {
                                D(bug("[tiff.datatype] %s[32BPS]: DT object failed to render\n", __func__));
                                done = TRUE;
                                break;
                            }
                        }
                    }
                    y += tileLength;  // next tile row
                } else {
                    D(bug("[tiff.datatype] %s[32BPS]: Scanline read line %u...\n", __func__, y));

                    if (TIFFReadScanline(tif, buf, y, 0) != 1) {
                        done = TRUE;
                        break;
                    }

                    if (tmp_buf) {
                        tiffConvert32to8(PhotometricInterpretation, SamplesPerPixel,
                                         pformat, bmhd->bmh_Width, 1, buf, tmp_buf);

                        PixelArrayMod = bmhd->bmh_Width;
                        PixelData     = tmp_buf;
                    } else {
                        PixelArrayMod = bmhd->bmh_Width * SamplesPerPixel;
                        PixelData     = buf;
                    }

                    if(!DoSuperMethod(cl, o,
                                      PDTM_WRITEPIXELARRAY,
                                      (IPTR)PixelData,
                                      pformat,
                                      PixelArrayMod,
                                      0, y,
                                      bmhd->bmh_Width, 1)) {
                        D(bug("[tiff.datatype] %s[32BPS]: DT object failed to render\n", __func__));
                        done = TRUE;
                        break;
                    }

                    y += 1; // next scanline
                }
            }
        }

        D(bug("[tiff.datatype] %s: done, cleaning up...\n", __func__));

        FreeVec(tmp_buf);
        FreeVec(buf);

        TIFFClose(tif);

        return TRUE;
    }
    D(bug("[tiff.datatype] %s: failed to open tif\n", __func__));

    return FALSE;
}

/**************************************************************************************************/

/**************************************************************************************************/

static BOOL SaveTIFF(struct IClass *cl, Object *o, struct dtWrite *dtw )
{
    D(bug("[tiff.datatype] %s()\n", __func__));

    return TRUE;
}

/**************************************************************************************************/

IPTR TIFF__OM_NEW(Class *cl, Object *o, Msg msg)
{
    Object *newobj;
    
    D(bug("[tiff.datatype] %s()\n", __func__));

    newobj = (Object *)DoSuperMethodA(cl, o, (Msg)msg);
    if (newobj) {
        if (!LoadTIFF(cl, newobj)) {
            CoerceMethod(cl, newobj, OM_DISPOSE);
            newobj = NULL;
        }
    }
    
    return (IPTR)newobj;
}

/**************************************************************************************************/

IPTR TIFF__DTM_WRITE(Class *cl, Object *o, struct dtWrite *dtw)
{
    D(bug("[tiff.datatype] %s()\n", __func__));
    if ((dtw -> dtw_Mode) == DTWM_RAW) {
        /* Local data format requested */
        return SaveTIFF(cl, o, dtw );
    } else {
        /* Pass msg to superclass (which writes an IFF ILBM picture)... */
        return DoSuperMethodA( cl, o, (Msg)dtw );
    }
}

/**************************************************************************************************/
