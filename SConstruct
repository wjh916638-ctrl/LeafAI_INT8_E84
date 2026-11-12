import os
import sys
import rtconfig
import platform
import subprocess
import uuid

SDK_BSP_ROOT = r'D:\RT-ThreadStudio\repo\Local\Board_Support_Packages\Infineon\PSOC_E84-EDGI-TALK\1.4.0'
RTT_ROOT = os.path.join(SDK_BSP_ROOT, 'rt-thread')

sys.path = sys.path + [os.path.join(RTT_ROOT, 'tools')]
try:
    from building import *
except Exception as e:
    print("Error message:", e)
    print('Cannot found RT-Thread root directory, please check RTT_ROOT')
    print(RTT_ROOT)
    sys.exit(-1)

TARGET = 'rt-thread.' + rtconfig.TARGET_EXT

DefaultEnvironment(tools=[])
env = Environment(tools = ['mingw'],
    AS = rtconfig.AS, ASFLAGS = rtconfig.AFLAGS,
    CC = rtconfig.CC, CFLAGS = rtconfig.CFLAGS,
    AR = rtconfig.AR, ARFLAGS = '-rc',
    CXX = rtconfig.CXX, CXXFLAGS = rtconfig.CXXFLAGS,
    LINK = rtconfig.LINK, LINKFLAGS = rtconfig.LFLAGS)
env.PrependENVPath('PATH', rtconfig.EXEC_PATH)

if rtconfig.PLATFORM in ['iccarm']:
    env.Replace(CCCOM = ['$CC $CFLAGS $CPPFLAGS $_CPPDEFFLAGS $_CPPINCFLAGS -o $TARGET $SOURCES'])
    env.Replace(ARFLAGS = [''])
    env.Replace(LINKCOM = env["LINKCOM"] + ' --map rt-thread.map')

Export('RTT_ROOT')
Export('rtconfig')

SDK_ROOT = SDK_BSP_ROOT
libraries_path_prefix = os.path.join(SDK_ROOT, 'libraries', 'components')

SDK_LIB = libraries_path_prefix
Export('SDK_LIB')

# prepare building environment
objs = PrepareBuilding(env, RTT_ROOT, has_libcpu=False)

# VariantDir copies implementation sources but not every SDK header. Keep the
# installed HAL include directory on the compiler path so board and application
# sources can resolve implementation headers such as mtb_hal_gpio_impl.h.
env.Append(CPPPATH=[
    os.path.join(
        libraries_path_prefix,
        'mtb-device-support-pse8xxgp',
        'hal',
        'include'),
])

IFX_library = 'Packages'
rtconfig.BSP_LIBRARY_TYPE = IFX_library

# set spawn
def ourspawn(sh, escape, cmd, args, e):
    filename = str(uuid.uuid4())
    newargs = ' '.join(args[1:])
    cmdline = cmd + " " + newargs
    if (len(cmdline) > 16 * 1024):
        f = open(filename, 'w')
        f.write(' '.join(args[1:]).replace('\\', '/'))
        f.close()
        # exec
        cmdline = cmd + " @" + filename
    proc = subprocess.Popen(cmdline, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, shell = False, env = e)
    data, err = proc.communicate()
    rv = proc.wait()
    def res_output(_output, _s):
        if len(_s):
            if isinstance(_s, str):
                _output(_s)
            elif isinstance(_s, bytes):
                _output(str(_s, 'UTF-8'))
            else:
                _output(str(_s))
    res_output(sys.stderr.write, err)
    res_output(sys.stdout.write, data)
    if os.path.isfile(filename):
        os.remove(filename)
    return rv

if platform.system() == 'Windows':
    env['SPAWN'] = ourspawn

# Add project libs with VariantDir
libs_root = os.path.join(SDK_BSP_ROOT, 'projects', 'libs')
env.VariantDir('build/libs', libs_root, duplicate=1)
objs.extend(SConscript(os.path.join(libs_root, 'SConscript'), variant_dir='build/libs', duplicate=1))

# Set VariantDir for external libraries
libraries_root = os.path.join(SDK_BSP_ROOT, 'libraries')
env.VariantDir('build/libraries/HAL_Drivers', os.path.join(libraries_root, 'HAL_Drivers'), duplicate=1)
env.VariantDir('build/libraries/components', os.path.join(libraries_root, 'components'), duplicate=1)
env.VariantDir('build/libraries/M55_Config/board', os.path.join(libraries_root, 'M55_Config', 'board'), duplicate=1)
env.VariantDir('build/libraries/Common/board', os.path.join(libraries_root, 'Common', 'board'), duplicate=1)

objs.extend(SConscript(os.path.join(libraries_root, 'HAL_Drivers', 'SConscript'), variant_dir='build/libraries/HAL_Drivers', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'Infineon_cmsis-latest', 'SConscript'), variant_dir='build/libraries/components/Infineon_cmsis-latest', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'Infineon_core-lib-latest', 'SConscript'), variant_dir='build/libraries/components/Infineon_core-lib-latest', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'mtb-device-support-pse8xxgp', 'hal', 'SConscript'), variant_dir='build/libraries/components/mtb-device-support-pse8xxgp/hal', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'mtb-device-support-pse8xxgp', 'pdl', 'SConscript'), variant_dir='build/libraries/components/mtb-device-support-pse8xxgp/pdl', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'async-transfer', 'SConscript'), variant_dir='build/libraries/components/async-transfer', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'mtb-device-support-pse8xxgp', 'device-utils', 'syspm', 'SConscript'), variant_dir='build/libraries/components/mtb-device-support-pse8xxgp/device-utils/syspm', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'serial-memory', 'SConscript'), variant_dir='build/libraries/components/serial-memory', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'Infineon_retarget-io-latest', 'SConscript'), variant_dir='build/libraries/components/Infineon_retarget-io-latest', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'ASRC', 'SConscript'), variant_dir='build/libraries/components/ASRC', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'mtb-srf', 'SConscript'), variant_dir='build/libraries/components/mtb-srf', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'mtb-ipc', 'SConscript'), variant_dir='build/libraries/components/mtb-ipc', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'littlefs', 'SConscript'), variant_dir='build/libraries/components/littlefs', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'components', 'SConscript'), variant_dir='build/libraries/components', duplicate=1))
objs.extend(SConscript(os.path.join(libraries_root, 'Common', 'board', 'SConscript'), variant_dir='build/libraries/Common/board', duplicate=1))
# The 1.4.0 LVGL bridge does not add its RT-Thread stdlib implementations to
# the recursive source scan. Compile them to project-local objects because the
# SDK installation directory is read-only.
lvgl_root = os.path.join(libraries_root, 'components', 'lvgl_9.2.0')
lvgl_port_root = os.path.join(Dir('#').abspath, 'applications', 'lvgl_port')
lvgl_src_root = os.path.join(lvgl_root, 'src')
lvgl_include_dirs = [
    lvgl_root,
    lvgl_src_root,
    os.path.join(lvgl_src_root, 'core'),
    os.path.join(lvgl_src_root, 'draw'),
    os.path.join(lvgl_src_root, 'draw', 'sw'),
    os.path.join(lvgl_src_root, 'draw', 'vg_lite'),
    os.path.join(lvgl_src_root, 'display'),
    os.path.join(lvgl_src_root, 'font'),
    os.path.join(lvgl_src_root, 'image'),
    os.path.join(lvgl_src_root, 'layouts'),
    os.path.join(lvgl_src_root, 'libs'),
    os.path.join(lvgl_src_root, 'libs', 'bin_decoder'),
    os.path.join(lvgl_src_root, 'misc'),
    os.path.join(lvgl_src_root, 'osal'),
    os.path.join(lvgl_src_root, 'others'),
    os.path.join(lvgl_src_root, 'stdlib'),
    os.path.join(lvgl_src_root, 'themes'),
    os.path.join(lvgl_src_root, 'tick'),
    os.path.join(lvgl_src_root, 'widgets'),
]
env.Append(CPPPATH=lvgl_include_dirs)

lvgl_core_cpppath = ([lvgl_port_root] +
                     list(env.get('CPPPATH', [])))
lvgl_core_cppdefines = list(env.get('CPPDEFINES', [])) + ['__RTTHREAD__']
lvgl_core_cflags = env['CFLAGS'] + ' -std=c99'
for walk_root, walk_dirs, walk_files in os.walk(os.path.join(lvgl_root, 'src')):
    for walk_file in walk_files:
        if not walk_file.lower().endswith('.c'):
            continue
        source = os.path.join(walk_root, walk_file)
        relative_source = os.path.relpath(source, lvgl_root)
        target = os.path.join(
            'build',
            'lvgl_core',
            relative_source.replace('.c', env['OBJSUFFIX']))
        lvgl_core_objs = env.Object(
            target,
            source,
            CPPPATH=lvgl_core_cpppath,
            CPPDEFINES=lvgl_core_cppdefines,
            CFLAGS=lvgl_core_cflags)
        objs.extend(lvgl_core_objs)

# The 1.4.0 generic board script adds the display-panel include path but can
# omit its two implementation files from the project build. Compile them
# explicitly so the LCD and ST7102 touch controller resolve at link time.
display_panel_root = os.path.join(
    libraries_root, 'Common', 'board', 'ports', 'display_panels')
display_panel_cpppath = list(env.get('CPPPATH', [])) + [
    display_panel_root,
    os.path.join(libraries_root, 'Common', 'board'),
    os.path.join(libraries_root, 'Common', 'board', 'ports'),
]
for display_panel_source in ['display_tl043wvv02.c', 'drv_touch.c']:
    source = os.path.join(display_panel_root, display_panel_source)
    target = os.path.join(
        'build',
        'board_display',
        display_panel_source.replace('.c', env['OBJSUFFIX']))
    display_panel_objs = env.Object(
        target,
        source,
        CPPPATH=display_panel_cpppath)
    objs.extend(display_panel_objs)

# Some BSP component SConscripts add the same SDK object through both a normal
# source scan and a group dependency. Keep the first occurrence so the linker
# does not receive duplicate startup/config objects.
def flatten_build_items(items):
    flat_items = []
    for item in items:
        if isinstance(item, (list, tuple)):
            flat_items.extend(flatten_build_items(item))
        else:
            flat_items.append(item)
    return flat_items

objs = flatten_build_items(objs)
unique_objs = []
seen_objs = set()
for obj in objs:
    key = os.path.normcase(os.path.normpath(os.path.abspath(str(obj))))
    if key not in seen_objs:
        unique_objs.append(obj)
        seen_objs.add(key)
objs = unique_objs

if os.environ.get('LEAF_DEBUG_OBJECTS'):
    print('LeafAI build objects:', len(objs))
    for obj in objs:
        normalized = str(obj).replace('\\', '/')
        if '/lvgl_9.2.0/src/stdlib/' in normalized:
            print('LVGL stdlib object:', normalized)

# make a building
DoBuilding(TARGET, objs)
