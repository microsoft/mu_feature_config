# @ VfrToXmlConverter.py
#
# Copyright (c) 2025, Microsoft Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#

import os
import sys
import shutil
import stat
import xml.etree.ElementTree as ET
from xml.dom import minidom
import re
import subprocess
import time
import uuid
import chardet
import importlib.util
import logging
import tkinter
import tkinter.messagebox as messagebox
import tkinter.filedialog as filedialog
import argparse

from edk2toollib.uefi.edk2.parsers.buildreport_parser import BuildReport
from edk2toollib.uefi.edk2.parsers.dec_parser import DecParser
from edk2toollib.uefi.edk2.parsers.dsc_parser import DscParser
from edk2toollib.uefi.edk2.parsers.inf_parser import InfParser
from edk2toollib.uefi.edk2.path_utilities import Edk2Path
from edk2toolext.environment import plugin_manager, shell_environment
from edk2toolext.environment.multiple_workspace import MultipleWorkspace
from edk2toolext.environment.plugintypes.uefi_helper_plugin import HelperFunctions
Edk2Path_logger = logging.getLogger("Edk2Path")
Edk2Path_logger.setLevel(logging.ERROR)
this_version = '0.2'

################################ Configuration ################################
srcdir = ''
workdir = os.path.dirname(os.path.abspath(__file__))  # Directory of this python script
sys.path.append(workdir)

# (Optional) For preprocessing the FixedPcdGet portions in vfr files
input_build_report = ''

# PlatformBuild.py path
input_platform_build_py = ''
input_platform_build_py = os.path.join('Platform', 'PlatformPkg', 'PlatformBuild.py')  # Default path

# Input file dictionary
# {INF file path to be processed : Corresponding vfr_resp file path}
input_file_name_dict = {
}

input_ref_xml = ''
output_file_path = os.path.join(workdir, 'output.xml')
export_file_path = os.path.join(workdir, 'output_export.xml')
debug_file_path = os.path.join(workdir, 'output_debug_log.txt')
clean_up_list = []
patch_numeric_default_for_debug = 1
################################ Configuration ################################


# Parse PlatformBuild.py and setup platform_builder for later use
# platform_build_py_path: PlatformBuild.py file path
# Return:
#   platform_builder: PlatformBuilder object
def setup_platform_builder(platform_build_py_path):
    platform_builder = None
    if os.path.isfile(platform_build_py_path):
        # Attempt to import PlatformBuild.py from the given path
        print('\nImporting PlatformBuild: %s' % platform_build_py_path)
        try:
            module_name = 'PlatformBuild'
            spec = importlib.util.spec_from_file_location(module_name, platform_build_py_path)
            PlatformBuild = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(PlatformBuild)
            platform_builder = PlatformBuild.PlatformBuilder()
        except Exception as e:
            print(e)
            print('  [PlatformBuild] Failed to import: %s' % platform_build_py_path)
            return None

        # Invoke SetPlatformEnv() and PlatformPreBuild() to setup the environment variables
        try:
            pathobj = Edk2Path(
                platform_builder.GetWorkspaceRoot(),
                platform_builder.GetPackagesPath(),
                error_on_invalid_pp=False
            )
            platform_builder.env = shell_environment.GetBuildVars()
            platform_builder.mws = MultipleWorkspace()
            platform_builder.mws.setWs(pathobj.WorkspacePath, os.pathsep.join(pathobj.PackagePathList))
            platform_builder.edk2path = Edk2Path(pathobj.WorkspacePath, pathobj.PackagePathList)
            platform_builder.ws = pathobj.WorkspacePath
            platform_builder.pp = os.pathsep.join(pathobj.PackagePathList)
            platform_builder.Helper = HelperFunctions()
            platform_builder.pm = plugin_manager.PluginManager()
        except Exception:
            # The mandatory info is platform_builder.GetPackagesPath(), others are optional/additional for most cases
            # print(e)
            pass

        try:
            print('  [PlatformBuild] Invoke SetPlatformEnv()...')
            platform_builder.SetPlatformEnv()
        except Exception:
            # print(e)
            pass
        try:
            print('  [PlatformBuild] Invoke PlatformPreBuild()...')
            platform_builder.PlatformPreBuild()
        except Exception:
            # print(e)
            pass

    else:
        print('PlatformBuild.py File not found: %s' % platform_build_py_path)
    return platform_builder


# Path Configuration >>>
# We do not have srcdir from gPlatformBuilder.GetWorkspaceRoot() yet,
#   try to assign a temporary srcdir until PlatformBuild.py is loaded
# Try various combinations of candidate paths, in most cases we could locate srcdir and ConfigEditor here
candidate_paths = [
    os.path.join('MU_BASECORE', 'BaseTools', 'Source', 'Python'),
    os.path.join('Common', 'mu_feature_config', 'SetupDataPkg', 'Tools')
]

for cand_dir in [os.getcwd(), workdir]:
    can_dirs = [cand_dir]
    if 'SetupDataPkg' in cand_dir:
        temp_path = os.path.dirname(os.path.dirname(os.path.normpath(cand_dir[:cand_dir.find('SetupDataPkg')])))
        if os.path.isdir(temp_path):
            can_dirs.append(temp_path)
    if 'BaseTools' in cand_dir:
        temp_path = os.path.dirname(os.path.normpath(cand_dir[:cand_dir.find('BaseTools')]))
        if os.path.isdir(temp_path):
            can_dirs.append(temp_path)
    for cdir in can_dirs:
        for cand_path in candidate_paths:
            cand_dir_path = os.path.join(cdir, cand_path)
            if os.path.isdir(cand_dir_path):
                srcdir = cdir
                sys.path.append(cand_dir_path)

# Try to locate the ConfigEditor module
default_configeditor_path = ''
default_basetools_python_path = ''
try:
    import ConfigEditor
except Exception:
    # If the ConfigEditor module is not found, ask for PlatformBuild.py first
    messagebox.showinfo("Information", "Please provide a path to PlatformBuild.py")
    input_platform_build_py = os.path.normpath(filedialog.askopenfilename(
        filetypes=(
            ("PlatformBuild", "PlatformBuild*.py"),
            ("all files", "*.*")
        )
    ))
    if input_platform_build_py and os.path.isfile(input_platform_build_py):
        print('PlatformBuild File: %s' % input_platform_build_py)
        gPlatformBuilder = setup_platform_builder(input_platform_build_py)
        if gPlatformBuilder is not None:
            srcdir = gPlatformBuilder.GetWorkspaceRoot()
            for package_path in gPlatformBuilder.GetPackagesPath():
                if os.path.isdir(os.path.join(srcdir, package_path, 'SetupDataPkg', 'Tools')):
                    default_configeditor_path = os.path.normpath(os.path.join(
                        srcdir,
                        package_path,
                        'SetupDataPkg',
                        'Tools'
                    ))
                    sys.path.append(default_configeditor_path)
                if os.path.isdir(os.path.join(srcdir, package_path, 'BaseTools', 'Source', 'Python')):
                    default_basetools_python_path = os.path.normpath(os.path.join(
                        srcdir,
                        package_path,
                        'BaseTools',
                        'Source',
                        'Python'
                    ))
                    sys.path.append(default_basetools_python_path)
    try:
        import ConfigEditor
    except Exception:
        pass

if 'ConfigEditor' not in sys.modules:
    print('Cannot locate ConfigEditor nor PlatformBuild.py, exiting...')
    print(f'  srcdir={srcdir}')
    print(f'  workdir={workdir}')
    exit(1)

# Default path setup for cl
default_VsDevCmd_path = r'C:\BuildTools\Common7\Tools\VsDevCmd.bat'

# Try to validate srcdir a little bit
path_ready = False
if default_basetools_python_path and os.path.isdir(default_basetools_python_path):
    path_ready = True
else:
    for cand_path in candidate_paths:
        if os.path.isfile(os.path.join(srcdir, cand_path, 'Trim', 'Trim.py')):
            default_basetools_python_path = os.path.join(srcdir, cand_path)
            path_ready = True

platform_build_py_required = 0
if not path_ready:
    platform_build_py_required = 1

print(f'path_ready={path_ready}')
print(f'  srcdir={srcdir}')
print(f'  workdir={workdir}')
# Path Configuration <<<


# This is copied from shutil.rmtree example https://docs.python.org/3.12/library/shutil.html
def remove_readonly(func, path, _):
    "Clear the readonly bit and reattempt the removal"
    os.chmod(path, stat.S_IWRITE)
    func(path)


# Clean up an item in current working directory with given folder/file/link name
def clean_up_workspace(item_name, verbose=0):
    if os.path.isfile(item_name):
        os.remove(item_name)
        if verbose:
            print('File \'%s\' has been removed' % item_name)
    if os.path.isdir(item_name):
        shutil.rmtree(item_name, onexc=remove_readonly)
        if verbose:
            print('Folder \'%s\' has been removed' % item_name)
    if os.path.islink(item_name):
        os.unlink(item_name)
        if verbose:
            print('Link \'%s\' has been removed' % item_name)


# Normalize a name to a valid C identifier, which complies with is_valid_name() in VariableList.py
def normalize_name(name):
    # Replace non-alphanumeric characters with underscores
    name = re.sub(r'[^0-9a-zA-Z_]', '_', name)

    # If the name starts with a digit, prepend '_' to make it valid
    if re.match(r'^\d', name):
        name = '_' + name

    # Ensure the final name matches the C identifier format
    matches = re.findall("^[a-zA-Z_][0-9a-zA-Z_]*$", name)
    if not len(matches) == 1:
        raise ValueError(f"Invalid normalized name: \"{name}\"")

    return name


# Remove comments from the given vfr content
# vfr_content: Content read and processed from a vfr file
# dump_file_name (Optional): If a dump file name is given, write the trimmed vfr content to the file
# Return: Trimmed vfr content
def vfr_remove_comments(vfr_content, dump_file_name=None):
    # Remove line comments
    vfr_content = re.sub(r'//.*', '', vfr_content)

    # Remove /* */ blocks
    # Define the pattern to match all /* */ block comments, even if there are multiple per line
    block_comment_pattern = re.compile(r'/\*.*?\*/', re.DOTALL)
    # Split lines to process one by one
    lines = vfr_content.splitlines()
    new_vfr_content = []
    stack = []

    for line in lines:
        # Remove block comments within the line first
        line = re.sub(block_comment_pattern, '', line)

        # Handle unmatched /* and */ using a stack
        while '/*' in line or '*/' in line:
            start_idx = line.find('/*')
            end_idx = line.find('*/')

            # If only /* found and no */, push to stack and cut line after /*
            if start_idx != -1 and (end_idx == -1 or start_idx < end_idx):
                stack.append('/*')
                line = line[:start_idx]
            # If only */ found and there's a matching /* in stack, pop from stack and cut line before */
            elif end_idx != -1 and (start_idx == -1 or end_idx < start_idx):
                if stack:
                    stack.pop()
                line = line[end_idx + 2:]
            else:
                # No match to process further
                break

        # If stack is empty, keep the line; otherwise, skip it as it's inside a block comment
        if not stack:
            new_vfr_content.append(line)

    # Reconstruct the processed vfr content
    vfr_content = '\n'.join(new_vfr_content)

    # Write to a dump file if provided
    if dump_file_name:
        with open(dump_file_name, 'w') as f:
            f.write(vfr_content)

    return vfr_content


# Remove redundant newlines in the given vfr content
# vfr_content: Content read and processed from a vfr file
# dump_file_name (Optional): If a dump file name is given, write the trimmed vfr content to the file
# Return: Trimmed vfr content
def vfr_remove_redundant_newlines(vfr_content, dump_file_name=None):
    # Strip all lines to remove trailing spaces and detect true empty lines
    lines = [line.rstrip() for line in vfr_content.splitlines()]
    vfr_content = '\n'.join(line for line in lines)

    # Replace 4 or more consecutive newlines with exactly 3 newlines
    vfr_content = re.sub(r'\n{4,}', '\n\n\n', vfr_content)

    # Write to a dump file if provided
    if dump_file_name:
        with open(dump_file_name, 'w') as f:
            f.write(vfr_content)

    return vfr_content


# Process given INF file to retrieve misc information
# inf_file_path: Path to the INF file to be processed
# platform_builder: PlatformBuilder object setup by PlatformBuild.py
# Return:
#   vfr_file_list: A list of vfr files
#   uni_file_list: A list of uni files
#   dec_file_list: A list of dec files
#   search_path_list: A list of include paths
#   base_name: Base name of the module
#   FixedPcd_dict: A dictionary of PCDs
def inf_get_info(inf_file_path, platform_builder):
    vfr_file_list = []
    uni_file_list = []
    dec_file_list = []
    search_path_list = []
    base_name = ''
    FixedPcd_dict = {}

    if os.path.isfile(inf_file_path):
        # Add the directory of the INF file to the search path
        inf_dir = os.path.normpath(os.path.dirname(inf_file_path))
        if inf_dir not in search_path_list:
            search_path_list.append(inf_dir)

        # Collect platform build environment variables
        platform_env_status = 0  # 0 - Invalid; 1 - Valid
        try:
            platform_env_dict = {}
            pathobj = Edk2Path(
                platform_builder.GetWorkspaceRoot(),
                platform_builder.GetPackagesPath(),
                error_on_invalid_pp=False
            )
            platform_env_dict.update(
                platform_builder.env.GetAllNonBuildKeyValues()
            )
            platform_env_dict.update(
                platform_builder.env.GetAllBuildKeyValues(BuildType=platform_env_dict.get('TARGET', '*'))
            )
            platform_env_status = 1
        except Exception:
            # print(e)
            platform_env_status = 0

        # Parse Platform DSC
        dsc_file_path = platform_env_dict.get('ACTIVE_PLATFORM')
        dsc_parser = None
        try:
            print('\nParsing file: %s' % dsc_file_path)
            dsc_parser = DscParser()
            if platform_env_status:
                dsc_parser.SetEdk2Path(pathobj)
                dsc_parser.SetInputVars(platform_env_dict)
            dsc_parser.SetNoFailMode()
            dsc_parser.Logger.setLevel(logging.FATAL)  # Ignore error messages
            dsc_parser.ParseFile(dsc_file_path)
        except FileNotFoundError:
            print('DSC File not found: %s' % dsc_file_path)
        except Exception as e:
            print(e)
            print('Exception occurred while parsing DSC file: %s' % dsc_file_path)

        # Parse INF
        print('\nParsing file: %s' % inf_file_path)
        inf_parser = InfParser()
        if platform_env_status:
            inf_parser.SetEdk2Path(pathobj)
            inf_parser.SetInputVars(platform_env_dict)
        inf_parser.ParseFile(inf_file_path)
        base_name = inf_parser.Dict.get('BASE_NAME', '')
        print('  [inf] BASE_NAME: %s' % base_name)

        if not platform_env_status:
            print(
                '  [inf] Exception occurred which might be related to invalid PlatformBuild.py and paths. '
                'Skip the DscParser and DecParser'
            )
        else:
            # Parse DEC files, collect search_path_list and pcd_fullname_dict
            pcd_fullname_dict = {}
            for inf_package in inf_parser.PackagesUsed:
                for common_pkg_path in platform_builder.GetPackagesPath():
                    # Try to find the dec file path
                    dec_file_path = os.path.normpath(os.path.join(common_pkg_path, inf_package))
                    if os.path.isfile(dec_file_path):
                        # print('  [dec] %s' % dec_file_path)  # Print the file path of all dec files
                        if dec_file_path not in dec_file_list:
                            dec_file_list.append(dec_file_path)
                        dec_dir = os.path.dirname(dec_file_path)
                        if dec_dir not in search_path_list:
                            search_path_list.append(dec_dir)
                        dec_parser = DecParser()
                        dec_parser.SetEdk2Path(pathobj)
                        dec_parser.SetInputVars(platform_env_dict)
                        dec_parser.ParseFile(dec_file_path)
                        for dec_include in dec_parser.IncludePaths:
                            include_path = os.path.normpath(os.path.join(dec_dir, dec_include))
                            if os.path.isdir(include_path) and include_path not in search_path_list:
                                search_path_list.append(include_path)
                        for pcd_entry in dec_parser.Pcds:
                            pcd_fullname_dict.update(
                                {f'{pcd_entry.token_space_name}.{pcd_entry.name}': pcd_entry.default_value}
                            )

            # Collect PCD dict
            if dsc_parser is not None:
                pcd_fullname_dict.update(dsc_parser.PcdValueDict)  # Platform DSC overrides DEC
            conversion_map = {'TRUE': '1', 'FALSE': '0'}
            for pcd in inf_parser.PcdsUsed:
                pcd_name = pcd.split('.')[-1]  # Remove token_space_name
                pcd_value = pcd_fullname_dict.get(pcd)
                if pcd_value:
                    pcd_value = conversion_map.get(pcd_value.upper(), pcd_value)
                    FixedPcd_dict.update({pcd_name: pcd_value})

            # Print the include paths
            print('  [inf] Include Paths:')
            for search_path in search_path_list:
                print(f'        {search_path}')

        # Routine to get a list of real file paths
        def get_file_list_from_inf(inf_parser, file_ext, verify_path=True):
            file_list = []
            for source_file in inf_parser.Sources:
                if os.path.splitext(source_file)[1].lower() == file_ext:
                    file_path = os.path.normpath(os.path.join(inf_dir, source_file))
                    if verify_path and not os.path.isfile(file_path):
                        print('        [NOT FOUND] %s' % file_path)
                        continue
                    else:
                        if file_path not in file_list:
                            file_list.append(file_path)
                        print(f'        {file_path}')
            return file_list

        # Get vfr_file_list form the INF file
        print('  [vfr] vfr file(s):')
        vfr_file_list = get_file_list_from_inf(inf_parser, '.vfr')

        # Get the list of uni files from the INF file
        print('  [uni] uni file(s):')
        uni_file_list = get_file_list_from_inf(inf_parser, '.uni')

    else:
        print('INF File not found: %s' % inf_file_path)

    return vfr_file_list, uni_file_list, dec_file_list, search_path_list, base_name, FixedPcd_dict


# Process includes in the given VFR content by recursively including referenced files
# vfr_content: Content of the VFR file to be processed
# vfr_dir: Directory of the source vfr file
# Return: VFR content with all includes processed
def vfr_process_includes(vfr_content, vfr_dir):
    # Regular expression pattern to match #include statements (vfr or hfr files only)
    include_pattern = re.compile(r'#include\s*[<"](.+?\.(vfr|hfr))[">]', re.IGNORECASE)

    # Find all #include statements
    includes = include_pattern.findall(vfr_content)

    # Define search paths: current dir, parent dir, and pkg dir
    search_paths = []
    search_paths.append(vfr_dir)  # current directory
    search_paths.append(os.path.abspath(os.path.join(vfr_dir, os.pardir)))  # parent directory

    # Search for pkg directory in the path
    pkg_dir = None
    path_parts = vfr_dir.split(os.sep)
    if os.path.splitdrive(path_parts[0])[1] == '':
        path_parts[0] = path_parts[0] + os.sep  # Patch the separator for Drive
    for i, part in enumerate(path_parts):
        if 'Pkg' in part:
            pkg_dir = os.path.join(*path_parts[:i+1])
            break
    if pkg_dir:
        # Recursively search pkg directory and subdirectories
        for root, _, _ in os.walk(pkg_dir):
            search_paths.append(root)

    # Process each include statement
    for include_file, _ in includes:
        include_content = None

        # Search for the include file in the defined search paths
        for path in search_paths:
            include_file_path = os.path.normpath(os.path.join(path, include_file))
            if os.path.isfile(include_file_path):
                try:
                    # Print the found include file path
                    print('  [vfr] %s' % include_file_path)

                    with open(include_file_path, 'r') as file:
                        include_content = file.read()

                    # Recursively process includes in the included file, using its directory as the new vfr_dir
                    include_dir = os.path.dirname(include_file_path)
                    include_content = vfr_process_includes(include_content, include_dir)
                    break  # Stop once we find and load the include file
                except Exception as e:
                    print(f'  [vfr] Error reading included file: {include_file_path}')
                    print(f'        {e}')

        # If we found and loaded the include file, replace the #include line with the actual content
        if include_content:
            vfr_content = vfr_content.replace(f'#include "{include_file}"', include_content)
        else:
            print(f'  [vfr] Warning: Include file "{include_file}" not found.')

    return vfr_content


# Replace FixedPcdGet* wordings in vfr_content with corresponding PCD value in FixedPcd_dict
# vfr_content: Content of the VFR file to be processed
# FixedPcd_dict: PCD value dictionary {PCD Name: PCD Value}
# dump_file_name (Optional): If a dump file name is given, write the trimmed vfr content to the file
def vfr_process_pcds(vfr_content, FixedPcd_dict, dump_file_name=None):
    # Regular expression pattern to match FixedPcdGet* calls
    pcd_pattern = re.compile(r'FixedPcdGet(?:8|16|32|64|Bool)\s*\(\s*(\w+)\s*\)')

    # Function to perform the replacement using the FixedPcd_dict
    def replace_pcd(match):
        pcd_name = match.group(1)
        # Replace the matched pattern with the corresponding value from FixedPcd_dict
        return FixedPcd_dict.get(f'{pcd_name}')

    # Replace all matches in the content
    vfr_content = pcd_pattern.sub(replace_pcd, vfr_content)

    # Write to a dump file if provided
    if dump_file_name:
        with open(dump_file_name, 'w') as f:
            f.write(vfr_content)

    return vfr_content


# Update os.environ with the environment variables from VsDevCmd.bat
# VsDevCmd_path (Optional): Specify the path to VsDevCmd.bat if necessary
# verbose: 0 - Not to print; 1 - Print Error; 2 - Print Error and Info
# Return: 0 if successful, non-zero otherwise
def update_env_by_VsDevCmd(VsDevCmd_path=None, verbose=0):
    if VsDevCmd_path is None:
        VsDevCmd_path = default_VsDevCmd_path

    if os.path.isfile(VsDevCmd_path):
        # Execute VsDevCmd.bat and capture the environment variables
        try:
            print('  [cl] Info: Loading environment variables from VsDevCmd.bat...')
            completed = subprocess.run(
                f'cmd /c "{VsDevCmd_path} && set"',
                capture_output=True,
                text=True,
                shell=True,
            )

            if completed.returncode != 0:
                if verbose:
                    print(f'  [cl] Error: Failed to run {VsDevCmd_path}: {completed.stderr}')
                return completed.returncode

            # Parse the environment variables from the output
            env_vars = {}
            for line in completed.stdout.splitlines():
                if '=' in line:
                    key, value = line.split('=', 1)
                    env_vars[key.strip()] = value.strip()

            # Update os.environ with the new variables
            os.environ.update(env_vars)
            if verbose > 1:
                print('  [cl] Info: VsDevCmd environment loaded successfully.')
            return 0

        except Exception as e:
            if verbose:
                print(f'  [cl] Exception: {e}')
            return 1

    else:
        if verbose:
            print('  [cl] Error: VsDevCmd.bat not found.')
        return 1


# Invoke cl.exe to preprocess the given file
# input_file_name: vfr file name to be processed
# include_path_list: A list of include paths
# defines_list (Optional): A list of defines
# add_arg (Optional): Additional attributes to be added
# resp_file (Optional): Response file for cl
# vfr_mode (Optional): 0 - normal; 1 - vfr
# dump_file_name (Optional): If a dump file name is given, write the preprocessed vfr content to the file,
#   else dump to stdout
# VsDevCmd_path (Optional): Specify the path to VsDevCmd.bat if necessary
# Return: 0 if successful, non-zero otherwise
def run_cl(
    input_file_name, include_path_list, defines_list=[], add_arg='', resp_file=None,
    vfr_mode=0, dump_file_name=None, VsDevCmd_path=None
):
    # Verify cl.exe is in PATH
    verify_cmd = 'where cl >nul 2>&1'
    ret = os.system(verify_cmd)

    # Run VsDevCmd.bat to set path for cl.exe if necessary
    if ret != 0:
        update_env_by_VsDevCmd(VsDevCmd_path, verbose=2)

    # Execute cl.exe
    ret = os.system(verify_cmd)
    if ret == 0:
        # Default cl.exe command
        cmd = 'cl.exe'  # Take off /showIncludes to avoid some messed contents in the output files

        # If a resp file is given, just take it and ignore vfr_mode, defines_list, and include_path_list
        # to avoid command too long error
        if resp_file and os.path.isfile(resp_file):
            cmd += f' @"{resp_file}"'

        else:
            cmd += ' /nologo /E /TC'

            # Add /DVFRCOMPILE for vfr
            if vfr_mode:
                cmd += ' /DVFRCOMPILE'

            # Add defines
            for define in defines_list:
                cmd += f' -D {define}'

            # Add include paths
            for include_path in include_path_list:
                cmd += f' /I{include_path}'

        # Add additional attributes
        if add_arg:
            cmd += f' {add_arg}'

        # Add vfr file name
        cmd += f' "{input_file_name}"'

        # Add dump file name
        if dump_file_name:
            # cmd += f' >"{dump_file_name}" 2>&1'
            cmd += f' >"{dump_file_name}"'  # Take off 2>&1 to avoid some messed contents in the output files

        # Execute cl.exe
        ret = os.system(cmd)
        if ret:
            print('  [cl] cmd: %s' % cmd)

    else:
        print('  [cl] Error: cl.exe is not available.')

    return ret


# Invoke TrimPreprocessedVfr or TrimPreprocessedFile in Trim.py to preprocess the given file
# input_file_name: vfr file name to be processed
# vfr_mode (Optional): 0 - normal; 1 - vfr
# dump_file_name: Dump the preprocessed vfr content to the file.
#   If not specified, dump to the default file name: input_file_name.iii
def run_trim(input_file_name, vfr_mode=0, dump_file_name=None):
    basetools_python_path = default_basetools_python_path
    sys.path.append(basetools_python_path)
    try:
        import Trim.Trim as trim  # type: ignore
    except ImportError:
        print('  [Trim] Error: Failed to import Trim module')
        return 1

    if dump_file_name is None:
        dump_file_name = input_file_name + '.iii'

    # Invoke TrimPreprocessedVfr() in Trim.py
    if vfr_mode:
        trim.TrimPreprocessedVfr(input_file_name, dump_file_name)
    else:
        trim.TrimPreprocessedFile(input_file_name, dump_file_name, ConvertHex=False, TrimLong=True)

    # Verify output file presence
    if os.path.isfile(dump_file_name):
        return 0
    else:
        print('  [Trim] Error: Failed to create output file: %s' % dump_file_name)
        return 1


# Preprocess file content with cl and trim
# input_file_name: vfr file name to be processed
# include_path_list: A list of include paths
# defines_list (Optional): A list of defines
# add_arg (Optional): Additional attributes to be added
# resp_file (Optional): Response file for cl
# vfr_mode (Optional): 0 - normal; 1 - vfr
# dump_file_name (Optional): If a dump file name is given, write the preprocessed vfr content to the file
# Return: Processed vfr content
def run_preprocess(
    input_file_name, include_path_list, defines_list=[],
    add_arg='', resp_file=None, vfr_mode=0, dump_file_name=None
):
    temp_processed_vfr = input_file_name + '.i'

    # for include_path in include_path_list:
    #    print('  [vfr]   %s' % include_path)

    # Invoke cl to preprocess vfr file
    ret = run_cl(input_file_name, include_path_list, defines_list, add_arg, resp_file, vfr_mode, temp_processed_vfr)

    # Invoke Trim to preprocess vfr file
    if os.path.isfile(temp_processed_vfr):
        ret += run_trim(temp_processed_vfr, vfr_mode, dump_file_name)
        os.remove(temp_processed_vfr)
    else:
        # If cl fails, still use run_trim to preprocess vfr file
        ret += run_trim(input_file_name, dump_file_name)

    if ret != 0:
        print('  [vfr] Warning: Error occurred while preprocessing vfr file: %s, ret = %d' % (input_file_name, ret))
        output_vfr = input_file_name
    elif not os.path.isfile(dump_file_name):
        output_vfr = input_file_name
    else:
        output_vfr = dump_file_name

    # Claim the vfr content to return
    with open(output_vfr, 'r') as f:
        vfr_content = f.read()

    return vfr_content


# Collect varstore, efivarstore, and namevaluevarstore information from vfr content
# vfr_content: Content read and processed from a vfr file
# uni_str_dict: Dictionary containing {<string token>: <string content>} mappings from uni files
# verbose: 0 - Default; 1 - Print Debug message
# Return: A dictionary containing varstore information: {name: varstore}
def vfr_collect_varstore(vfr_content, uni_str_dict, verbose=0):
    varstore_dict = {}

    # Regular expression for varstore/efivarstore/namevaluevarstore blocks
    varstore_pattern = re.compile(r'(?<!\w)varstore\s+(\w+).*?name\s*=\s*(\w+).*?;', re.DOTALL | re.IGNORECASE)
    efivarstore_pattern = re.compile(r'(?<!\w)efivarstore\s+(\w+).*?name\s*=\s*(\w+).*?;', re.DOTALL | re.IGNORECASE)
    namevalue_pattern = re.compile(r'(?<!\w)namevaluevarstore\s+(\w+)(.*?);', re.DOTALL | re.IGNORECASE)

    # Process varstore blocks
    varstore_matches = varstore_pattern.findall(vfr_content)
    for varstore, name in varstore_matches:
        varstore_dict[name] = varstore

    # Process efivarstore blocks
    efivarstore_matches = efivarstore_pattern.findall(vfr_content)
    for varstore, name in efivarstore_matches:
        varstore_dict[name] = varstore

    # Process namevaluevarstore blocks
    namevalue_matches = namevalue_pattern.findall(vfr_content)
    for varstore, token_content in namevalue_matches:
        # Collect all STRING_TOKENs and their values from uni_str_dict
        string_token_pattern = re.compile(r'STRING_TOKEN\s*\(\s*([^)]+)\s*\)')
        name_values = string_token_pattern.findall(token_content)
        for idx, token in enumerate(name_values):
            if token in uni_str_dict:
                varstore_dict[f'{varstore}[{idx}]'] = uni_str_dict[token]
            else:
                print(f'  [vfr] Warning: STRING_TOKEN {token} not found in UNI.')

    # Debug print for varstore_dict
    if verbose:
        print()
        print_dict(varstore_dict, 'varstore_dict', indent=2)
        print()

    return varstore_dict


# Function to judge the condition string based on current structs values
# condition: The condition string to be judged
# structs: The current structs dictionary with values
# Return: Boolean result of the condition
def vfr_condition_judge(condition, structs):
    # Remove line breaks and extra spaces to normalize the condition string
    condition = ' '.join(condition.split())

    # Pattern to match condition parts like "NOT ideqval CBS_CONFIG.CbsDbgCpuSnpMemCover == 2"
    condition_pattern = re.compile(r'(NOT\s+)?ideqval\s+(\w+)\.(\w+)\s*==\s*(\w+)', re.IGNORECASE)

    # Replace logical operators for Python evaluation
    condition = condition.replace('AND', 'and').replace('OR', 'or')

    # Function to evaluate each condition part
    def evaluate_condition(match):
        neg, struct_name, member_name, value = match.groups()

        # Get the current value from structs
        struct = structs.get(struct_name)
        current_value = None
        if struct is not None:
            member = struct.find(f"./Member[@name='{member_name}']")
            if member is not None:
                current_value = member.get('default')

        # If current_value is None or an empty string, treat as False
        if not current_value:
            return False

        # Compare the current value to the target value
        is_equal = str(current_value) == value

        # Reverse the result if 'NOT' is present
        if neg and neg.strip().upper() == 'NOT':
            is_equal = not is_equal

        return is_equal

    # Evaluate all conditions and substitute their results in the condition string
    result_str = condition_pattern.sub(lambda match: str(evaluate_condition(match)), condition)
    # print('condition: %s' % condition)
    # print('result_str: %s' % result_str)

    # Evaluate the combined condition string
    try:
        result = eval(result_str)
    except Exception:
        # print(f'Error evaluating condition: {condition}. Error: {e}')
        result = False

    return result


# Process suppressif/grayoutif/disableif statements from the given vfr content
# vfr_content: Content read and processed from a vfr file
# structs: Dictionary containing current struct values
# dump_file_name (Optional): If a dump file name is given, write the processed vfr content to the file
# Return: Processed vfr content
def vfr_process_statements(vfr_content, structs, dump_file_name=None):
    # Find all condition blocks
    statements = re.findall(
        r'((?<!\w)suppressif\b|(?<!\w)grayoutif\b|(?<!\w)disableif\b)\s+([\s\S]*?);',
        vfr_content,
        re.IGNORECASE
    )

    for statement in statements:
        # condition_type = statement[0].lower()  # Convert to lowercase for consistency
        condition = statement[1].strip()  # Remove extra spaces and newline for condition processing

        # Evaluate the condition
        # print(f'Found {condition_type} condition: {condition}')
        result = vfr_condition_judge(condition, structs)
        # print(f'result: {result}\n')

        if result:
            # Locate the statement in vfr_content
            statement_match = re.search(
                rf'{re.escape(statement[0])}\s+{re.escape(statement[1])};',
                vfr_content,
                re.IGNORECASE
            )

            # Replace the suppressif/grayoutif/disableif block with /*
            vfr_content_pre = vfr_content[:statement_match.start()] + '/*'
            vfr_content_post = vfr_content[statement_match.end():]

            # Search forward to pair the corresponding endif, and replace with */
            sgd_matches = re.finditer(
                r'((?<!\w)suppressif\b)|((?<!\w)grayoutif\b)|((?<!\w)disableif\b)|((?<!\w)endif;)',
                vfr_content_post,
                re.IGNORECASE
            )
            stack = []  # Initialize a stack to keep track of opened conditions
            for match in sgd_matches:
                # print(match)
                # print(f'  match.group(): {match.group()}')
                # print(f'  stack before: {stack}')
                token = match.group().lower()
                if token in ['suppressif', 'grayoutif', 'disableif']:
                    stack.append(token)
                elif token == 'endif;':
                    if stack:
                        stack.pop()
                    else:
                        vfr_content_post = vfr_content_post[:match.start()] + '*/' + vfr_content_post[match.end():]
                        # print(f'  stack after: {stack}')
                        break
                # print(f'  stack after: {stack}')

            vfr_content = vfr_content_pre + vfr_content_post

    vfr_content = vfr_remove_comments(vfr_content, dump_file_name)
    return vfr_content


# According to the given INF file path, attempt to find the corresponding vfrpp_resp file automatically
# inf_file_path: Path to the INF file
# WorkspaceRoot: Root of the workspace directory.
#   In this file it should be the same as srcdir or PlatformBuilder.GetWorkspaceRoot()
# verbose (Optional): Print verbose information
# Return: Path to the vfrpp_resp file. Empty string '' if not found
def find_vfrpp_resp(inf_file_path, WorkspaceRoot, verbose=0):
    vfrpp_resp_path = ''

    # Debug message
    if verbose:
        time_start = time.time()
        print(f'  [find_vfrpp_resp] WorkspaceRoot: {WorkspaceRoot}')
        print(f'  [find_vfrpp_resp] inf_file_path: {inf_file_path}')

    # Validate inf_file_path and create a search string
    # e.g. Silicon\Amd\Turin_SERVER\AmdCbsPkg\Library\Family\0x1A\BRH\External\CbsSetupLib.inf
    #                                      => library\family\0x1a\brh\external\cbssetuplib\output\vfrpp_resp.txt
    if inf_file_path and os.path.isfile(inf_file_path):
        search_string = os.path.join(
            os.path.splitext(os.path.normpath(inf_file_path).lower().split('pkg\\')[-1])[0],
            'output',
            'vfrpp_resp.txt'
        )
        if verbose:
            print(f'  [find_vfrpp_resp] search_string = {search_string}')

    else:
        if verbose:
            print(f'  [find_vfrpp_resp] inf_file_path is not valid: {inf_file_path}')
        return vfrpp_resp_path

    # Validate WorkspaceRoot
    if not WorkspaceRoot or not os.path.isdir(WorkspaceRoot):
        if verbose:
            print(f'  [find_vfrpp_resp] WorkspaceRoot is not valid: {WorkspaceRoot}')
        return vfrpp_resp_path

    # Collect a candidate file list of all vfrpp_resp.txt under Build folder
    vfrpp_resp_list = []
    for root, _, files in os.walk(os.path.join(WorkspaceRoot, 'Build')):
        if "vfrpp_resp.txt" in files:
            vfrpp_resp_list.append(os.path.normpath(os.path.join(root, 'vfrpp_resp.txt')))

    # Select the vfrpp_resp file that matches the given inf file
    for vfrpp_resp in vfrpp_resp_list:
        if vfrpp_resp.lower().endswith(search_string.lower()):
            vfrpp_resp_path = vfrpp_resp
            break

    # Debug message
    if verbose:
        print(f'  [find_vfrpp_resp] vfrpp_resp_path: {vfrpp_resp_path}')
        print_time(time_start, '[find_vfrpp_resp] ', 2)

    return vfrpp_resp_path


# Get a dictionary of PCD values from build report
# build_report: Path to the build report
# module_name: Module name which will be used to looked up in the build report
# platform_builder: PlatformBuilder object
# Return: A dictionary of PCD values
def get_pcd_from_build_report(build_report, module_name, platform_builder):
    FixedPcd_dict = {}
    module_pcd_dict = {}
    module_found = False

    # Parse the given build report
    print('\nParsing file: %s' % build_report)
    try:
        packagepathcsv = ','.join(platform_builder.GetPackagesPath())
        protectedWordsDict = {}
        build_report_parser = BuildReport(
            build_report,
            platform_builder.GetWorkspaceRoot(),
            packagepathcsv,
            protectedWordsDict
        )
        build_report_parser.BasicParse()

    except Exception as e:
        print('  [PCD] Failed to parse build report %s: %s' % (build_report, e))
        return FixedPcd_dict

    # Look up module with module name, and get the PCD values
    for module in build_report_parser.Modules.values():
        if module.Name == module_name:
            module_found = True
            module_pcd_dict = module.PCDs
            break

    # Validate whether the module is found with PCDs
    if not module_found:
        print('  [PCD] Module %s is not found in build report %s' % (module_name, build_report))
        return FixedPcd_dict  # Return an empty dictionary

    elif len(module_pcd_dict) == 0:
        print('  [PCD] Module %s found but has no PCDs' % module_name)
        return FixedPcd_dict  # Return an empty dictionary

    else:
        # Translate the module.PCDs to FixedPcd_dict
        # module.PCDs {'aaaPkgTokenSpaceGuid.PcdName':'HexValue (DecValue)'} ==> FixedPcd_dict {'PcdName':'HexValue'}
        for key, value in module_pcd_dict.items():
            key_mod = key.split('.')[-1]
            if key_mod:
                value_mod = value
                if value_mod.startswith("0x"):
                    value_mod = re.sub(r'\(.*\)', '', value_mod)
                FixedPcd_dict[key_mod] = value_mod

    return FixedPcd_dict


# Extract a list of defines and additional attributes from the given vfrpp_resp file
# vfrpp_resp: The input file containing the response string from the vfr preprocessor
# Return:
#   defines_list: A list of defines (currently unused, but reserved for future implementation)
#   add_arg: Additional attributes (currently unused, but reserved for future implementation)
#   modified_resp: Modified resp file for later usage
def get_defines_list_and_add_arg(vfrpp_resp):
    defines_list = []  # Do nothing for now
    add_arg = ''  # Do nothing for now
    base_name, ext = os.path.splitext(os.path.basename(vfrpp_resp))
    modified_resp = os.path.join(srcdir, base_name+'_mod.txt')

    print('\nApplying resp file: %s -> %s' % (vfrpp_resp, modified_resp))
    clean_up_list.append(modified_resp)

    # Open and read the input file
    with open(vfrpp_resp, 'r') as file:
        content = file.read()

    # Use regex to match /FI followed by a file path, with or without a space
    fi_pattern = re.compile(r'(/FI\s*[\S]+)(?=\s|$|/|-)')

    # Take off force include files /FI since we do not want to replace the string ids in vfr_content
    modified_content = re.sub(fi_pattern, '', content)

    # Write the modified content to the output file
    with open(modified_resp, 'w') as file:
        file.write(modified_content)

    return defines_list, add_arg, modified_resp


# Search string tokens in the given uni files
# uni_file_list: A list of uni files
# language: language selection
# encoding (Optional): encoding for reading files, if not provided we should detect encoding with chardet (recommended)
# verbose: Print encoding information or not
# Return: A dictionary with {<string token>:<string content>}
def load_string_from_uni(uni_file_list, language='en-US', encoding=None, verbose=0):
    uni_str_dict = {}
    # Regular expression pattern for string tokens
    token_pattern = re.compile(rf'#string\s+(\w+)\s+#language\s+{language}\s+"(.*?)"')

    for uni_file in uni_file_list:
        if os.path.isfile(uni_file):
            # Read raw data with rb
            with open(uni_file, 'rb') as file:
                raw_data = file.read()

            # Detect the encoding
            target_enc = None
            if encoding:
                target_enc = encoding
            else:
                chardet_result = chardet.detect(raw_data)
                target_enc = chardet_result['encoding']
                confidence = chardet_result['confidence']
                if verbose:
                    print(
                        f'  [uni] Detected encoding: {target_enc} with confidence {confidence}, '
                        f'uni_file: {os.path.basename(uni_file)}'
                    )

            # Read file with the detected encoding
            if target_enc:
                try:
                    with open(uni_file, 'r', encoding=target_enc) as file:
                        for line in file:
                            match = token_pattern.search(line)
                            if match:
                                str_token = match.group(1).strip()
                                str_content = match.group(2).strip()
                                # Patch STR_NULL with a space
                                if str_content == '':
                                    str_content = ' '
                                uni_str_dict[str_token] = str_content
                except Exception as e:
                    print(f'  [uni] Error reading uni file with {target_enc} encoding: {uni_file}')
                    print(f'        {e}')
            else:
                print(f'  [uni] Encoding could not be detected: {uni_file}')

    return uni_str_dict


# Get a list of header files and include paths regarding the given vfr file,
#   and patches unfound header files in the given vfr content
# vfr_content: Content read and processed from a vfr file
# vfr_dir: directory of the source vfr file
# search_path_list: A list of include paths from the inf file
# dump_file_name (Optional): If a dump file name is given, write the patched vfr content to the file
# Return:
#   header_file_list: A list of header files
#   include_path_list: A list of include paths which are actually used by vfr_content
#   vfr_content: Processed vfr content
def get_header_file_list(vfr_content, vfr_dir, search_path_list, dump_file_name=None):
    header_files = set()  # Use a set to avoid duplicates
    include_paths = set()

    # Regular expression pattern to match #include lines
    include_pattern = re.compile(r'#include\s*[<"](.+?\.h)[">]', re.IGNORECASE)

    # Define search paths: current dir, parent dir, pkg dir, and default search paths
    search_paths = list(search_path_list)  # Avoid mutating original list
    search_paths.append(vfr_dir)  # current directory
    search_paths.append(os.path.abspath(os.path.join(vfr_dir, os.pardir)))  # parent directory

    # Search for pkg directory in the path
    pkg_dir = None
    path_parts = vfr_dir.split(os.sep)
    if os.path.splitdrive(path_parts[0])[1] == '':
        path_parts[0] = path_parts[0] + os.sep  # Patch the separator for Drive
    for i, part in enumerate(path_parts):
        if 'Pkg' in part:
            pkg_dir = os.path.join(*path_parts[:i+1])
            break

    if pkg_dir:
        # Recursively search pkg directory and subdirectories
        for root, _, _ in os.walk(pkg_dir):
            search_paths.append(root)

    # Recursive function to find header files
    def find_header_files(include, vfr_content):
        # print('  [*.h] Looking for header file: %s' % include)
        path_found = 0
        for path in search_paths:
            header_file_path = os.path.normpath(os.path.join(path, include))
            if os.path.isfile(header_file_path):
                path_found = 1
                if header_file_path not in header_files:
                    print('  [*.h] %s' % header_file_path)
                    header_files.add(header_file_path)
                    include_paths.add(path)

                    # Read header content to search for nested includes
                    with open(header_file_path, 'r') as header_file:
                        header_content = header_file.read()
                        nested_includes = include_pattern.findall(header_content)
                        for nested_include in nested_includes:
                            find_header_files(nested_include, vfr_content)  # Recursive call
                break  # Stop after finding the first match
        if not path_found:
            print('  [*.h] Warning: Header file not found: %s' % include)
            # Comment out the #include line in vfr_content
            vfr_content = vfr_content.replace(f'#include "{include}"', f'//(NOT_FOUND)#include "{include}"')
            vfr_content = vfr_content.replace(f'#include <{include}>', f'//(NOT_FOUND)#include <{include}>')
        return vfr_content  # Return the modified vfr_content

    # Find initial includes in the VFR content
    includes = include_pattern.findall(vfr_content)
    for include in includes:
        vfr_content = find_header_files(include, vfr_content)  # Update vfr_content

    # Convert sets to lists before returning
    header_file_list = list(header_files)
    include_path_list = list(include_paths)

    # Print a warning if no header files are found
    if not header_file_list:
        print('  [vfr] Warning: No header files found for the given vfr files.')

    # Write to a dump file if provided
    if dump_file_name:
        with open(dump_file_name, 'w') as f:
            f.write(vfr_content)

    return header_file_list, include_path_list, vfr_content


StringIdentifier_dict = {
    'UINT8': 'uint8_t',
    'INT8': 'int8_t',
    'UINT16': 'uint16_t',
    'INT16': 'int16_t',
    'UINT32': 'uint32_t',
    'INT32': 'int32_t',
    'UINT64': 'uint64_t',
    'INT64': 'int64_t',
    'UINTN': 'uint32_t',
    'BOOLEAN': 'bool',
    'EFI_HII_DATE': 'uint32_t',
    'EFI_HII_TIME': 'uint32_t',
    'EFI_HII_REF': ''
}


# Search struct definition in the given header files
# header_file_list: A list of header files
# verbose: 0 - Not to print; 1 - Print Error; 2 - Print Error and Info
# Return: A dictionary with {<struct_name>:{<member_name>:<member_type>}},
#   while member_type might point to certain struct_name
def load_type_from_header(header_file_list, verbose=2):
    # Regular expression patterns for typedef struct and members
    struct_pattern = re.compile(r'typedef\s+struct(\s+_\w+)?\s*{([\s\S]*?)}\s*(\w+);', re.DOTALL)
    member_pattern = re.compile(r'\s*(\w+)\s+(\w+)(\[\d+\])?;', re.DOTALL)

    typedef_struct_dict = {}
    for header_file in header_file_list:
        if verbose > 1:
            print(f'  [*.h] Loading {header_file}')
        try:
            with open(header_file, 'r', encoding='utf-8') as file:
                content = file.read()
                # Find all typedef struct blocks
                for struct_match in struct_pattern.finditer(content):
                    struct_prefix, struct_body, struct_name = struct_match.groups()
                    if struct_name not in typedef_struct_dict.keys():
                        typedef_struct_dict[struct_name] = {}

                    for member_match in member_pattern.finditer(struct_body):
                        member_type, member_name, array_size = member_match.groups()
                        if member_type in StringIdentifier_dict.keys():
                            typedef_struct_dict[struct_name][member_name] = StringIdentifier_dict.get(member_type)
                        else:
                            typedef_struct_dict[struct_name][member_name] = member_type

        except FileNotFoundError:
            if verbose:
                print(f'  [*.h] File not found: {header_file}')
        except Exception as e:
            if verbose:
                print(f'  [*.h] Error reading {header_file}: {e}')

    return typedef_struct_dict


# Print error/warning message for invalid values in current_member
# verbose: 0 - Not to print; 1 - Print Error; 2 - Print Error and Warnings
def validate_member(struct_name, current_member, verbose=2):
    error_count = 0
    warning_count = 0
    if current_member.get('name') == '':
        if verbose:
            print('  [vfr] Error: Member is missing a name attibute')
        error_count += 1

    if current_member.get('default') == '':
        if verbose > 1:
            print(f'  [vfr] Warning: Member {struct_name}.{current_member.get('name')} is missing a default value')
        warning_count += 1

    if current_member.get('type') == '':
        if verbose:
            print(f'  [vfr] Error: Member {struct_name}.{current_member.get('name')} is missing type')
        error_count += 1

    if current_member.get('prompt') == '':
        if verbose > 1:
            print(f'  [vfr] Warning: Member {struct_name}.{current_member.get('name')} is missing a prompt string')
        warning_count += 1

    if current_member.get('help') == '':
        if verbose > 1:
            print(f'  [vfr] Warning: Member {struct_name}.{current_member.get('name')} is missing a help string')
        warning_count += 1

    return error_count, warning_count


# Validate all structs and their members in the structs dictionary
# structs: Dictionary containing all structs and their associated members
# verbose: 0 - Not to print; 1 - Print Error; 2 - Print Error and Warnings
def validate_structs(structs, verbose=1):
    error_count = 0
    warning_count = 0
    # Iterate over each struct in the structs dictionary
    for struct_name, struct in structs.items():
        if verbose > 2:
            print('Validating Struct: %s' % struct_name)
        # Iterate over each member in the current struct
        for member in struct.findall('Member'):
            member_name = member.get('name')
            if verbose > 2:
                print('  Validating Member: %s' % member_name)
            err, warn = validate_member(struct_name, member, verbose)  # Call the existing validate_member function
            error_count += err
            warning_count += warn
    return error_count, warning_count


# Validate all enums and their values in the enums element
# enums_element: The Enums element
# patch_mode: 0 - Disable patch mode; 1 - Modify Enum name on error to avoid use of member type
# verbose: 0 - Not to print; 1 - Print Error
def validate_enums(enums_element, patch_mode=0, verbose=1):
    error_count = 0
    for enum in enums_element.findall('Enum'):
        enum_name = enum.get('name')
        if not enum_name:
            if verbose:
                print('  [vfr] Error: Enum is missing a name attibute')
            error_count += 1

        value_valid = True
        for value in enum.findall('Value'):
            if value.get('name') == '':
                if verbose:
                    print('  [vfr] Error: Value is missing a name attibute in Enum %s' % enum_name)
                error_count += 1
                value_valid = False

            if value.get('value') == '':
                if verbose:
                    print('  [vfr] Error: Value is missing a value attibute in Enum %s' % enum_name)
                error_count += 1
                value_valid = False

        if patch_mode and enum_name and not value_valid:
            enum.set('name', '_' + enum_name)

    return error_count


# Debug function to print a dictionary
# print_dict: A dictionary to be printed
# dict_name: Name of the dictionary
# indent: Indentation level
def print_dict(print_dict, dict_name='dict', indent=0):
    if len(print_dict) == 0:
        print(f'{dict_name} = {print_dict}')
        return
    for key, value in print_dict.items():
        print(' ' * indent, end='')
        print(f'{dict_name}[{key}] = {value}')


# Debug function to print a tuple of blocks
# blocks: A list of tuples ('oneof|numeric|checkbox', 'block_content')
def print_blocks(blocks, newlines=1):
    for index, block in enumerate(blocks):
        print('\n' * newlines, end='')
        print(index)
        print(block)


# Debug function to print time period from time_start to now time.time()
# time_start: Start time
# prefix: Prefix string for debug print
# indent: Indentation level
def print_time(time_start, prefix='', indent=0):
    period = time.time()-time_start
    print()
    print(' ' * indent, end='')
    if prefix:
        print('%s ' % prefix, end='')
    print('Time = %02dm %02ds\n' % (int(period/60), int(period % 60)))


# Parse given VFR content (typically from a single file) and append its structure to the XML element
# vfr_content: VFR content to be parsed
# uni_str_dict: A dictionary for string token reference
# root: Root element of the XML tree
# structs: A dictionary of all struct elements
# repeated_items: List of tuples containing (struct_name, member_name) for repeated items
# verbose: 0 - Not to print; 1 - Print Warning; 2 - Print Info and Warning
# parse_mode: 0 - Generate a new xml from a vfr; 1 - re-parse vfr to update values in xml
def parse_vfr_content(vfr_content, uni_str_dict, root, structs, repeated_items, verbose=2, parse_mode=0):
    # Prepare a dictionary for varstore
    varstore_dict = vfr_collect_varstore(vfr_content, uni_str_dict)

    # Get the enum and structs elements
    enums_element = root.find('Enums')
    structs_element = root.find('Structs')

    print('\n  Start parsing...')
    # Initialize variables and data structures
    current_member = None
    current_struct = None
    current_prettyname = ''
    current_help = ''
    default_set = False
    special_str = '-1FAULT-1'

    # Regular expression for oneof/numeric/checkbox blocks
    all_blocks = re.findall(
        r'((?<!\w)oneof)\s+(.*?)endoneof;|((?<!\w)numeric)\s+(.*?)endnumeric;|((?<!\w)checkbox)\s+(.*?)endcheckbox;',
        vfr_content,
        re.DOTALL | re.IGNORECASE
    )
    # print_blocks(all_blocks)  # Temporary debug print

    # Iterate over each block
    for block in all_blocks:
        if block[0] == 'oneof':
            block_type = block[0]     # oneof
            block_content = block[1]  # varid = ...,
        elif block[2] == 'numeric':
            block_type = block[2]     # numeric
            block_content = block[3]  # varid = ...,
        elif block[4] == 'checkbox':
            block_type = block[4]     # checkbox
            block_content = block[5]  # varid = ...,

        # Start parsing 'oneof' block
        if block_type == 'oneof':
            current_member = ET.Element('Member', {
                'prettyname': '',
                'name': '',
                'default': '',
                'count': '1',
                'type': '',
                'help': ''
            })

        # Start parsing 'numeric' block
        elif block_type == 'numeric':
            current_member = ET.Element('Member', {
                'prettyname': '',
                'name': '',
                'default': '',
                'min': '',
                'max': '',
                'type': '',
                'help': ''
            })

        # Start parsing 'checkbox' block
        elif block_type == 'checkbox':
            current_member = ET.Element('Member', {
                'prettyname': '',
                'name': '',
                'default': '',
                'type': 'checkbox',
                'help': ''
            })

        if current_member is not None:
            # Get varid in the block_content
            matches = re.findall(r'(?<!\w)varid\s*=\s*([^,]+)', block_content, re.DOTALL | re.IGNORECASE)
            if matches:
                varid = matches[0]  # If there are more than one varid, just take the first one
                varid_split = varid.split('.')
                struct_name = varid_split[0]
                if len(varid_split) > 1:
                    member_name = '.'.join(varid_split[1:])
                else:
                    print('  [vfr] Warning: Invalid varid = %s' % varid)
                    continue

                # Patch struct_name according to varstore/efivarstore/namevaluevarstore names
                if varstore_dict.get(struct_name) is not None:
                    # print('  [vfr] Patch varstore_dict: %s -> %s' % (struct_name, varstore_dict[struct_name]))
                    struct_name = varstore_dict[struct_name]

                struct_name = struct_name + '_STRUCT'  # Add _STRUCT to struct name to avoid enum redefinition

                # Check if the struct already exists, if not, create it
                if struct_name not in structs:
                    current_struct = ET.SubElement(structs_element, 'Struct', name=struct_name, help='')
                    structs[struct_name] = current_struct
                else:
                    current_struct = structs[struct_name]

                # Update or create the member
                existing_member = current_struct.find(f"./Member[@name='{member_name}']")
                if existing_member is not None:
                    current_member = existing_member
                    if parse_mode == 0:
                        # New parse mode: Add to repeated_items list if not already present
                        if (struct_name, member_name) not in repeated_items:
                            repeated_items.append((struct_name, member_name))
                            if verbose > 1:
                                print(f'  [vfr] Info: Duplicate Member found in Struct: {struct_name}.{member_name}')
                else:
                    current_member.set('name', member_name)

            # Get prompt in the block_content
            matches = re.findall(
                r'(?<!\w)prompt\s*=\s*STRING_TOKEN\s*\(\s*([^)]+?)\s*\)',
                block_content,
                re.DOTALL | re.IGNORECASE
            )
            if matches:
                prompt_token = matches[0]  # If there are more than one prompt, just take the first one
                current_prettyname = uni_str_dict.get(prompt_token, '')
                if current_prettyname is None and verbose:
                    print(
                        '  [uni] Warning: Pretty Name not found. '
                        f'Member name={current_member.get('name')}, '
                        f'Token={prompt_token}, '
                    )

            # Get help in the block_content
            matches = re.findall(
                r'(?<!\w)help\s*=\s*STRING_TOKEN\s*\(\s*([^)]+?)\s*\)',
                block_content,
                re.DOTALL | re.IGNORECASE
            )
            if matches:
                help_token = matches[0]  # If there are more than one help, just take the first one
                current_help = uni_str_dict.get(help_token, '')
                if current_help is None and verbose:
                    print(
                        '  [uni] Warning: Help not found. '
                        f'Member name={current_member.get('name')}, Token={help_token}, '
                    )

            # Parse numeric-specific attributes
            if block_type == 'numeric':
                # minimum
                matches = re.findall(r'(?<!\w)minimum\s*=\s*([^,]+)', block_content, re.DOTALL | re.IGNORECASE)
                if matches:
                    minimum_value = matches[0].strip()
                    # If there are more than one minimum, just take the first one
                    current_member.set('min', minimum_value)

                # maximum
                matches = re.findall(r'(?<!\w)maximum\s*=\s*([^,]+)', block_content, re.DOTALL | re.IGNORECASE)
                if matches:
                    maximum_value = matches[0].strip()
                    current_member.set('max', maximum_value)

            # Parse default value
            matches = re.findall(r'(?<!\w)default\s*=\s*([^,]+)', block_content, re.DOTALL | re.IGNORECASE)
            if matches:
                default_value = matches[0].strip()
                current_member.set('default', default_value)
                default_set = True

            if block_type == 'oneof':  # Parse oneof options with default flag handling
                # Create a new Enum element for each oneof
                enum_help = xml_get_enum_help(struct_name, member_name)
                enum_name = enum_help.upper()
                existing_enum = enums_element.find(f'./Enum[@name="{enum_name}"]')
                if existing_enum is not None:
                    current_enum = existing_enum
                else:
                    current_enum = ET.Element('Enum', attrib={'name': enum_name, 'help': enum_help})

                # Parse each option
                matches = re.findall(r'(?<!\w)option\s+(.*?);', block_content, re.DOTALL | re.IGNORECASE)
                for option in matches:
                    # option text = STRING_TOKEN(text_token), value = {value}, flags = {flags};
                    text_matches = re.findall(
                        r'(?<!\w)text\s*=\s*STRING_TOKEN\s*\(\s*([^)]+?)\s*\)',
                        option,
                        re.DOTALL | re.IGNORECASE
                    )
                    text_token = ''
                    if text_matches:
                        text_token = text_matches[0].strip()

                    value_matches = re.findall(r'(?<!\w)value\s*=\s*([^,]+)', option, re.DOTALL | re.IGNORECASE)
                    option_value = ''
                    if value_matches:
                        option_value = value_matches[0].strip()

                    flags_matches = re.findall(r'(?<!\w)flags\s*=\s*([\w\s|]+)', option, re.DOTALL | re.IGNORECASE)
                    flags = []
                    for match in flags_matches:
                        for flag in match.split('|'):
                            flag = flag.strip()
                            if flag:
                                flags.append(flag)

                    # Append Value to the Enum
                    if text_matches and value_matches:
                        enum_value_name = uni_str_dict.get(text_token, '')
                        existing_value = current_enum.find(f'./Value[@name="{enum_value_name}"]')
                        if existing_value is None:
                            ET.SubElement(current_enum, 'Value', name=enum_value_name, value=option_value, help='')

                    # Check if 'DEFAULT' flag is specifically set and not just any mention of 'DEFAULT'
                    if 'DEFAULT' in flags and not default_set:
                        if (struct_name, member_name) in repeated_items:
                            if (parse_mode == 0) and (option_value != current_member.get('default')):
                                # For repeated_items, only assign the value when the default value is all the same,
                                # otherwise takes ''
                                current_member.set('default', '')
                            elif parse_mode == 1:
                                # For re-parsing mode, accept the existing empty string
                                if current_member.get('default') == '':
                                    current_member.set('default', option_value)
                                elif option_value != current_member.get('default'):
                                    current_member.set('default', special_str)  # Assume special_str is unique
                        else:
                            current_member.set('default', option_value)
                            default_set = True

                # Claim and append the new Enum element
                if (len(current_enum) > 0) and (current_enum not in enums_element):
                    enums_element.append(current_enum)

            # Debug patch for numeric items, use this only for generating test data
            # Sometimes we have default="", but 0 is not in the range of (min, max),
            #   that will cause a InvalidRangeError in ConfigEditor
            if patch_numeric_default_for_debug and (block_type == 'numeric') and not default_set:
                num_min = eval(current_member.get('min', '0'))
                num_max = eval(current_member.get('max', '0'))
                if (num_min > 0) or (num_max < 0):
                    current_member.set('default', str(num_min))

            # End parsing for an oneof|numeric|checkbox block
            # Set prettyname and help for each option
            current_member.set('prettyname', current_prettyname)
            current_member.set('help', current_help)

            # Fix up special_str
            if current_member.get('default') == special_str:
                current_member.set('default', '')

            # Claim and append new member to the struct, and then clean up for the next parsing
            if current_member not in current_struct:
                current_struct.append(current_member)
            current_member = None
            current_prettyname = ''
            current_help = ''
            default_set = False  # Reset default_set for the next block


# Parse a list of vfr files in re-parse mode, after evaluating the suppressif, grayoutif, disableif statements
# processed_vfr_dict: A dictionary of {Processed vfr files: Corresponding uni_str_dict}
# root: Root element of the XML tree
# structs: A dictionary of all struct elements
# repeated_items: List of tuples containing (struct_name, member_name) for repeated items
def parse_default_value(processed_vfr_dict, root, structs, repeated_items):
    for vfr_file_name, uni_str_dict in processed_vfr_dict.items():
        if os.path.isfile(vfr_file_name):
            print('Re-Parsing file: %s' % vfr_file_name)
            # Get uni file list for reference
            with open(vfr_file_name, 'r') as file:
                vfr_content = file.read()
                vfr_content_no_comments = vfr_remove_comments(vfr_content)
                vfr_content_statements = vfr_process_statements(
                    vfr_content_no_comments,
                    structs,
                    dump_file_name=os.path.splitext(vfr_file_name)[0]+'_statements.vfr'
                )
                parse_vfr_content(vfr_content_statements, uni_str_dict, root,
                                  structs, repeated_items, verbose=2, parse_mode=1)


# Load struct definition from the header files, and update data type for each struct.member
# processed_vfr_list: A list of pre-processed vfr files.
#   After pre-processed by run_cl(), it should have required typedef struct
# structs: For each struct in structs, this function will try to find the struct definition in the header files,
#   and set data type for each struct.member
# header_file_list (Optional): A list of header files.
#   This is not used if load_type_from_header() succeeds with processed_vfr_list
def parse_data_type(processed_vfr_list, structs, header_file_list=[]):
    # Prepare struct definition dictionary (like a tree)
    typedef_struct_dict = load_type_from_header(processed_vfr_list, verbose=2)
    if len(typedef_struct_dict) == 0:
        typedef_struct_dict = load_type_from_header(header_file_list, verbose=2)

    # Remove Index Pattern
    array_pattern = re.compile(r'\[\d+\]$')

    # Iterate over each struct in the structs dictionary
    for struct_name, struct in structs.items():
        # Iterate over each member in the current struct
        for member in struct.findall('Member'):
            member_name = member.get('name')
            member_type = member.get('type')

            if member_type == 'checkbox':
                member.set('type', 'bool')  # synchronize checkbox type

            else:
                current_struct_name = struct_name.removesuffix('_STRUCT')  # Go back to the parent struct
                next_struct_name = None
                member_name_split = member_name.split('.')

                # When varid = a.b[i].c[j], go through:
                #   typedef_struct_dict[a][b]
                #   typedef_struct_dict[typedef_struct_dict[a][b]][c]
                #   ...
                for member_part in member_name_split:
                    member_part_base = array_pattern.sub('', member_part)  # Remove [index] from member_part

                    current_dict = typedef_struct_dict.get(current_struct_name)
                    if current_dict is None:
                        print(
                            f'  [*.h] Warning: Struct: "{current_struct_name}" '
                            'is not found in the provided header files'
                        )
                        break
                    else:
                        next_struct_name = current_dict.get(member_part_base)
                        if next_struct_name is None:
                            print(
                                f'  [*.h] Struct/Member: {member_part_base} '
                                f'is not found in struct: {current_struct_name}'
                            )
                            break
                        current_struct_name = next_struct_name

                # Claim the data type
                if next_struct_name is not None:
                    member_type = next_struct_name

                member.set('type', member_type)


# Get enum value name according to given enum name and enum value value
# For example:
#   <Enums>
#     <Enum name="{enum_name}">
#       <Value name="{enum_value_name1}" value="{enum_value_value1}" help="" />
#       <Value name="{enum_value_name2}" value="{enum_value_value2}" help="" />
#       <Value name="{enum_value_name3}" value="{enum_value_value3}" help="" />
#       ...
#     </Enum>
#     ...
#   </Enums>
# When we call xml_get_enum_value_name(
#                  enums_element, {enum_name}, {enum_value_value2}, {default_value}), it returns {enum_value_name2}
# enums_element: The Enums element
# enum_name: The name of the enum
# enum_value_value: The value of the enum value
# default (Optional): If the enum value is not found, return this default value
# Return: enum value name
def xml_get_enum_value_name(enums_element, enum_name, enum_value_value, default=None):
    enum_element = enums_element.find(f'./Enum[@name="{enum_name}"]')
    if enum_element is not None:
        enum_value_element = enum_element.find(f'./Value[@value="{enum_value_value}"]')
        if enum_value_element is not None:
            return enum_value_element.get('name', default)
    return default


# Current Naming Rule of Enum name
def xml_get_enum_name(struct_name, member_name):
    knob_name = xml_struct_to_knob_name(struct_name)
    return normalize_name(f'{knob_name}.{member_name}'.upper())


# Current Naming Rule of Enum help
def xml_get_enum_help(struct_name, member_name):
    knob_name = xml_struct_to_knob_name(struct_name)
    return f'{knob_name}.{member_name}'


# Load XML file and return the root element
def xml_get_root_from_file(file_path):
    try:
        with open(file_path, 'r') as f:
            xml_string = f.read()
        xml_root = ET.fromstring(xml_string)
    except Exception:
        print('Failed to get root from file: %s' % file_path)
        xml_root = None
    return xml_root


# Remove Enum from the enums_element
# enums_element: The Enums element
# enum_name: The name of the enum to be removed
def xml_remove_enum(enums_element, enum_name):
    enum = enums_element.find(f'./Enum[@name="{enum_name}"]')
    if enum is not None:
        enums_element.remove(enum)


# Current Naming Rule of Knob name
def xml_struct_to_knob_name(struct_name):
    return struct_name.title().replace('_', '')


# Get knob namespace dictionary
# xml_root: The root of ET.Element
# Return:
#   dict_type == 'NameToType': A dictionary from Knobs namespace {<Knob name>: <Knob type>}
#   dict_type == 'TypeToName': A dictionary from Knobs namespace {<Knob type>: <Knob name>}
def xml_get_knobs_namespace_dict(xml_root, dict_type='NameToType'):
    knob_list = xml_root.findall('./Knobs/Knob')
    knobs_namespace_dict = {}
    for knob in knob_list:
        if dict_type == 'NameToType':
            knobs_namespace_dict[knob.get('name')] = knob.get('type')
        elif dict_type == 'TypeToName':
            knobs_namespace_dict[knob.get('type')] = knob.get('name')
        else:
            print('Unsupported dict_type: %s' % dict_type)
    return knobs_namespace_dict


# Compare xml_root and ref_cfg_root to get differences in:
#   - Default Value changes with the same Struct/Member name
#   - Added/Removed/Changed Enum Values with the same Enum name
# xml_root: The root of ET.Element
# ref_cfg_root: Root of the Reference Config XML to be compared
# concern_cname_list (Optional): A list of items to be compared. Compare all items if concern_list is empty
# verbose: 0 - Not to print; 1 - Print messages
# Return:
#   struct_diff_count: Number of differences in Structs
#   enum_diff_count: Number of differences in Enums
#   diff_cname_list: A list of items with differences
#   match_cname_list: A list of items which are found in both xml_root and ref_cfg_root
def xml_ref_compare(xml_root, ref_cfg_root, concern_cname_list=[], verbose=1):
    struct_diff_count = 0
    enum_diff_count = 0
    diff_cname_list = []
    match_cname_list = []

    # Compare structs members
    knobs_namespace_dict = xml_get_knobs_namespace_dict(xml_root, dict_type='TypeToName')
    for ref_structs in ref_cfg_root.findall('./Structs'):
        for ref_struct in ref_structs.findall('./Struct'):
            # Iterate through each struct_name in the reference xml
            struct_name = ref_struct.get('name')

            # Trying to match the struct_name in xml_root
            for structs in xml_root.findall('./Structs'):
                struct = structs.find(f"./Struct[@name='{struct_name}']")
                if struct is not None:
                    break

            if struct is not None:
                knob_name = knobs_namespace_dict.get(struct_name, struct_name)
                # Iterate through each member in the reference struct
                for ref_member in ref_struct.findall('./Member'):
                    member_name = ref_member.get('name')
                    member = struct.find(f"./Member[@name='{member_name}']")
                    if member is not None:
                        cname = f'{knob_name}.{member_name}'
                        if (not concern_cname_list) or (cname in concern_cname_list):
                            # Found matched member here, compare the values
                            pretty_name = member.get('prettyname', '')
                            if cname not in match_cname_list:
                                match_cname_list.append(cname)
                            if ref_member.get('default') != member.get('default'):
                                struct_diff_count += 1
                                if verbose:
                                    print(
                                        f'  Default value of {cname} ({pretty_name}) is different: '
                                        f'\"{member.get("default")}\" != \"{ref_member.get("default")}\"'
                                    )
                                if cname not in diff_cname_list:
                                    diff_cname_list.append(cname)
    if verbose:
        print(f'==> Number of differences in Structs: {struct_diff_count}\n')

    # Compare enum values
    for ref_enums in ref_cfg_root.findall('./Enums'):
        for ref_enum in ref_enums.findall('./Enum'):
            # Get the reference enum name
            enum_name = ref_enum.get('name')
            enum_help = ref_enum.get('help')

            # Try to match the same enum in xml_root
            enum = None
            for enums in xml_root.findall('./Enums'):
                enum = enums.find(f"./Enum[@name='{enum_name}']")
                if enum is not None:
                    break

            if enum is not None:
                if (not concern_cname_list) or (enum_help in concern_cname_list):
                    # Found matched enum here, compare the enum and ref_enum
                    ref_values_dict = {
                        value.get('name'): value
                        for value in ref_enum.findall('./Value')
                    }
                    current_values_dict = {
                        value.get('name'): value
                        for value in enum.findall('./Value')
                    }

                    # Track differences
                    added = []  # In current enum but not in ref_enum
                    removed = []  # In ref_enum but not in current enum
                    changed = []  # Same value name, different value value

                    # Check for added or changed values
                    for name, current_value in current_values_dict.items():
                        if name not in ref_values_dict:
                            added.append(name)  # Exists in current enum, but not in ref_enum
                            enum_diff_count += 1
                        else:
                            ref_value = ref_values_dict[name]
                            # Compare 'value' attributes
                            if ref_value.get('value') != current_value.get('value'):
                                changed.append({
                                    "name": name,
                                    "current_value": current_value.get('value'),
                                    "ref_value": ref_value.get('value'),
                                    "current_help": current_value.get('help', ''),
                                    "ref_help": ref_value.get('help', ''),
                                })
                                enum_diff_count += 1

                    # Check for removed values
                    for name in ref_values_dict.keys():
                        if name not in current_values_dict:
                            removed.append(name)  # Exists in ref_enum, but not in current enum
                            enum_diff_count += 1

                    # Output results
                    if added or removed or changed:
                        if enum_help not in diff_cname_list:
                            diff_cname_list.append(enum_help)  # enum_help is actually identical to cname
                        if verbose:
                            print(f'  Enum name=\"{enum_name}\":')
                            if added:
                                print(f'    Added Value(s): {', '.join([f'\"{item}\"' for item in added])}')
                            if removed:
                                print(f'    Removed Value(s): {', '.join([f'\"{item}\"' for item in removed])}')
                            if changed:
                                for diff in changed:
                                    if diff['ref_help'] == diff['current_help']:
                                        help_print = f'\"{diff["ref_help"]}\"'
                                    else:
                                        help_print = f'\"{diff["ref_help"]}\" -> \"{diff["current_help"]}\"'
                                    print(
                                        f'    Changed Value(s): \"{diff["name"]}\" (value: \"{diff["ref_value"]}\"'
                                        f'-> \"{diff['current_value']}\", help: {help_print})'
                                    )

    if verbose:
        print(f'==> Number of differences in Enums: {enum_diff_count}')
        if enum_diff_count > 0:
            print('  Help:')
            print('    Added Value(s): Values in current config but not in the reference config file')
            print('    Removed Value(s): Values in the reference config file but not in current config')
            print(
                '    Changed Value(s): Value names are in both current config and the reference file, '
                'but Value values are different\n'
            )

    return struct_diff_count, enum_diff_count, diff_cname_list, match_cname_list


# Output the given root of ET.Element to a formatted XML file
# xml_root: The root of ET.Element
# xml_file_path: XML file output path
# verbose: 0 - Not to print; 1 - Print Output File Name
def xml_root_to_string(xml_root, xml_file_path, verbose=1):
    xml_str = ET.tostring(xml_root, encoding='unicode', method='xml')

    # Use minidom to format the XML string
    dom = minidom.parseString(xml_str)
    pretty_xml_str = dom.toprettyxml(indent='  ', newl='\n')

    # Remove XML declaration
    formatted_xml = '\n'.join(line for line in pretty_xml_str.splitlines() if not line.strip().startswith('<?xml'))

    # Write the custom-formatted XML to a file
    with open(xml_file_path, 'w', encoding='utf-8') as f:
        f.write(formatted_xml)
        if verbose:
            print('\nOutput file: %s' % xml_file_path)


# Parse a list of vfr files according to the given input inf files, and generate a xml file with the option structures
# input_inf_dict: Dictionary that stores the input inf files:
#   {INF file path to be processed : Corresponding vfr_resp file path}
# input_platform_build_py_path: PlatformBuild.py file path
# output_xml_file_path: XML file output path
# build_report (Optional): Project build report for preprocessing PCD values
# Return: root element in XML, return code: 0 - Completed; 1 - Aborted
def parse_inf_to_xml(input_inf_dict, input_platform_build_py_path, output_xml_file_path, build_report=None):
    global srcdir, default_basetools_python_path, path_ready
    tempdir = os.getcwd()
    # Create root element with namespaces
    root = ET.Element('ConfigSchema', attrib={
        'xmlns:xsi': 'http://www.w3.org/2001/XMLSchema-instance',
        'xsi:noNamespaceSchemaLocation': 'configschema.xsd'
    })

    # Create Enums element and a common ENABLEDDISABLE enum
    enums_element = ET.SubElement(root, 'Enums')
    enable_disable_enum = ET.SubElement(enums_element, 'Enum', name='ENABLEDDISABLE', help='')
    ET.SubElement(enable_disable_enum, 'Value', name='Disabled', value='0', help='')
    ET.SubElement(enable_disable_enum, 'Value', name='Enabled', value='1', help='')

    # Create Structs element to wrap all Struct elements
    structs_element = ET.SubElement(root, 'Structs')
    structs = {}
    repeated_items = []

    # structures to contain all the processed vfr files and header files
    processed_vfr_dict = {}  # {Processed vfr files: Corresponding uni_str_dict}
    header_master_list = []

    if len(input_inf_dict) == 0:
        print('No input inf files provided')
        return None, 1
    vfr_found = False

    # Parse PlatformBuild.py and setup platform_builder object
    os.chdir(srcdir)
    platform_builder = setup_platform_builder(input_platform_build_py_path)
    if platform_build_py_required and platform_builder is None:
        print('Failed to parse PlatformBuild.py in the path: %s' % input_platform_build_py_path)
        return None, 1

    # Setup path again with the right srcdir
    if platform_builder:
        srcdir = platform_builder.GetWorkspaceRoot()
    if not path_ready:
        os.chdir(srcdir)
        for package_path in platform_builder.GetPackagesPath():
            if os.path.isdir(os.path.join(srcdir, package_path, 'BaseTools', 'Source', 'Python')):
                default_basetools_python_path = os.path.normpath(os.path.join(
                    srcdir,
                    package_path,
                    'BaseTools',
                    'Source',
                    'Python')
                )
                sys.path.append(default_basetools_python_path)
                path_ready = True
                break

    # Parse INF file
    for inf_file_name, vfrpp_resp in input_inf_dict.items():
        vfr_file_list = []
        uni_file_list = []
        dec_file_list = []
        search_path_list = []
        module_name = ''
        uni_str_dict = {}
        FixedPcd_dict = {}
        defines_list = []
        add_arg = ''
        modified_resp = None

        # Iterate over each given inf file
        if os.path.isfile(inf_file_name):
            (vfr_file_list, uni_file_list, dec_file_list, search_path_list, module_name, FixedPcd_dict) = inf_get_info(
                inf_file_name, platform_builder)  # Parse INF/DEC/Platform DSC file
            # Only vfr_file_list is mandatory, others are optional for some cases
            if len(vfr_file_list) == 0:
                print(f'No vfr files found in {inf_file_name}')
                continue

            # If given, parse Build Report to get PCD values
            if os.path.isfile(build_report):
                FixedPcd_dict.update(get_pcd_from_build_report(build_report, module_name, platform_builder))

            # If given, parse resp to get defines for cl
            if os.path.isfile(vfrpp_resp):
                defines_list, add_arg, modified_resp = get_defines_list_and_add_arg(vfrpp_resp)

            # Prepare a dictionary for string token
            uni_str_dict = load_string_from_uni(uni_file_list, language='en-US')

            # Iterate over each vfr file found in inf
            for vfr_file_name in vfr_file_list:
                if os.path.isfile(vfr_file_name):
                    with open(vfr_file_name, 'r') as file:
                        include_path_list = []
                        header_file_list = []
                        # Get the directory of the VFR file
                        vfr_dir = os.path.dirname(vfr_file_name)
                        if not vfr_dir:
                            vfr_dir = srcdir

                        # Parse VFR file
                        print('\nParsing file: %s' % vfr_file_name)
                        vfr_content = file.read()
                        vfr_found = True

                        # Remove comments
                        dump_vfr_file_name = os.path.splitext(vfr_file_name)[0]+'_processed_01_no_comments.vfr'
                        vfr_content = vfr_remove_comments(vfr_content, dump_file_name=dump_vfr_file_name)

                        # Process the including vfr and hfr files
                        # time_before_include = time.time()
                        dump_vfr_file_name = os.path.splitext(vfr_file_name)[0]+'_processed_02_includes.vfr'
                        vfr_content = vfr_process_includes(vfr_content, vfr_dir)
                        vfr_content = vfr_remove_comments(vfr_content, dump_file_name=dump_vfr_file_name)
                        # print_time(time_before_include, 'vfr_process_includes', indent=2)

                        # Get a list of header files to be processed
                        dump_vfr_file_name = os.path.splitext(vfr_file_name)[0]+'_processed_03_headers.vfr'
                        header_file_list, include_path_list, vfr_content = get_header_file_list(
                            vfr_content,
                            vfr_dir,
                            search_path_list,
                            dump_file_name=dump_vfr_file_name
                        )

                        # Add header files to the master list
                        for header_file_name in header_file_list:
                            if header_file_name not in header_master_list:
                                header_master_list.append(header_file_name)

                        # Append all possible search paths
                        for include_path in include_path_list:
                            if include_path not in search_path_list:
                                search_path_list.append(include_path)

                        # Process PCDs
                        if len(FixedPcd_dict) > 0:
                            dump_vfr_file_name = os.path.splitext(vfr_file_name)[0]+'_processed_04_pcds.vfr'
                            vfr_content = vfr_process_pcds(
                                vfr_content, FixedPcd_dict, dump_file_name=dump_vfr_file_name
                            )

                        # Process header files for includes and defines
                        input_vfr_file_name = dump_vfr_file_name
                        dump_vfr_file_name = os.path.splitext(vfr_file_name)[0]+'_processed_05_macros.vfr'
                        vfr_content = run_preprocess(
                            input_vfr_file_name, search_path_list, defines_list,
                            add_arg, modified_resp,
                            vfr_mode=1, dump_file_name=dump_vfr_file_name
                        )

                        # Process PCDs again since preprocess may bring additional macros with FixedPcdGet wording,
                        # and then remove redundant newlines
                        dump_vfr_file_name = os.path.splitext(vfr_file_name)[0]+'_processed_06_trimmed.vfr'
                        if len(FixedPcd_dict) > 0:
                            vfr_content = vfr_process_pcds(vfr_content, FixedPcd_dict, dump_file_name=None)
                        vfr_content = vfr_remove_redundant_newlines(vfr_content, dump_file_name=dump_vfr_file_name)
                        processed_vfr_dict[dump_vfr_file_name] = uni_str_dict

                        # Parse VFR content
                        parse_vfr_content(
                            vfr_content, uni_str_dict, root, structs, repeated_items, verbose=2, parse_mode=0
                        )

                else:
                    print('VFR File not found: %s' % vfr_file_name)
        else:
            print('INF File not found: %s' % inf_file_name)
            os.chdir(tempdir)
            return None, 1

    if not vfr_found:
        print('No VFR file found')
        os.chdir(tempdir)
        return None, 1

    # Process Data Type
    print('\nparse_data_type:')
    processed_vfr_list = processed_vfr_dict.keys()
    parse_data_type(processed_vfr_list, structs, header_master_list)

    # Process Default values for duplicate items >>>
    print('\nvalidate_structs:')
    error_count = 0
    warning_count = 0
    new_error_count = 0
    new_warning_count = 0
    while True:
        # Evaluate suppressif, grayoutif, disableif statements and re-parse
        # until no more pending value can be determined in a parsing
        error_count, warning_count = validate_structs(structs, verbose=0)
        if (error_count == 0) and (warning_count == 0):
            print('  error_count = %d, warning_count = %d' % (error_count, warning_count))
            break
        parse_default_value(processed_vfr_dict, root, structs, repeated_items)

        new_error_count, new_warning_count = validate_structs(structs, verbose=1)
        print(
            f'  previous_error_count = {error_count}, '
            f'previous_warning_count = {warning_count}, '
            f'error_count = {new_error_count}, '
            f'warning_count = {new_warning_count}'
        )
        if ((new_error_count + new_warning_count) == 0) or (
            (error_count == new_error_count) and (warning_count == new_warning_count)
        ):
            break

    # Normalize Enum names and Enum Value names
    for enum in enums_element.findall('Enum'):
        # Normalize enum name
        enum_name = enum.get('name')
        normalized_enum_name = normalize_name(enum_name)
        if normalized_enum_name != enum_name:
            enum.set('name', normalized_enum_name)

        # Normalize enum value names
        for enum_value in enum.findall('Value'):
            enum_value_name = enum_value.get('name')
            if enum_value_name:
                normalized_enum_value_name = normalize_name(enum_value_name)
                if normalized_enum_value_name != enum_value_name:
                    enum_value.set('help', enum_value_name)  # Keep the original name in help
                    enum_value.set('name', normalized_enum_value_name)

    # Validate Enums as well as Structs
    print('\nvalidate_enums:')
    error_count = validate_enums(enums_element, patch_mode=0, verbose=1)
    print('  error_count = %d' % (error_count))

    # Normalize member names before generating xml file
    #   If there are same pretty names under the same struct, differentiate them by member_name
    #   If Enum is available, update member type and member default accordingly
    print('\nNormalize member names:')
    pretty_name_dict = {}
    for struct in structs_element.findall('Struct'):
        struct_name = struct.get('name')
        pretty_name_dict.clear()
        for member in struct.findall('Member'):
            member_name = member.get('name')
            pretty_name = member.get('prettyname')
            member_default = member.get('default')
            member_type = member.get('type')

            # Collect pretty_name_dict
            if pretty_name not in pretty_name_dict:
                pretty_name_dict[pretty_name] = []
            pretty_name_dict[pretty_name].append(member_name)

            # Normalize member_name
            normalized_member_name = normalize_name(member_name)
            if normalized_member_name != member_name:
                member.set('name', normalized_member_name)
                # print(f'  member_name={member_name}, normalized_member_name={normalized_member_name}')

            # Update member type and member default if Enum is available
            look_up_enum_name = xml_get_enum_name(struct_name, member_name)
            enum_found = enums_element.find(f'Enum[@name="{look_up_enum_name}"]')
            if enum_found is not None:
                # If member type is a large number, skip it to avoid the VariableList error with struct.pack("<i",...)
                if member_type in ['uint64_t', 'int64_t']:
                    print(
                        '  Skipped updating Enum type for oneof option: '
                        f'member_name={member_name}, member_type={member_type}'
                    )
                    xml_remove_enum(enums_element, look_up_enum_name)

                # If default value is not assigned, just update type
                elif not member_default:
                    member.set('type', look_up_enum_name)

                else:
                    loop_up_enum_value_name = xml_get_enum_value_name(enums_element, look_up_enum_name, member_default)
                    if loop_up_enum_value_name is not None:
                        # If default value is assigned and found corresponding enum value, update type and default
                        # else if default value is assigned but not found corresponding enum value, skip it
                        member.set('type', look_up_enum_name)
                        member.set('default', loop_up_enum_value_name)

        # Patch pretty_name according to pretty_name_dict
        for pretty_name in pretty_name_dict:
            if len(pretty_name_dict[pretty_name]) > 1:
                for member_name in pretty_name_dict[pretty_name]:
                    member = struct.find(f'Member[@name="{normalize_name(member_name)}"]')
                    if member is not None:
                        if pretty_name == ' ':
                            member.set('prettyname', f'{member_name}')
                        else:
                            member.set('prettyname', f'{member_name}: {pretty_name}')

    # Generate Knobs namespace
    # Add comment to indicate the GUID namespace
    root.append(ET.Comment(' namespace indicates the GUID namespace the values are stored in '))

    # Use the fixed GUID for now
    knobs_guid = uuid.UUID('DE20F1E8-A9EB-4473-802F-31BC30D121D9')
    guid_upper = str(knobs_guid).upper()
    knobs_element = ET.SubElement(root, 'Knobs', attrib={
        'namespace': f'{{{guid_upper}}}'
    })

    # Generate GUID breakdown comment
    guid_parts = knobs_guid.hex
    formatted_guid = (
        f' {{ 0x{guid_parts[:8]}, 0x{guid_parts[8:12]}, 0x{guid_parts[12:16]}, '
        f'{{{", ".join(f"0x{guid_parts[i:i+2]}" for i in range(16, 32, 2))}}} }} '
    )
    knobs_element.append(ET.Comment(formatted_guid))

    # Create Knob elements
    for struct_name in structs:
        knob_name = xml_struct_to_knob_name(struct_name)
        ET.SubElement(knobs_element, 'Knob', attrib={
            'name': knob_name,
            'type': struct_name
        })

    # Generate the XML string and output to a file
    xml_root_to_string(root, output_xml_file_path, verbose=1)
    os.chdir(tempdir)
    return root, 0


# GUI based on ConfigEditor application
class vfrxml_app(ConfigEditor.application):
    def __init__(self, master=None):
        temp_argv = sys.argv
        sys.argv = sys.argv[:1]  # Patch sys.argv to avoid unnecessary errors from ConfigEditor
        super().__init__(master)
        sys.argv = temp_argv  # Restore sys.argv

        # Hide self.canvas at the right bottom of status bar, which is not necessary to this program
        if hasattr(self, 'canvas'):
            self.hide_item(self.canvas)

        # Input/Output Defaults
        if srcdir and os.path.isdir(srcdir):
            os.chdir(srcdir)
        self.inf_dict = input_file_name_dict
        self.platform_build_py = input_platform_build_py
        self.build_report = input_build_report
        self.output_file_path = output_file_path
        self.export_file_path = export_file_path
        self.update_inf_dict()
        self.display_inputs()

        # XML
        self.xml_root = None
        self.ref_cfg_path = input_ref_xml
        if self.ref_cfg_path and os.path.isfile(self.ref_cfg_path):
            self.print_msg('Reference XML: %s' % self.ref_cfg_path)
            self.ref_cfg_root = xml_get_root_from_file(self.ref_cfg_path)
        else:
            self.ref_cfg_root = None

        # Checkbox states {<checkbox id>: <checkbox state>}
        self.checkbox_state = {}
        self.checkbox_default = False

        # Highlight items {<cname>: <background>}
        self.highlight_items = {}

        # menubar and File Menu
        menubar = tkinter.Menu(root)
        self.vfrxml_menu = tkinter.Menu(menubar, tearoff=0)
        self.vfrxml_menu.add_command(label='Open INF File...', command=self.open_inf_file)
        self.vfrxml_menu.add_command(label='Open PlatformBuild.py...', command=self.open_platform_build)
        self.vfrxml_menu.add_command(label='Open vfrpp_resp File...', command=self.open_vfrpp_resp)
        self.vfrxml_menu.add_command(label='Process INF File', command=self.process_inf)
        self.vfrxml_menu.add_command(label='Export Config to XML', command=self.export_config_to_xml)
        self.vfrxml_menu.add_separator()
        self.vfrxml_menu.add_command(label='Load a reference PlatformCfgData.xml...', command=self.load_ref_cfg_xml)
        self.vfrxml_menu.add_command(label='Compare current config with reference xml', command=self.compare_cfg_xml)
        self.vfrxml_menu.add_separator()
        # self.vfrxml_menu.add_command(label='Open Build Report File...', command=self.open_build_report)
        self.vfrxml_menu.add_command(label='Debug Dump', command=self.debug_dump)
        self.vfrxml_menu.add_command(label="About", command=self.about)
        menubar.add_cascade(label="File", menu=self.vfrxml_menu)
        root.config(menu=menubar)
        self.refresh_vfrxml_menu()

    # Check if there is any inf file available in self.inf_dict
    # Return True if there is inf file available, else return False
    def check_inf_dict(self):
        found = False
        for inf_file in self.inf_dict.keys():
            if os.path.isfile(inf_file):
                found = True
                break
        return found

    # Update inf_dict by automatically finding vfrpp_resp file
    def update_inf_dict(self):
        for inf_file, vfrpp_resp_file in self.inf_dict.items():
            if not vfrpp_resp_file:
                vfrpp_resp_file = find_vfrpp_resp(inf_file, srcdir)  # Return '' if not found
                self.inf_dict[inf_file] = vfrpp_resp_file

    # Show input files
    def display_inputs(self):
        # INF File and vfrpp_resp
        for inf_file, vfrpp_resp_file in self.inf_dict.items():
            if os.path.isfile(inf_file):
                self.print_msg('INF File: %s' % inf_file)
                if vfrpp_resp_file and os.path.isfile(vfrpp_resp_file):
                    self.print_msg('VFRPP_RESP: %s' % vfrpp_resp_file)

        # PlatformBuild.py
        if self.platform_build_py and os.path.isfile(self.platform_build_py):
            self.print_msg('PlatformBuild File: %s' % self.platform_build_py)

        # Build Report
        if self.build_report and os.path.isfile(self.build_report):
            self.print_msg('BUILD REPORT: %s' % self.build_report)

    # Check whether the data has been loaded to page list
    def check_page(self):
        if len(self.page_cfg_map) > 0:
            return True
        else:
            return False

    # Refresh vfrxml_file_menu
    def refresh_vfrxml_menu(self):
        # Choose INF file prior to vfrpp_resp file
        if self.check_inf_dict():
            inf_dict_state = 'normal'
        else:
            inf_dict_state = 'disabled'
        self.vfrxml_menu.entryconfig("Open vfrpp_resp File...", state=inf_dict_state)

        # Choose INF file and PlatformBuild.py prior to executing Process INF File
        if not platform_build_py_required or (self.platform_build_py and os.path.isfile(self.platform_build_py)):
            self.vfrxml_menu.entryconfig("Process INF File", state=inf_dict_state)
        else:
            self.vfrxml_menu.entryconfig("Process INF File", state='disabled')

        # Menu items depend on page data availability
        if self.check_page():
            page_state = 'normal'
        else:
            page_state = 'disabled'
        self.vfrxml_menu.entryconfig("Export Config to XML", state=page_state)

        # Validate page data and reference xml availability before comparing them
        if self.check_page() and self.ref_cfg_root is not None:
            self.vfrxml_menu.entryconfig("Compare current config with reference xml", state='normal')
        else:
            self.vfrxml_menu.entryconfig("Compare current config with reference xml", state='disabled')

    # Hide an item no matter it is mapped by pack, grid, or place
    def hide_item(self, widget):
        widget_winfo_manager = widget.winfo_manager()
        if widget_winfo_manager == 'place':
            widget.place_forget()
        elif widget_winfo_manager == 'pack':
            widget.pack_forget()
        elif widget_winfo_manager == 'grid':
            widget.grid_forget()
        else:
            print(f'widget.winfo_manager() = {widget.winfo_manager()}')

    # Output message to the status bar and as well as console
    def print_msg(self, msg):
        print(msg)
        if hasattr(self, 'output_current_status'):
            self.output_current_status(msg)

    # File -> Open INF File...
    # Select input inf file
    def open_inf_file(self):
        file_path = os.path.normpath(filedialog.askopenfilename(
            filetypes=(("INF files", "*.inf"), ("all files", "*.*"))))
        if file_path and os.path.isfile(file_path):
            self.print_msg('INF FILE: %s' % file_path)
            self.inf_dict.clear()
            # Auto find vfrpp_resp. If necessary, select vfrpp_resp again after selecting inf file
            self.inf_dict[file_path] = find_vfrpp_resp(file_path, srcdir)
            if self.inf_dict[file_path] and os.path.isfile(self.inf_dict[file_path]):
                self.print_msg('VFRPP_RESP: %s' % self.inf_dict[file_path])
            self.refresh_vfrxml_menu()
        # print(f'file_path = {file_path}')
        # print_dict(self.inf_dict, 'self.inf_dict', 2)

    # File -> Open PlatformBuild.py...
    # Select input PlatformBuild.py
    def open_platform_build(self):
        file_path = os.path.normpath(filedialog.askopenfilename(
            filetypes=(("PlatformBuild", "PlatformBuild*.py"), ("all files", "*.*"))))
        if file_path and os.path.isfile(file_path):
            self.print_msg('PlatformBuild File: %s' % file_path)
            self.platform_build_py = file_path
            self.refresh_vfrxml_menu()

    # File -> Open vfrpp_resp File...
    # Select input vfrpp_resp file
    def open_vfrpp_resp(self):
        file_path = os.path.normpath(filedialog.askopenfilename(
            filetypes=(("vfrpp_resp", "*vfrpp_resp.*"), ("All files", "*.*"))))
        if file_path and os.path.isfile(file_path):
            self.print_msg('VFRPP_RESP: %s' % file_path)
            for key in self.inf_dict.keys():
                self.inf_dict[key] = file_path

    # File -> Process INF File
    # Parse inf files to get vfr files, and convert vfr files to xml
    def process_inf(self):
        time_start = time.time()
        self.xml_root, ret = parse_inf_to_xml(
            self.inf_dict,
            self.platform_build_py,
            self.output_file_path,
            self.build_report
        )
        print_time(time_start, 'parse_inf_to_xml is done. ')

        # Clean up temporary files in workspace
        for item in clean_up_list:
            clean_up_workspace(item)

        # Load output xml file to GUI
        if not ret:
            print('Loading output xml file to Treeview by ConfigEditor...')
            file_id = len(self.cfg_data_list)
            self.load_cfg_file(self.output_file_path, file_id, True)
            print_time(time_start, 'load_cfg_file is done. ')
            self.refresh_vfrxml_menu()

    # File -> Export Config to XML
    def export_config_to_xml(self):
        self.update_config_data_on_page()
        # file_path = filedialog.asksaveasfilename(filetypes=(("XML files", "*.xml"), ("all files", "*.*")))
        file_path = self.export_file_path
        if file_path:
            self.export_file_path = os.path.normpath(file_path)

            # Make a copy of self.xml_root
            export_xml_root = ET.fromstring(ET.tostring(self.xml_root))

            # Update Enums under export_xml_root according to self.checkbox_state
            enums_element = export_xml_root.find('./Enums')

            # Update struct members under export_xml_root according to self.checkbox_state
            knobs_element = export_xml_root.find('./Knobs')
            knob_list = export_xml_root.findall('./Knobs/Knob')

            # Prepare a dictionary from Knobs namespace {<Knob type>: <Knob name>}
            knobs_namespace_dict = xml_get_knobs_namespace_dict(export_xml_root, dict_type='TypeToName')

            # Iterate through structs and update struct members according to self.checkbox_state
            for structs in export_xml_root.findall('./Structs'):
                for struct in structs.findall('./Struct'):
                    any_member_checked = False
                    # Note:
                    #   In XML: <struct name="{struct_name}"...> <Member name="{member_name}...">
                    #   In self.checkbox_state: item.get('cname') = "{knob_name}.{member_name}"
                    struct_name = struct.get('name')
                    knob_name = knobs_namespace_dict.get(struct_name, struct_name)

                    # Iterate over members of the struct
                    # Use list() to avoid iterator issues during removal
                    for member in list(struct.findall('./Member')):
                        member_name = member.get('name')
                        cname = f'{knob_name}.{member_name}'
                        if cname in self.checkbox_state:
                            member_checked = self.checkbox_state[cname].get()
                        else:
                            member_checked = self.checkbox_default

                        # Remove member if checkbox is not checked
                        if not member_checked:
                            struct.remove(member)
                            # Remove corresponding Enum
                            xml_remove_enum(enums_element, xml_get_enum_name(struct_name, member_name))
                        else:
                            any_member_checked = True

                    # Remove empty struct and corresponding knob
                    if not any_member_checked:
                        # Remove struct if there is no member
                        structs.remove(struct)

                        # Remove corresponding knob
                        for knob in knob_list:
                            if knob.get('name') == knob_name:
                                knobs_element.remove(knob)

            # Generate the XML string and output to a file
            xml_root_to_string(export_xml_root, self.export_file_path, verbose=0)
            self.print_msg(f'Exported Config File: {self.export_file_path}')

    # File -> Load a reference PlatformCfgData.xml...
    def load_ref_cfg_xml(self):
        file_path = os.path.normpath(filedialog.askopenfilename(
            filetypes=(("XML files", "*.xml"), ("All files", "*.*"))))
        if file_path and os.path.isfile(file_path):
            self.ref_cfg_path = file_path
            self.ref_cfg_root = xml_get_root_from_file(self.ref_cfg_path)
            self.print_msg(f'Loaded reference Config File: {self.ref_cfg_path}')
            self.refresh_vfrxml_menu()

    # File -> Compare current config with reference xml
    def compare_cfg_xml(self):
        self.update_config_data_on_page()
        self.reset_widget_backgrounds()
        struct_diff_count, enum_diff_count, diff_cname_list, match_cname_list = xml_ref_compare(
            self.xml_root,
            self.ref_cfg_root
        )

        # Highlight the different items
        for cname in diff_cname_list:
            self.highlight_items[cname] = 'yellow'

        # Status bar output
        if hasattr(self, 'output_current_status'):
            self.output_current_status('Check the detailed comparison result in the console window.')
            self.output_current_status(f'  Number of differences in Structs: {struct_diff_count}')
            self.output_current_status(f'  Number of differences in Enums: {enum_diff_count}\n')

    # File -> Open Build Report File...
    # Select input build report file
    def open_build_report(self):
        file_path = os.path.normpath(filedialog.askopenfilename(
            filetypes=(("Log files", "*.txt;*.log"), ("All files", "*.*"))))
        if file_path and os.path.isfile(file_path):
            self.print_msg('BUILD REPORT: %s' % file_path)
            self.build_report = file_path
        # print(f'file_path = {file_path}, self.build_report = {self.build_report}')

    # Debug function
    def debug_dump(self):
        with open(debug_file_path, 'w') as f:
            f.write('######## Debug Dump Start ########\n')
            f.write(f'self.inf_dict = {self.inf_dict}\n')
            f.write(f'self.page_id = {self.page_id}\n')
            f.write(f'self.page_list = {self.page_list}\n')
            f.write(f'self.conf_list = {self.conf_list}\n')
            f.write(f'self.in_left = {self.in_left}\n')
            f.write(f'self.in_right = {self.in_right}\n')
            f.write(f'self.cfg_data_list = {self.cfg_data_list}\n')
            f.write(f'self.page_cfg_map = {self.page_cfg_map}\n')
            f.write(f'self.check_page() = {self.check_page()}\n')
            f.write('######## Debug Dump End ########\n')
        self.print_msg('Debug file saved to %s' % debug_file_path)

    # Override from ConfigEditor >>>
    #  Also clear highlighted items when reset_widget_backgrounds
    def reset_widget_backgrounds(self):
        if hasattr(super(), 'reset_widget_backgrounds'):
            super().reset_widget_backgrounds()
        self.highlight_items.clear()

    # File -> About
    def about(self):
        msg = (
            'VFR to XML Converter\n--------------------------------\n'
            f'Version {this_version}\n{time.localtime().tm_year}'
        )
        lines = msg.split("\n")
        width = 30
        text = []
        for line in lines:
            text.append(line.center(width, " "))
        messagebox.showinfo(os.path.basename(os.path.abspath(__file__)), "\n".join(text))

    # Update modified value to self.xml_root as well
    def set_config_item_value(self, item, value_str, file_id):
        if hasattr(super(), 'set_config_item_value'):
            super().set_config_item_value(item, value_str, file_id)

            # Prepare a dictionary from Knobs namespace {<Knob name>: <Knob type>}
            knobs_namespace_dict = xml_get_knobs_namespace_dict(self.xml_root, dict_type='NameToType')
            varid_split = item.get('cname').split('.')
            knob_name = varid_split[0]
            struct_name = knobs_namespace_dict.get(knob_name, knob_name)
            member_name = '.'.join(varid_split[1:])

            # Update item value to self.xml_root
            for structs in self.xml_root.findall('./Structs'):
                struct = structs.find(f"./Struct[@name='{struct_name}']")
                if struct is not None:
                    member = struct.find(f"./Member[@name='{member_name}']")
                    if member is not None:
                        member_value = member.get('default')
                        if member_value != value_str:
                            member.set('default', value_str)
                    else:
                        print(f'item_cname = {item.get("cname")}, value_str = {value_str}')
                        print(f'member_name: {member_name} is not found in struct: {struct_name}')
                        print()
                else:
                    print(f'item_cname = {item.get("cname")}, value_str = {value_str}')
                    print(f'struct_name: {struct_name} is not found')
                    print()

    # Add checkbox for each item
    def add_config_item(self, item, row, file_id):
        if hasattr(super(), 'add_config_item'):
            super().add_config_item(item, row, file_id)

            def is_name(child):
                return isinstance(child, tkinter.Label) and (child.cget('text') == item.get('name'))
            parent = self.right_grid

            # Get name and widget of this item and move them to padx=25
            last_name = parent.winfo_children()[-1]
            if not is_name(last_name):
                last_name = parent.winfo_children()[-2]
                last_widget = parent.winfo_children()[-1]
                last_widget.grid(row=row+1, column=0, padx=25, pady=5, sticky="nsew")
            last_name.grid(row=row, column=0, padx=25, pady=5, sticky="nsew")

            # Add Checkbox for each item
            item_type = item.get('type')
            item_cname = item.get('cname')
            if not item_type == 'STRUCT_KNOB':
                if not self.checkbox_state.get(item_cname):
                    self.checkbox_state[item_cname] = tkinter.BooleanVar()
                    self.checkbox_state[item_cname].set(0)
                cb = tkinter.Checkbutton(parent, variable=self.checkbox_state[item_cname], onvalue=1, offvalue=0)
                cb.grid(row=row, column=0, sticky="w")

            # Update highlight color
            bg_color = self.highlight_items.get(item_cname)
            if bg_color:
                last_name.configure(background=bg_color)
    # Override from ConfigEditor <<<


def ProcessArgs():
    argParser = argparse.ArgumentParser(description='VFR to XML Converter Version %s' % this_version)
    # Specify input/output files
    argParser.add_argument('-inf', '--inf_file', help='UEFI INF file including VFR file(s) in [Sources]')
    argParser.add_argument(
        '-resp', '--vfrpp_resp',
        help='vfrpp_resp file including additional defines for preprocessing macros in vfr files'
    )
    argParser.add_argument('-pb', '--platform_build', help='PlatformBuild.py file path')
    argParser.add_argument(
        '-br', '--build_report',
        help='Build Report file in Build folder, which is for preprocessing PCDs in vfr files'
    )
    argParser.add_argument('-ref', '--ref_xml', help='Reference Config XML file to be compared')
    argParser.add_argument('-o', '--output_xml', help='Output Config XML file to be generated')
    # CLI options
    argParser.add_argument(
        '-cli', '--command_line',
        help='Not to launch GUI. Either --inf_file or --compare_xml will be required along with this option',
        action="store_true")
    argParser.add_argument(
        '-cx', '--compare_xml', nargs=2,
        metavar=('current_xml', 'ref_xml'),
        help='Compare Config XML files. This argument only works when --command_line is specified'
    )
    argParser.add_argument(
        '-vx', '--verify_xml', nargs=3,
        metavar=('output.xml', 'output_prev.xml', 'PlatformCfgData.xml'),
        help='Verify Config XML files to notify Reference code changes related to Platform porting. '
        'This argument only works when --command_line is specified'
    )
    args = argParser.parse_args()
    return args


if __name__ == '__main__':

    # print('%s debug log. Python version = %s' % (os.path.basename(sys.argv[0]), sys.version))
    # print(sys.argv)

    # Process Arguments
    args = ProcessArgs()

    # INF and VFRPP_RESP
    if args.inf_file and os.path.isfile(args.inf_file):
        input_file_name_dict.clear()
        if args.vfrpp_resp and os.path.isfile(args.vfrpp_resp):
            input_file_name_dict[args.inf_file] = args.vfrpp_resp
        else:
            input_file_name_dict[args.inf_file] = ''

    # PlatformBuild.py
    if args.platform_build and os.path.isfile(args.platform_build):
        input_platform_build_py = args.platform_build

    # Build Report
    if args.build_report and os.path.isfile(args.build_report):
        input_build_report = args.build_report

    # Reference XML
    if args.ref_xml and os.path.isfile(args.ref_xml):
        input_ref_xml = args.ref_xml

    # output.xml
    if args.output_xml:
        output_file_path = args.output_xml

    # CLI
    if args.command_line:
        ret = 0
        restore_dir = None
        if srcdir and os.path.isdir(srcdir):
            restore_dir = os.getcwd()
            os.chdir(srcdir)

        # Display input files
        inf_ready = False
        for inf_file, vfrpp_resp_file in input_file_name_dict.items():
            if os.path.isfile(inf_file):
                print('INF File: %s' % inf_file)
                inf_ready = True
                if vfrpp_resp_file and os.path.isfile(vfrpp_resp_file):
                    print('VFRPP_RESP: %s' % vfrpp_resp_file)
                else:
                    vfrpp_resp_file = find_vfrpp_resp(inf_file, srcdir)
                    if vfrpp_resp_file and os.path.isfile(vfrpp_resp_file):
                        print('VFRPP_RESP: %s' % vfrpp_resp_file)
                        input_file_name_dict[inf_file] = vfrpp_resp_file

        if input_platform_build_py and os.path.isfile(input_platform_build_py):
            print('PlatformBuild.py: %s' % input_platform_build_py)

        if input_build_report and os.path.isfile(input_build_report):
            print('BUILD REPORT: %s' % input_build_report)

        if input_ref_xml and os.path.isfile(input_ref_xml):
            print('Reference XML: %s' % input_ref_xml)

        # Execute XML comparison or INF/VFR convertion in CLI
        if args.compare_xml:
            # Compare XML
            if args.compare_xml[0] and os.path.isfile(args.compare_xml[0]):
                if args.compare_xml[1] and os.path.isfile(args.compare_xml[1]):
                    print('Comparing Config XML files: %s and %s' % (args.compare_xml[0], args.compare_xml[1]))
                    new_xml_root = xml_get_root_from_file(args.compare_xml[0])
                    old_xml_root = xml_get_root_from_file(args.compare_xml[1])
                    struct_diff_count, enum_diff_count, diff_cname_list, match_cname_list = xml_ref_compare(
                        new_xml_root,
                        old_xml_root
                    )
                    ret = struct_diff_count + enum_diff_count
                else:
                    print('File not found: %s' % args.compare_xml[1])
                    ret = 1
            else:
                print('File not found: %s' % args.compare_xml[0])
                ret = 1

        elif args.verify_xml:
            # Process input files
            for xml in args.verify_xml:
                if not xml or not os.path.isfile(xml):
                    print('File not found: %s' % xml)
                    ret += 1
            if not ret:
                print('Verifying Config XML files:')
                print(f'  Current RC: {args.verify_xml[0]}')
                print(f'  Previous RC: {args.verify_xml[1]}')
                print(f'  Platform Config: {args.verify_xml[2]}')
                curr_xml_root = xml_get_root_from_file(args.verify_xml[0])
                prev_xml_root = xml_get_root_from_file(args.verify_xml[1])
                plat_xml_root = xml_get_root_from_file(args.verify_xml[2])

                # Get Platform porting cname lists, and check if there is anything removed
                # in recent Reference Code changes
                _, _, _, prev_plat_cname_list = xml_ref_compare(prev_xml_root, plat_xml_root, verbose=0)
                _, _, _, curr_plat_cname_list = xml_ref_compare(curr_xml_root, plat_xml_root, verbose=0)
                plat_remove_count = 0
                for prev_plat_cname in prev_plat_cname_list:
                    if prev_plat_cname not in curr_plat_cname_list:
                        plat_remove_count += 1
                        print(
                            f'==> Item \"{prev_plat_cname}\" is removed or renamed '
                            'in recent Reference Code changes\n'
                        )

                # Compare Current and Previous XML from Reference Code,
                # but only cares about the items in plat_cname_list
                struct_diff_count, enum_diff_count, diff_cname_list, match_cname_list = xml_ref_compare(
                    curr_xml_root,
                    prev_xml_root,
                    prev_plat_cname_list
                )
                ret = plat_remove_count + struct_diff_count + enum_diff_count

        elif inf_ready:
            # Convert INF/VFR to XML
            time_start = time.time()
            xml_root, ret = parse_inf_to_xml(
                input_file_name_dict, input_platform_build_py, output_file_path, input_build_report
            )
            print_time(time_start, 'parse_inf_to_xml is done. ')
            # Clean up temporary files in workspace
            for item in clean_up_list:
                clean_up_workspace(item)

        elif args.inf_file:
            print('INF File not found: %s' % args.inf_file)
            ret = 1

        else:
            print(
                'Error: One of the arguments --inf_file, --compare_xml or --verify_xml '
                'is required in CLI mode. Check --help for more details.'
            )
            ret = 1

        if restore_dir and os.path.isdir(restore_dir):
            os.chdir(restore_dir)
        exit(ret)

    # GUI
    else:
        root = tkinter.Tk()
        app = vfrxml_app(master=root)
        root.title('VFR to XML Converter')
        root.mainloop()
