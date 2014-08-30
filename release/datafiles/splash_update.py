#!/usr/bin/env python3

# This script updates splashscreen from the SVG file
#
# Usage: ./splash_update.py 2.71 RC1
#
# If no version string is specified, it fallsback to the version provided by Blender
#
# (blender --version)

import os
import sys
import subprocess
import re

SVG_FILE = "splash_template.svg"
SPLASH_IMAGE = "splash_body.png"

BASEDIR = os.path.abspath(os.path.dirname(__file__)) + os.sep
SPLASH_TEMPLATE = "%s%s" % (BASEDIR, SVG_FILE)

inkscape_path = 'inkscape'

if sys.platform == 'darwin':
    inkscape_app_path = '/Applications/Inkscape.app/Contents/Resources/script'
    if os.path.exists(inkscape_app_path):
        inkscape_path = inkscape_app_path


def run(cmd, output=False):
    print("   ", cmd)
    if output:
        return subprocess.check_output(cmd, shell=True)
    else:
        os.system(cmd)


def get_blender_version():
    """get current Blender version and return it formatted"""

    if len(sys.argv) < 2:
        print("No version string specified, using version defined by Blender")

        cmd = (
            "blender "
            "--background -noaudio "
            "--version"
            )

        version_raw = run(cmd, output=True)
        pattern = b'Blender (.*) \(.*'
        match = re.search(pattern, version_raw)

        if not match or (len(match.groups()) != 1):
            raise Exception

        version = match.groups()[0].decode()
        print("Version: %s" % (version,))
        return version

    else:
        return " ".join(sys.argv[1:])


def update_splash_svg(version):
    """update id label and make sure image has relative path"""

    pattern_version = re.compile('(id="version".*>)(.*)(</flowPara>)', re.DOTALL)
    pattern_image = re.compile('".*{0}"'.format(SPLASH_IMAGE))

    svg_file = open(SPLASH_TEMPLATE, mode='rt')
    svn_buffer = svg_file.read()
    svg_file.close()

    svn_buffer = pattern_version.sub(r'\g<1>{0}\g<3>'.format(version), svn_buffer)
    svn_buffer = pattern_image.sub(r'"{0}"'.format(SPLASH_IMAGE), svn_buffer)

    svg_file = open(SPLASH_TEMPLATE, mode='wt')
    svg_file.write(svn_buffer)
    svg_file.close()


def update_splash_png():
    """update the splash images used by datatoc"""

    #export fullsized file
    cmd = inkscape_path + ' "%s" --without-gui --export-dpi=90 --export-png="%ssplash_2x.png"' % (SPLASH_TEMPLATE, BASEDIR)
    os.system(cmd)

    # export half-resolution file
    cmd = inkscape_path + ' "%s" --without-gui --export-dpi=45 --export-png="%ssplash.png"' % (SPLASH_TEMPLATE, BASEDIR)
    os.system(cmd)


def main():
    # get version from argument or open blender file and get current version
    blender_version = get_blender_version()

    # open inkscape file and update number
    update_splash_svg(blender_version)

    # call inkscape to update the splash.sh
    update_splash_png()


main()
