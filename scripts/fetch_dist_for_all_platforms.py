import os
import shutil
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--version', help='version to fetch')
parser.add_argument('--python_version', nargs='?', default=37, help='python version to fetch')
args = parser.parse_args()
VERSION = args.version
PYTHON_VERSION = str(args.python_version)
print("using version " + VERSION)
print("python version " + PYTHON_VERSION)

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
         "macosx_10_9_x86_64",
         "macosx_14_0_arm64",
         "manylinux_2_24_x86_64"
         ]

print("Download")
os.chdir(TMPDIR)
for plat in plats:
    print("Fetching " + plat)
    os.system("pip3 download --only-binary=:all: --no-deps --platform "+plat+" --python-version "+PYTHON_VERSION+" --implementation cp --abi cp"+PYTHON_VERSION+"m pygeodiff==" + VERSION )

print("Extract & Combine")
for plat in plats:
    platdir = "pygeodiff-" + VERSION + "-" + plat
    os.system("unzip pygeodiff-" + VERSION + "-cp"+PYTHON_VERSION+"-cp"+PYTHON_VERSION+"m-" + plat + ".whl -d " + platdir )
    if not os.path.exists(FINALDIR):
        os.mkdir(FINALDIR)
    os.system("cp "+platdir+"/pygeodiff/* "+FINALDIR+"/")

if ((not os.path.exists(FINALDIR)) or
    (not os.path.exists(FINALDIR + "/pygeodiff-" + VERSION + "-python.pyd")) or
    (not os.path.exists(FINALDIR + "/pygeodiff-" + VERSION + "-python-win32.pyd")) or
    (not os.path.exists(FINALDIR + "/libpygeodiff-" + VERSION + "-python.dylib")) or
    (not os.path.exists(FINALDIR + "/libpygeodiff-" + VERSION + "-python-arm64.dylib")) or
    (not os.path.exists(FINALDIR + "/libpygeodiff-" + VERSION + "-python.so"))
   ):
    print ("ERROR")
    exit(1)
else:
    print("Done")
