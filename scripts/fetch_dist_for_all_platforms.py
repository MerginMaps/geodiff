import os
import shutil
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--version', help='version to fetch')
args = parser.parse_args()
VERSION = args.version
print("using version " + VERSION)

THIS_DIR = os.path.dirname(os.path.realpath(__file__))
RESULT_DIR = os.path.join(THIS_DIR, os.pardir, "build-platforms")
TMPDIR = os.path.join(RESULT_DIR, "tmp")
FINALDIR = os.path.join(RESULT_DIR, "pygeodiff")
PWD = os.curdir

if os.path.exists(RESULT_DIR):
    shutil.rmtree(RESULT_DIR)
os.makedirs(RESULT_DIR)
os.makedirs(TMPDIR)

source = "pygeodiff-" + VERSION + ".tar.gz"
plats = ["win32",
         "win_amd64",
         "macosx_10_14_x86_64",
         "manylinux2010_x86_64"
         ]

print("Download")
os.chdir(TMPDIR)
for plat in plats:
    print("Fetching " + plat)
    os.system("pip3 download --only-binary=:all: --no-deps --platform "+plat+" --python-version 36 --implementation cp --abi cp36m pygeodiff==" + VERSION )

print("Extract & Combine")
for plat in plats:
    platdir = "pygeodiff-" + VERSION + "-" + plat
    os.system("unzip pygeodiff-0.7.0-cp36-cp36m-" + plat + ".whl -d " + platdir )
    if not os.path.exists(FINALDIR):
        os.system("cp -R " + platdir + "/pygeodiff " + FINALDIR)
    os.system("cp "+platdir+"/pygeodiff/*.{so,dll,dylib} "+FINALDIR+"/")

if ((not os.path.exists(FINALDIR)) or
    (not os.path.exists(FINALDIR + "/pygeodiff-" + VERSION + "-python.dll")) or
    (not os.path.exists(FINALDIR + "/pygeodiff-" + VERSION + "-python-win32.dll")) or
    (not os.path.exists(FINALDIR + "/libpygeodiff-" + VERSION + "-python.dylib")) or
    (not os.path.exists(FINALDIR + "/libpygeodiff-" + VERSION + "-python.so"))
   ):
    print ("ERROR")
    exit(1)
else:
    print("Done")
