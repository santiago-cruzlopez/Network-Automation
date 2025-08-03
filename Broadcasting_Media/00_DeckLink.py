#Test comment

# Python Script to check the DeckLink Duo Video and Audio Capabilities 

import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib

Gst.init(None)
print("GStreamer initialized.")

# List All DeckLink Devices

def list_decklink_devices():
    device_monitor = Gst.DeviceMonitor()
    device_monitor.add_filter("Video/Source", None)
    device_monitor.start()
    devices = device_monitor.get_devices()
    decklink_devices = []
    for dev in devices:
        if 'decklink' in dev.get_display_name().lower():
            decklink_devices.append(dev)
    device_monitor.stop()
    return decklink_devices

decklink_devices = list_decklink_devices()

if decklink_devices:
    for idx, dev in enumerate(decklink_devices):
        print(f"Device {idx}: {dev.get_display_name()}")
else:
    print("No DeckLink devices found.")

# Video Modes and Pixel Formats

def print_device_caps(device):
    print(f"\nCapabilities for device: {device.get_display_name()}")
    vid_caps = device.get_caps()
    for i in range(vid_caps.get_size()):
        structure = vid_caps.get_structure(i)
        print(f"  {structure.to_string()}")

if decklink_devices:
    print_device_caps(decklink_devices[0])
else:
    print("No DeckLink device to query.")

# Audio Capture Settings

def list_decklink_audio_caps():
    # DeviceMonitor for audio sources
    device_monitor = Gst.DeviceMonitor()
    device_monitor.add_filter("Audio/Source", None)
    device_monitor.start()
    devices = device_monitor.get_devices()
    for dev in devices:
        if 'decklink' in dev.get_display_name().lower():
            print(f"\nAudio Capabilities for: {dev.get_display_name()}")
            caps = dev.get_caps()
            for i in range(caps.get_size()):
                structure = caps.get_structure(i)
                print(f"  {structure.to_string()}")
    device_monitor.stop()

list_decklink_audio_caps()