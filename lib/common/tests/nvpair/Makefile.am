#
# Copyright 2021-2025 the Pacemaker project contributors
#
# The version control history for this file may have further details.
#
# This source code is licensed under the GNU General Public License version 2
# or later (GPLv2+) WITHOUT ANY WARRANTY.
#

include $(top_srcdir)/mk/common.mk
include $(top_srcdir)/mk/tap.mk
include $(top_srcdir)/mk/unittest.mk

# Add "_test" to the end of all test program names to simplify .gitignore.
check_PROGRAMS = crm_meta_name_test		\
		 crm_meta_value_test		\
		 pcmk__cmp_nvpair_blocks_test	\
		 pcmk__scan_nvpair_test		\
		 pcmk__xe_attr_is_true_test 	\
		 pcmk__xe_get_bool_attr_test 	\
		 pcmk__xe_get_datetime_test	\
		 pcmk__xe_get_flags_test	\
		 pcmk__xe_set_bool_attr_test	\
		 pcmk__unpack_nvpair_block_test	\
		 pcmk_unpack_nvpair_blocks_test

TESTS = $(check_PROGRAMS)
