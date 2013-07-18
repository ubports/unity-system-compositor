#!/usr/bin/python

from distutils.core import setup
from setuptools import find_packages

setup(
    name='unity_system_compositor',
    version='1.0',
    description='unity-system-compositor autopilot tests.',
    author='Thomi Richards',
    author_email='thomi.richards@canonical.com',
    url='https://launchpad.net/unity-system-compositor',
    license='GPLv3',
    packages=find_packages(),
)
