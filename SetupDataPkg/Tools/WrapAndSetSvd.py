# @file
#
# Build a JSON packet for the USB Refresh option
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent

##
##
## Convert each of the bin files to Base64 then add each of them to a JSON element.
##

import os, sys
import logging
import argparse

#get script path
sp = os.path.abspath(os.path.dirname(__file__))

#setup python path for build modules
sys.path.append(sp)

from edk2toollib.utility_functions import RunPythonScript
from SettingSupport.UefiVariablesSupportLib import UefiVariable

DFCI_SETTINGS_APPLY_INPUT_VAR_NAME    = "DfciSettingsRequest"
DFCI_SETTINGS_GUID                    = 'D41C8C24-3F5E-4EF4-8FDD-073E1866CD01'
DFCI_SECURED_SETTINGS_VAR_ATTRIBUTES  = 7 #(EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS)

# Setup import and argument parser
def path_parse():

    parser = argparse.ArgumentParser()

    parser.add_argument (
        '-i', '--InputSVDFile', dest = 'InputSVDFile', required = True, type=str,
        help = '''Specify the absolute path to SVD file intended to be set.'''
        )
    parser.add_argument (
        '-m', '--Manufacturer', dest = 'Manufacturer', type=str, default='""',
        help = '''Specify the your manufacturer name.'''
        )
    parser.add_argument (
        '-p', '--Product', dest = 'Product', type=str, default='""',
        help = '''Specify the your product name.'''
        )
    parser.add_argument (
        '-s', '--SerialNum', dest = 'SerialNum', type=str, default='""',
        help = '''Specify the your targeted serial number.'''
        )

    Paths = parser.parse_args()

    if not os.path.isfile (Paths.InputSVDFile):
      raise Exception ("Invalid input files %s" % Paths.InputSVDFile)

    return Paths

#
#main script function
#
def main():

  Paths = path_parse()

  setting_file = 'Unsigned_Settings_apply.bin'
  py_file = 'SettingSupport/GenerateSettingsPacketData.py'
  params = ['--Step1Enable']
  params += ['--PrepResultFile', setting_file]
  params += ['--XmlFilePath', Paths.InputSVDFile]
  params += ['--HdrVersion', '2']
  params += ["--SMBIOSMfg", Paths.Manufacturer]
  params += ["--SMBIOSProd", Paths.Product]
  params += ["--SMBIOSSerial", Paths.SerialNum]
  ret = RunPythonScript (py_file, ' '.join(params), workingdir=sp, environ=os.environ.copy())
  if ret != 0:
    raise Exception ('Failed to generate package - %x' % ret)

  with open (os.path.join(sp, setting_file), 'rb') as file:
    var = file.read()
    UefiVar = UefiVariable()
    (rc, err, errorstring) = UefiVar.SetUefiVar(DFCI_SETTINGS_APPLY_INPUT_VAR_NAME, DFCI_SETTINGS_GUID, var, DFCI_SECURED_SETTINGS_VAR_ATTRIBUTES)
    if rc == 0: # This is per function document...
      logging.error ('Failed to set mailbox settings into UEFI variable - %r' % errorstring)
      return 1
  return 0

if __name__ == '__main__':
    #setup main console as logger
    logger = logging.getLogger('')
    logger.setLevel(logging.DEBUG)
    formatter = logging.Formatter("%(levelname)s - %(message)s")
    console = logging.StreamHandler()
    console.setLevel(logging.CRITICAL)

    #call main worker function
    retcode = main()

    if retcode != 0:
        logging.critical("Failed.  Return Code: %i" % retcode)
    #end logging
    logging.shutdown()
    sys.exit(retcode)
