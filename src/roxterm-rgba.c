/*
    roxterm - PROFILE/GTK terminal emulator with tabs
    Copyright (C) 2019 Tony Houghton <h@realh.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include "roxterm-rgba.h"

#define ROXTERM_RGBA_MULTIPLIER 255

RoxtermRGBA roxterm_rgba_from_gdk(const GdkRGBA *grgba)
{
    RoxtermRGBA rrgba;
    rrgba.r = (RoxtermRGBAChannel) (grgba->red * ROXTERM_RGBA_MULTIPLIER);
    rrgba.g = (RoxtermRGBAChannel) (grgba->green * ROXTERM_RGBA_MULTIPLIER);
    rrgba.b = (RoxtermRGBAChannel) (grgba->blue * ROXTERM_RGBA_MULTIPLIER);
    rrgba.a = (RoxtermRGBAChannel) (grgba->alpha * ROXTERM_RGBA_MULTIPLIER);
    return rrgba;
}

RoxtermRGBA roxterm_rgba_parse(const char *s)
{
    GdkRGBA grgba;
    if (!s || !gdk_rgba_parse(&grgba, s))
    {
        if (s)
            g_critical("Invalid colour spec '%s'", s);
        // Play safe, don't return uninitialised value even though it's in
        // valid memory
        RoxtermRGBA rrgba;
        rrgba.v = 0;
        return rrgba;
    }
    return roxterm_rgba_from_gdk(&grgba);
}

void roxterm_rgba_to_gdk(RoxtermRGBA rrgba, GdkRGBA *grgba)
{
    grgba->red = (double) rrgba.r / ROXTERM_RGBA_MULTIPLIER;
    grgba->green = (double) rrgba.g / ROXTERM_RGBA_MULTIPLIER;
    grgba->blue = (double) rrgba.b / ROXTERM_RGBA_MULTIPLIER;
    grgba->alpha = (double) rrgba.a / ROXTERM_RGBA_MULTIPLIER;
}

char *roxterm_rgba_to_string(RoxtermRGBA rrgba)
{
    GdkRGBA grgba;
    roxterm_rgba_to_gdk(rrgba, &grgba);
    return gdk_rgba_to_string(&grgba);
}
