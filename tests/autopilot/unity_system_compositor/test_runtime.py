# -*- Mode: Python; coding: utf-8; indent-tabs-mode: nil; tab-width: 4 -*-
#
# Unity System Compositor
# Copyright (C) 2013 Canonical
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

"""Runtime Environment Tests for unity-system-compositor."""

from autopilot.testcase import AutopilotTestCase
import logging
import re
from subprocess import check_output, CalledProcessError
from testtools.matchers import Equals


logger = logging.getLogger(__name__)


# this maps device manufacturers to their open source drivers.
supported_hardware = {
    'Intel': 'i915',
    'NVidia': 'nouveau',
    'Advanced Micro Devices': 'radeon',
}


class RuntimeEnvironmentTests(AutopilotTestCase):

    """Tests for the unity system compositor runtime environment."""

    def test_running_hardware_check(self):
        """Checks whether u-s-c is running if we're on supported hardware, and
        checks the inverse if we're not on supported hardware.

        """
        should_be_running = True

        devices = _get_video_devices()
        self.assertThat(len(devices), Equals(1))
        device = devices[0]

        try:
            manufacturer = _get_supported_device_manufacturer(device)
            logger.info("Video card manufacturer is supported.")
        except RuntimeError:
            logger.info("Video card manufacturer is not supported.")
            should_be_running = False

        driver = _get_kernel_driver_in_use(device)
        if _is_running_oss_driver(manufacturer, driver):
            logger.info("Running OSS driver (%s).", driver)
        else:
            logger.info("Running proprietary driver (%s).", driver)
            should_be_running = False


        self.assertThat(
            _is_system_compositor_running(),
            Equals(should_be_running)
        )


def _get_video_devices():
    lspci_output = check_output(["lspci", "-vvv"]).strip()
    devices = [s.strip() for s in re.split("\n\n", lspci_output, flags=re.M)]
    video_devices = [dev for dev in devices if _is_video_device(dev)]
    return video_devices


def _is_video_device(device_section):
    return 'VGA' in device_section.split('\n')[0]


def _get_supported_device_manufacturer(device_section):
    global supported_hardware
    dev_line = device_section.split('\n')[0]
    for manufacturer in supported_hardware:
        if manufacturer.lower() in dev_line.lower():
            return manufacturer
    raise RuntimeError("Unknown or unsupported device: " + dev_line)


def _get_kernel_driver_in_use(device_section):
    for line in reversed(device_section.split('\n')):
        k,v = line.strip().split(':')
        if k == 'Kernel driver in use':
            return v.strip()
    return "unlisted driver"


def _is_running_oss_driver(manufacturer, driver):
    global supported_hardware
    return supported_hardware.get(manufacturer, "") == driver


def _is_system_compositor_running():
    try:
        check_output(["pgrep", "unity-system"])
        return True
    except CalledProcessError:
        return False
