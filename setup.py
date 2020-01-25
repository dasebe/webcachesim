import os
import re
import sys
import platform
import subprocess

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from distutils.version import LooseVersion


setup(
    name='pywebcachesim',
    version='0.0.1',
    author='Zhenyu Song',
    author_email='sunnyszy@gmail.com',
    description='Cache Simulator',
    long_description='',
    install_requires=[
        'pyyaml',
        'pygit2',
        'numpy',
        'pandas',
        # 'arrow',
        # 'seaborn',
        # 'matplotlib',
        # 'tqdm',
    ],
    zip_safe=False,
    packages=['pywebcachesim'],
    # entry_points = {
    #     'console_scripts': ['pywebcachesim=pywebcachesim.runner:main'],
    # },
)
