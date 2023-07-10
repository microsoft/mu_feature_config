# @file
#
# Cert Support functions
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

##
## Obtain the SHA1 value from a .pfx file
##

import os
import traceback
import logging
import tempfile

from DFCI_SupportLib import DFCI_SupportLib
from edk2toollib.utility_functions import RunCmd

CertMgrPath = None


class CertSupportLib(object):
    def get_thumbprint_from_pfx(self, pfxfilename=None):
        global CertMgrPath

        if pfxfilename is None:
            raise Exception("Pfx File Name is required")
        fp = tempfile.NamedTemporaryFile(delete=False)

        t_file = fp.name
        fp.close()

        try:
            #
            # Cert Manager is used for deleting the cert when add/removing certs
            #

            # 1 - use Certmgr to get the PFX sha1 thumbprint
            if CertMgrPath is None:
                CertMgrPath = DFCI_SupportLib().get_certmgr_path()

            parameters = " /c " + pfxfilename
            ret = RunCmd(CertMgrPath, parameters, outfile=t_file)
            if ret != 0:
                logging.critical(
                    "Failed to get cert info from Pfx file using CertMgr.exe"
                )
                return ret
            f = open(t_file, "r")

            pfx_details = f.readlines()
            f.close()
            os.remove(t_file)
            # 2 Parse the pfx_details for the sha1 thumbprint
            thumbprint = ""
            found = False
            for a in pfx_details:
                a = a.strip()
                if len(a):
                    if found:
                        thumbprint = "".join(a.split())
                        break
                    else:
                        if a == "SHA1 Thumbprint::":
                            found = True

            if (len(thumbprint) != 40) or (found is False):
                return "No thumbprint"

        except Exception:
            traceback.print_exc()
            return "Unable to read certificate"

        return thumbprint
