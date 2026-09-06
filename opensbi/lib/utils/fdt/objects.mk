# SPDX-License-Identifier: BSD-2-Clause

libsbiutils-objs-$(CONFIG_FDT_DOMAIN) += fdt/fdt_domain.o
libsbiutils-objs-$(CONFIG_FDT_PMU) += fdt/fdt_pmu.o
libsbiutils-objs-$(CONFIG_FDT) += fdt/fdt_helper.o
libsbiutils-objs-$(CONFIG_FDT) += fdt/fdt_fixup.o
