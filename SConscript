# for module compiling
import os
Import('RTT_ROOT')
from building import *

cwd = GetCurrentDir()
objs = []
list = os.listdir(cwd)

for d in list:
    # These SDK trees are linked into the project for IDE indexing. Their
    # sources are added exactly once by SConstruct from the installed 1.4.0 BSP.
    if d in ['libs', 'libraries', 'rt-thread', 'tools']:
        continue

    path = os.path.join(cwd, d)
    if os.path.isfile(os.path.join(path, 'SConscript')):
        objs = objs + SConscript(os.path.join(d, 'SConscript'))

Return('objs')
