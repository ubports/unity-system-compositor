#!/usr/bin/python3

import os
from apport.hookutils import *

def attach_graphic_card_pci_info(report, ui=None):
    '''
    Extracts the device system and subsystem IDs for the video card.
    Note that the user could have multiple video cards installed, so
    this may return a multi-line string.
    '''
    info = ''
    display_pci = pci_devices(PCI_DISPLAY)
    for paragraph in display_pci.split('\n\n'):
        for line in paragraph.split('\n'):
            if ':' not in line:
                continue
            m = re.match(r'(.*?):\s(.*)', line)
            if not m:
                continue
            key, value = m.group(1), m.group(2)
            value = value.strip()
            key = key.strip()
            if "VGA compatible controller" in key:
                info += "%s\n" % (value)
            elif key == "Subsystem":
                info += "  %s: %s\n" %(key, value)
    report['GraphicsCard'] = info

def add_info(report, ui=None):
    attach_file_if_exists(report, '/var/log/boot.log', 'BootLog')

    display_manager_files = {}
    if os.path.lexists('/var/log/lightdm'):
        display_manager_files['LightDMLog'] = \
                'test -f /var/log/lightdm/lightdm.log && cat /var/log/lightdm/lightdm.log'
        display_manager_files['LightDMLogOld'] = \
                'test -f /var/log/lightdm/lightdm.log.1.gz && zcat /var/log/lightdm/lightdm.log.1.gz'
        display_manager_files['USCLog'] = \
                'test -f /var/log/lightdm/unity-system-compositor.log && cat /var/log/lightdm/unity-system-compositor.log'
        display_manager_files['USCLogOld'] = \
                'test -f /var/log/lightdm/unity-system-compositor.log.1.gz && zcat /var/log/lightdm/unity-system-compositor.log.1.gz'
    attach_root_command_outputs(report, display_manager_files)

    report['version.libdrm'] = package_versions('libdrm2')
    report['version.lightdm'] = package_versions('lightdm')
    report['version.mesa'] = package_versions('libegl1-mesa-dev')
    attach_graphic_card_pci_info(report, ui)

    return True

if __name__ == '__main__':
    import sys

    report = {}
    if not add_info(report, None):
        print("Unreportable")
        sys.exit(1)
    keys = sorted(report.keys())
    for key in keys:
        print("[%s]\n%s\n" %(key, report[key]))
