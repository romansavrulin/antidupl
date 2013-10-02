/*
* AntiDupl Dynamic-Link Library.
*
* Copyright (c) 2002-2013 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy 
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in 
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#define OPJ_STATIC
#include "LibOpenJpeg/openjpeg.h"

#include "adPerformance.h"
#include "adOpenJpeg.h"

namespace ad
{
    TOpenJpeg* TOpenJpeg::Load(HGLOBAL hGlobal)
    {
        if(hGlobal)
        {
            TView *pView = Load((unsigned char*)::GlobalLock(hGlobal), ::GlobalSize(hGlobal));
            ::GlobalUnlock(hGlobal);
            if(pView)
            {
                TOpenJpeg* pOpenJpeg = new TOpenJpeg();
                pOpenJpeg->m_pView = pView;
                pOpenJpeg->m_format = TImage::Jp2;
                return pOpenJpeg;
            }
        }
        return NULL;
    }

    bool TOpenJpeg::Supported(HGLOBAL hGlobal)
    {
        if(hGlobal)
        {
            int codecFormat = CodecFormat((unsigned char*)::GlobalLock(hGlobal), ::GlobalSize(hGlobal));
            ::GlobalUnlock(hGlobal);
            return codecFormat != CODEC_UNKNOWN;
        }
        return false;
    }

    int TOpenJpeg::CodecFormat(unsigned char *data, size_t size)
    {
        unsigned char j2k[2] = {0xff, 0x4f};
        unsigned char jp2[4] = {0x6a, 0x50, 0x20, 0x20};
        if(size >= 2 && memcmp(data, j2k, sizeof(j2k)) == 0)
            return CODEC_J2K;
        if(size >= 8 && memcmp(data + 4, jp2, sizeof(jp2)) == 0)
            return CODEC_JP2;
        return CODEC_UNKNOWN;
    }

    TView* TOpenJpeg::Load(unsigned char *data, size_t size)
    {
        AD_FUNCTION_PERFORMANCE_TEST
        TView *pView = NULL;
        opj_dinfo_t *dinfo = opj_create_decompress((OPJ_CODEC_FORMAT)CodecFormat(data, size));
        if(dinfo)
        {
            opj_dparameters_t parameters;
            opj_set_default_decoder_parameters(&parameters);
            opj_setup_decoder(dinfo, &parameters);
            opj_cio_t *cio = opj_cio_open((opj_common_ptr)dinfo, data, (int)size);
            if(cio)
            {
                opj_image_t *image = opj_decode(dinfo, cio);
                if(image)
                {
                    size_t width = image->x1 - image->x0;
                    size_t height = image->y1 - image->y0;
                    if(image->color_space != CLRSPC_UNKNOWN && width > 0 && width <= SHRT_MAX && 
                        height > 0 && height <= SHRT_MAX && image->numcomps > 0)
                    {
                        opj_image_comp_t &c0 = image->comps[0];
                        if(image->numcomps >= 3)
                        {
                            opj_image_comp_t &c1 = image->comps[1];
                            opj_image_comp_t &c2 = image->comps[2];
                            if(image->color_space == CLRSPC_SRGB || image->color_space == CLRSPC_UNSPECIFIED)
                            {
                                if(c0.prec >= 8 && c0.dx == 1 && c1.dx == 1 && c2.dx == 1 && c0.dy == 1 && c1.dy == 1 && c2.dy == 1)
                                {
                                    pView = new TView(width, height, width*TView::PixelSize(TView::Bgra32), TView::Bgra32, NULL);
                                    Simd::InterleaveBgra(pView->data, width*height, 
                                        c2.data, c2.prec, c2.sgnd != 0, c1.data, c1.prec, c1.sgnd != 0, c0.data, c0.prec, c0.sgnd != 0, 0xFF);
                                }
                            }
                            else if (image->color_space == CLRSPC_SYCC)
                            { 
                                if(c0.prec >= 8 && (c1.dx == 1 || c1.dx == 2) && (c1.dy == 1 || c1.dy == 2) 
                                    && (c1.dy != 1 || c1.dx != 2) && width%c1.dx == 0 && height%c1.dy == 0)
                                {
                                    pView = new TView(width, height, width*TView::PixelSize(TView::Bgra32), TView::Bgra32, NULL);
                                    Simd::YuvToBgra(pView->data, width, height, pView->stride,
                                        c0.data, c1.data, c2.data, c1.dx, c1.dy, c0.prec, 0xFF);
                                }
                            }
                        }
                        else
                        {
                            if(image->color_space == CLRSPC_GRAY || image->color_space == CLRSPC_UNSPECIFIED)
                            {
                                if(c0.prec >= 8 && c0.dx == 1 && c0.dy == 1)
                                {
                                    pView = new TView(width, height, width*TView::PixelSize(TView::Bgra32), TView::Bgra32, NULL);
                                    Simd::InterleaveBgra(pView->data, width*height, c0.data, c0.prec, c0.sgnd != 0, 0xFF);
                                }
                            }
                        }
                        AD_PERFORMANCE_TEST_SET_SIZE(width*height)
                    }
                    opj_image_destroy(image); 
                }
                opj_cio_close(cio);
            }
            opj_destroy_decompress(dinfo);
        }
        return pView;
    }
}
