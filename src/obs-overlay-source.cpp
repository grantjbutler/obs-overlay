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

#include <graphics/graphics.h>
#include <graphics/math-defs.h>
#include <util/platform.h>
#include <util/bmem.h>
#include <obs-module.h>
#include <chrono>
#include <Portal.hpp>
#include <usbmuxd.h>
#include <obs-avc.h>

#define SETTING_DEVICE_UUID "setting_device_uuid"
#define SETTING_DEVICE_UUID_NONE_VALUE "null"

class OverlayInput: public portal::PortalDelegate
{
public:
    obs_source_t *source;
    obs_data_t *settings;

    gs_texture_t *tex = nullptr;
    uint8_t *bits = nullptr;
    pthread_mutex_t bits_mutex;

    bool active = false;
    std::string deviceUUID;

    std::shared_ptr<portal::Portal> sharedPortal;
    portal::Portal portal;

    OverlayInput(obs_source_t *source_, obs_data_t *settings)
    : source(source_), settings(settings), portal(this)
    {
        blog(LOG_INFO, "Creating instance of plugin!");

        /// In order for the internal Portal Delegates to work there
        /// must be a shared_ptr to the instance of Portal.
        ///
        /// We create a shared pointer to the heap allocated Portal
        /// instance, and wrap it up in a sharedPointer with a deleter
        /// that doesn't do anything (this is handled automatically with
        /// the class)
        auto null_deleter = [](portal::Portal *portal) { UNUSED_PARAMETER(portal); };
        auto portalReference = std::shared_ptr<portal::Portal>(&portal, null_deleter);
        sharedPortal = portalReference;

        loadSettings(settings);
        active = true;
    }

    inline ~OverlayInput()
    {
		if (tex) {
			obs_enter_graphics();
			gs_texture_destroy(tex);
			obs_leave_graphics();
		}

        if (bits) {
            bfree(bits);
        }
    }

    void activate() {
        blog(LOG_INFO, "Activating");
        active = true;
    }

    void deactivate() {
        blog(LOG_INFO, "Deactivating");
        active = false;
    }

    void loadSettings(obs_data_t *settings) {
        auto device_uuid = obs_data_get_string(settings, SETTING_DEVICE_UUID);

        blog(LOG_INFO, "Loaded Settings: Connecting to device");

        connectToDevice(device_uuid, false);
    }

    void update() {
        pthread_mutex_lock(&bits_mutex);
        
        if (!bits) {
            return;
        }

        if (!tex) {
            obs_enter_graphics();

            const uint8_t *data = bits;
            tex = gs_texture_create(1920, 1080, GS_RGBA, 1, &data,
                        GS_DYNAMIC);

            obs_leave_graphics();
        } else if (tex) {
            obs_enter_graphics();
            gs_texture_set_image(tex, bits, 1920 * 4, false);
            obs_leave_graphics();
        }

        pthread_mutex_unlock(&bits_mutex);
    }

    void render() {
        if (!tex) {
    		return;
        }

        gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
        gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

        gs_technique_begin(tech);
        gs_technique_begin_pass(tech, 0);

        gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
                    tex);
        gs_draw_sprite(tex, 0, 1920, 1080);

        gs_technique_end_pass(tech);
        gs_technique_end(tech);
    }

    void reconnectToDevice()
    {
        if (deviceUUID.size() < 1) {
            return;
        }

        connectToDevice(deviceUUID, true);
    }

    void connectToDevice(std::string uuid, bool force) {

        if (portal._device) {
            // Make sure that we're not already connected to the device
            if (force == false && portal._device->uuid().compare(uuid) == 0 && portal._device->isConnected()) {
                blog(LOG_DEBUG, "Already connected to the device. Skipping.");
                return;
            } else {
                // Disconnect from from the old device
                portal._device->disconnect();
                portal._device = nullptr;
            }
        }

        blog(LOG_INFO, "Connecting to device");

        // Find device
        auto devices = portal.getDevices();
        deviceUUID = std::string(uuid);

        int index = 0;
        std::for_each(devices.begin(), devices.end(), [this, uuid, &index](std::map<int, portal::Device::shared_ptr>::value_type &deviceMap) {
            // Add the device name to the list
            auto _uuid = deviceMap.second->uuid();

            if (_uuid.compare(uuid) == 0) {
                blog(LOG_DEBUG, "comparing \n%s\n%s\n", _uuid.c_str(), uuid.c_str());
                portal.connectToDevice(deviceMap.second);
            }

            index++;
        });
    }

    void portalDeviceDidReceivePacket(std::vector<char> packet, int type, int tag)
    {
        UNUSED_PARAMETER(packet);
        UNUSED_PARAMETER(tag);
        
        try
        {
            if (type == 101) {
                pthread_mutex_lock(&bits_mutex);

                if (!bits) {
                    bits = (uint8_t *)::bzalloc(sizeof(uint8_t) * 1920 * 1080 * 4);
                }

                ::memcpy(bits, packet.data(), sizeof(uint8_t) * 1920 * 1080 * 4);

                pthread_mutex_unlock(&bits_mutex);
            }
        }
        catch (...)
        {
            // This isn't great, but I haven't been able to figure out what is causing
            // the exception that happens when
            //   the phone is plugged in with the app open
            //   OBS Studio is launched with the iOS Camera plugin ready
            // This also doesn't happen _all_ the time. Which makes this 'fun'..
            blog(LOG_INFO, "Exception caught...");
        }
    }

    void portalDidUpdateDeviceList(std::map<int, portal::Device::shared_ptr> deviceList)
    {
        // Update OBS Settings
        blog(LOG_INFO, "Updated device list");

        /// If there is one device in the list, then we should attempt to connect to it.
        /// I would guess that this is the main use case - one device, and it's good to
        /// attempt to automatically connect in this case, and 'just work'.
        ///
        /// If there are multiple devices, then we can't just connect to all devices.
        /// We cannot currently detect if a device is connected to another instance of the
        /// plugin, so it's not safe to attempt to connect to any devices automatically
        /// as we could be connecting to a device that is currently connected elsewhere.
        /// Due to this, if there are multiple devices, we won't do anything and will let
        /// the user configure the instance of the plugin.
        if (deviceList.size() == 1) {

            for (const auto& [index, device] : deviceList) {
                auto uuid = device.get()->uuid();

                auto isFirstTimeConnectingToDevice = deviceUUID.size() == 0;
                auto isDeviceConnected = device.get()->isConnected();
                auto isThisDeviceTheSameAsThePreviouslyConnectedDevice = deviceUUID.compare(uuid) == 0;

                if (isFirstTimeConnectingToDevice || (isThisDeviceTheSameAsThePreviouslyConnectedDevice && !isDeviceConnected)) {

                    // Set the setting so that the UI in OBS Studio is updated
                    obs_data_set_string(this->settings, SETTING_DEVICE_UUID, uuid.c_str());

                    // Connect to the device
                    connectToDevice(uuid, false);
                }
            }

        } else {
            // User will have to configure the plugin manually when more than one device is plugged in
            // due to the fact that multiple instances of the plugin can't subscribe to device events...
        }
    }
};

#pragma mark - Settings Config

static bool refresh_devices(obs_properties_t *props, obs_property_t *p, void *data)
{
    UNUSED_PARAMETER(p);

    auto cameraInput = reinterpret_cast<OverlayInput*>(data);

    cameraInput->portal.reloadDeviceList();
    auto devices = cameraInput->portal.getDevices();

    obs_property_t *dev_list = obs_properties_get(props, SETTING_DEVICE_UUID);
    obs_property_list_clear(dev_list);

    obs_property_list_add_string(dev_list, obs_module_text("OBSOverlay.None"), SETTING_DEVICE_UUID_NONE_VALUE);

    int index = 1;
    std::for_each(devices.begin(), devices.end(), [dev_list, &index](std::map<int, portal::Device::shared_ptr>::value_type &deviceMap) {
        // Add the device uuid to the list.
        // It would be neat to grab the device name somehow, but that will likely require
        // libmobiledevice instead of usbmuxd. Something to look into.
        auto uuid = deviceMap.second->uuid().c_str();
        obs_property_list_add_string(dev_list, uuid, uuid);

        // Disable the row if the device is selected as we can only
        // connect to one device to one source.
        // Disabled for now as I'm not sure how to sync status across
        // multiple instances of the plugin.
        //                      auto isConnected = deviceMap.second->isConnected();
        //                      obs_property_list_item_disable(dev_list, index, isConnected);

        index++;
    });

    return true;
}

static bool reconnect_to_device(obs_properties_t *props, obs_property_t *p, void *data)
{
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(p);

    auto cameraInput = reinterpret_cast<OverlayInput* >(data);
    cameraInput->reconnectToDevice();

    return false;
}

#pragma mark - Plugin Callbacks

static const char *GetIOSCameraInputName(void *)
{
    return obs_module_text("OBSOverlay.Title");
}

static void *CreateIOSCameraInput(obs_data_t *settings, obs_source_t *source)
{
    OverlayInput *cameraInput = nullptr;

    try
    {
        cameraInput = new OverlayInput(source, settings);
    }
    catch (const char *error)
    {
        blog(LOG_ERROR, "Could not create device '%s': %s", obs_source_get_name(source), error);
    }

    return cameraInput;
}

static void DestroyIOSCameraInput(void *data)
{
    delete reinterpret_cast<OverlayInput *>(data);
}

static void DeactivateIOSCameraInput(void *data)
{
    auto cameraInput = reinterpret_cast<OverlayInput*>(data);
    cameraInput->deactivate();
}

static void ActivateIOSCameraInput(void *data)
{
    auto cameraInput = reinterpret_cast<OverlayInput*>(data);
    cameraInput->activate();
}

static obs_properties_t *GetIOSCameraProperties(void *data)
{
    UNUSED_PARAMETER(data);
    obs_properties_t *ppts = obs_properties_create();

    obs_property_t *dev_list = obs_properties_add_list(ppts, SETTING_DEVICE_UUID,
                                                       obs_module_text("OBSOverlay.iOSDevice"),
                                                       OBS_COMBO_TYPE_LIST,
                                                       OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(dev_list, "", "");

    refresh_devices(ppts, dev_list, data);

    obs_properties_add_button(ppts, "setting_refresh_devices", obs_module_text("OBSOverlay.RefreshDevices"), refresh_devices);
    obs_properties_add_button(ppts, "setting_button_connect_to_device", obs_module_text("OBSOverlay.ReconnectToDevice"), reconnect_to_device);

    return ppts;
}

static void GetIOSCameraDefaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, SETTING_DEVICE_UUID, "");
}

static void SaveIOSCameraInput(void *data, obs_data_t *settings)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(settings);
}

static void UpdateIOSCameraInput(void *data, obs_data_t *settings)
{
    OverlayInput *input = reinterpret_cast<OverlayInput*>(data);

    // Connect to the device
    auto uuid = obs_data_get_string(settings, SETTING_DEVICE_UUID);
    input->connectToDevice(uuid, false);
}

static uint32_t GetWidthOverlayInput(void *data) {
    UNUSED_PARAMETER(data);
    
    return 1920;
}

static uint32_t GetHeightOverlayInput(void *data) {
    UNUSED_PARAMETER(data);

    return 1080;
}

static void TickOverlayInput(void *data, float seconds) {
    UNUSED_PARAMETER(seconds);
    
    OverlayInput *input = reinterpret_cast<OverlayInput*>(data);
    input->update();
}

static void RenderOverlayInput(void *data, gs_effect_t *effect) {
    UNUSED_PARAMETER(effect);

    OverlayInput *input = reinterpret_cast<OverlayInput*>(data);
    input->render();
}

void register_overlay_source(void) {
    obs_source_info info = {};
    info.id              = "overlay-source";
    info.type            = OBS_SOURCE_TYPE_INPUT;
    info.output_flags    = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
    info.get_name        = GetIOSCameraInputName;

    info.create          = CreateIOSCameraInput;
    info.destroy         = DestroyIOSCameraInput;

    info.deactivate      = DeactivateIOSCameraInput;
    info.activate        = ActivateIOSCameraInput;

    info.get_defaults    = GetIOSCameraDefaults;
    info.get_properties  = GetIOSCameraProperties;
    info.save            = SaveIOSCameraInput;
    info.update          = UpdateIOSCameraInput;

    info.get_width = GetWidthOverlayInput;
	info.get_height = GetHeightOverlayInput;
    info.video_tick = TickOverlayInput;
    info.video_render = RenderOverlayInput;

    info.icon_type       = OBS_ICON_TYPE_IMAGE;
    obs_register_source(&info);
}
