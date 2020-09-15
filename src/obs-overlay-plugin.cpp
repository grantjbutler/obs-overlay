/*
 obs-overlay
 Copyright (C) 2020 Grant Butler
 
 Forked and modified from obs-ios-camera-source, created by Will Townsend.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program. If not, see <https://www.gnu.org/licenses/>
 */

#include <obs-module.h>
#include <obs.hpp>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-overlay-plugin", "en-US")

// TODO: Fetch this from a file? Embed this as part of the build process?
#define OVERLAY_PLUGIN_VERSION "0.1"

extern void register_overlay_source();

bool obs_module_load(void)
{
    blog(LOG_INFO, "Loading Overlay Plugin (version %s)", OVERLAY_PLUGIN_VERSION);
    register_overlay_source();
    return true;
}
