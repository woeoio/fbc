/*
 *  Copyright (C) 2004 Sterling Christensen (sterling@engineer.com)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "QB_gfx_main.h"
#include "QB_gfx_text.h"

FBCALL int fb_GfxScrollUp (Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint16 scrollAmount, Uint32 fillColor)
{
    Uint8 *colorptr;
    SDL_Rect src, dst;

    if (SANITY_CHECK_CONDITION || scrollAmount < 1) return -1;

    if (y1 > y2) {
        src.y = y2;
        src.h = y1 - y2 + 1;
    } else {
        src.y = y1;
        src.h = y2 - y1 + 1;
    }
    if (x1 > x2) {
        src.x = x2;
        src.w = x1 - x2 + 1;
    } else {
        src.x = x1;
        src.w = x2 - x1 + 1;
    }

    src.y += scrollAmount;
    src.h -= scrollAmount;

    dst.x = src.x;
    dst.y = src.y - scrollAmount;

    if ( SDL_BlitSurface(fb_GfxInfo.screen, &src, fb_GfxInfo.screen, &dst) ) return -1;

    if (fillColor == DEFAULT_COLOR) return 0;

    if (fb_GfxInfo.screen->format->BitsPerPixel > 8)
    {
        colorptr = (Uint8 *) & fillColor;
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            fillColor = SDL_MapRGB(fb_GfxInfo.screen->format, colorptr[0], colorptr[1], colorptr[2]);
        } else {
            fillColor = SDL_MapRGB(fb_GfxInfo.screen->format, colorptr[3], colorptr[2], colorptr[1]);
        }
    }

    src.y += src.h - scrollAmount;
    src.h = scrollAmount;
    SDL_FillRect(fb_GfxInfo.screen, &src, fillColor);

    return 0;
}

void fb_GfxPrintBuffer (char *str, int mask)
{
    SDL_Surface *surface;
    SDL_Color palette[2];
    SDL_Rect src, dst, oldClip;
    char c = 0, after, drawFlag;
    int ret, length;
    Sint16 ox, x, y;

    SANITY_CHECK;

    if (fb_GfxInfo.fontData == NULL) fb_GfxSetFont((Uint8*)fb_GfxDefaultFont8x16);

    surface = SDL_CreateRGBSurfaceFrom(fb_GfxInfo.fontData,
                                       fb_GfxInfo.charWidth, fb_GfxInfo.charHeight << 8,
                                       1, (fb_GfxInfo.charWidth+7) >> 3,
                                       0, 0, 0, 0);

    if (surface == NULL) return;

    x = fb_GfxInfo.text_cursorX;
    y = fb_GfxInfo.text_cursorY;

    if (fb_GfxInfo.printAffectedByView)
    {
        x += fb_GfxInfo.view.x;
        y += fb_GfxInfo.view.y;
    }
    else
    {
        oldClip = fb_GfxInfo.screen->clip_rect;
        SDL_SetClipRect(fb_GfxInfo.screen, NULL);
    }

    if (fb_GfxInfo.screen->format->BytesPerPixel == 1)
    {
//        if (fb_GfxInfo.paletteChanged) fb_GfxFlushPalette();

        palette[0] = fb_GfxInfo.screen->format->palette->colors[fb_GfxInfo.fontBGColor];
        palette[1] = fb_GfxInfo.screen->format->palette->colors[fb_GfxInfo.defaultColor];
    }
    else
    {
        /* Convert from 0xRRGGBBAA: */
        palette[0].r = (Uint8)(fb_GfxInfo.fontBGColor >> 24);
        palette[0].g = (Uint8)(fb_GfxInfo.fontBGColor >> 16);
        palette[0].b = (Uint8)(fb_GfxInfo.fontBGColor >> 8);
        palette[1].r = (Uint8)(fb_GfxInfo.defaultColor >> 24);
        palette[1].g = (Uint8)(fb_GfxInfo.defaultColor >> 16);
        palette[1].b = (Uint8)(fb_GfxInfo.defaultColor >> 8);
    }

    SDL_SetColors(surface, palette, 0, 2);

    if (fb_GfxInfo.fontBgTransparent) SDL_SetColorKey(surface, SDL_SRCCOLORKEY, 0);

    ret = 0;

    src.x = 0;
    src.w = fb_GfxInfo.charWidth;
    src.h = fb_GfxInfo.charHeight;

    if (str == NULL) length = 0; else length = -1;

 	after = 0;
 	//if( (mask & FB_PRINT_NEWLINE) != 0 )
 	//	after = 13;

    while (length != 0 || after != 0)
    {
        if (length != 0) {
            c = *str++;
        } else if (after != 0) {
            c = after;
            after = 0;
            length = 1;
        }

        if (length < 0)
        {
            if (c == 0)
            {
                if (after != 0)
                {
                    c = after;
                    after = 0;
                    length = 0;
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            length--;
        }

        if (fb_GfxInfo.tabDist)
        {
            drawFlag = 0;

            switch (c)
            {
            case 9:
                ox = x;
                if (fb_GfxInfo.printAffectedByView) {
                    x = ((x - fb_GfxInfo.view.x) / fb_GfxInfo.tabDist + 1) * fb_GfxInfo.tabDist + fb_GfxInfo.view.x;
                    if (fb_GfxInfo.letterWrap)
                    {
                        if (x + fb_GfxInfo.charWidth > fb_GfxInfo.view.x + fb_GfxInfo.view.w) c = 13;
                    }
                } else {
                    x = (x / fb_GfxInfo.tabDist + 1) * fb_GfxInfo.tabDist;
                    if (fb_GfxInfo.letterWrap)
                    {
                        if (x + fb_GfxInfo.charWidth > fb_GfxInfo.screen->w) c = 13;
                    }
                }

                if (fb_GfxInfo.fontBgTransparent == 0)
                {
                    fb_GfxFBoxEx(ox, y, x-ox-1, fb_GfxInfo.charHeight-1, fb_GfxInfo.fontBGColor);
                }

                if (c == 9) break;

            case 10:
            case 13:
stringex_doTheCarriageReturnThang:
                if (fb_GfxInfo.printAffectedByView) {
                    x = fb_GfxInfo.view.x;
                } else {
                    x = 0;
                }
                y += fb_GfxInfo.charHeight;

                if (fb_GfxInfo.printScrolls)
                {
                    if (fb_GfxInfo.printAffectedByView)
                    {
                        if (y + fb_GfxInfo.charHeight > fb_GfxInfo.view.y + fb_GfxInfo.view.h)
                        {
                            fb_GfxScrollUp(fb_GfxInfo.view.x,
                                           fb_GfxInfo.view.y,
                                           fb_GfxInfo.view.x + fb_GfxInfo.view.w / fb_GfxInfo.charWidth * fb_GfxInfo.charWidth - 1,
                                           fb_GfxInfo.view.y + fb_GfxInfo.view.h / fb_GfxInfo.charHeight * fb_GfxInfo.charHeight - 1,
                                           fb_GfxInfo.charHeight, fb_GfxInfo.fontBGColor);

                            y -= fb_GfxInfo.charHeight;
                        }
                    }
                    else
                    {
                        if (y + fb_GfxInfo.charHeight > fb_GfxInfo.screen->h)
                        {
                            fb_GfxScrollUp(0, 0,
                                           fb_GfxInfo.screen->w / fb_GfxInfo.charWidth * fb_GfxInfo.charWidth - 1,
                                           fb_GfxInfo.screen->h / fb_GfxInfo.charHeight * fb_GfxInfo.charHeight - 1,
                                           fb_GfxInfo.charHeight, fb_GfxInfo.fontBGColor);

                            y -= fb_GfxInfo.charHeight;
                        }
                    }
                }

                break;

            default:
                drawFlag = 1;
            }
        }
        else
        {
            drawFlag = 1;
        }

        if (drawFlag)
        {
            /* Blit the character: */
            src.y = (Uint8)c * fb_GfxInfo.charHeight;
            dst.x = x;
            dst.y = y;
            ret |= SDL_BlitSurface(surface, &src, fb_GfxInfo.screen, &dst);

            x += fb_GfxInfo.charWidth;
            if (fb_GfxInfo.letterWrap)
            {
                if (fb_GfxInfo.printAffectedByView)
                {
                    if (x + fb_GfxInfo.charWidth > fb_GfxInfo.screen->w)
                    {
                        drawFlag = 0;
                        goto stringex_doTheCarriageReturnThang;
                    }
                }
                else
                {
                    if (x + fb_GfxInfo.charWidth > fb_GfxInfo.view.x + fb_GfxInfo.view.w)
                    {
                        drawFlag = 0;
                        goto stringex_doTheCarriageReturnThang;
                    }
                }
            }
        }
    }

    SDL_FreeSurface(surface);

    if (fb_GfxInfo.printAffectedByView)
    {
        x -= fb_GfxInfo.view.x;
        y -= fb_GfxInfo.view.y;
    }
    else
    {
        SDL_SetClipRect(fb_GfxInfo.screen, &oldClip);
    }

    fb_GfxInfo.text_cursorX = x;
    fb_GfxInfo.text_cursorY = y;

//    return (int)((Uint32)y << 16) | ((Uint32)x & 0xFFFF);
    return;
}

