"""A module providing XML validation utilities."""

__copyright__ = "Copyright 2024 the Pacemaker project contributors"
__license__ = "GNU General Public License version 2 or later (GPLv2+)"

__all__ = ["validate"]

import os
import subprocess
import sys

from pacemaker._cts.errors import XmlValidationError
from pacemaker._cts.process import pipe_communicate
from pacemaker.buildoptions import BuildOptions


def find_validator(rng_file):
    """
    Return the command line used to validate XML output.

    If no validator is found, return None.
    """
    if os.access(BuildOptions.XMLLINT_PATH, os.X_OK):
        if rng_file is None:
            return [BuildOptions.XMLLINT_PATH, "-"]

        return [BuildOptions.XMLLINT_PATH, "--relaxng", rng_file, "-"]

    raise FileNotFoundError("Could not find validator for %s" % rng_file)


def rng_directory():
    """Return the directory containing RNG schema files."""
    if os.environ.get("PCMK_schema_directory", "") != "":
        return os.environ["PCMK_schema_directory"]

    for path in sys.path:
        if os.path.exists("%s/cts-fencing.in" % path):
            return "%s/xml" % path

    return BuildOptions.SCHEMA_DIR


def validate(xml, check_rng=True, verbose=False):
    """Validate the given XML input string against a schema."""
    if check_rng:
        rng_file = "%s/api/api-result.rng" % rng_directory()
    else:
        rng_file = None

    cmd = find_validator(rng_file)

    if verbose:
        print("\nRunning: %s" % " ".join(cmd))

    with subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as validator:
        output = pipe_communicate(validator, check_stderr=True, stdin=xml)

        if validator.returncode != 0:
            raise XmlValidationError(output, validator.returncode)

        return output