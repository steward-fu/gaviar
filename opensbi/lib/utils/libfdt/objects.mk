# SPDX-License-Identifier: BSD-2-Clause

libfdt_files = fdt.o fdt_addresses.o fdt_check.o fdt_empty_tree.o fdt_ro.o fdt_rw.o \
               fdt_strerror.o fdt_sw.o fdt_wip.o
$(foreach file, $(libfdt_files), \
        $(eval CFLAGS_$(file) = -I$(src)/../../utils/libfdt))

libsbiutils-objs-$(CONFIG_LIBFDT) += $(addprefix libfdt/,$(libfdt_files))
libsbiutils-genflags-y  += -I$(libsbiutils_dir)/libfdt/
