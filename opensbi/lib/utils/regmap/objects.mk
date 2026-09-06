# SPDX-License-Identifier: BSD-2-Clause

libsbiutils-objs-$(CONFIG_FDT_REGMAP) += regmap/fdt_regmap.o
libsbiutils-objs-$(CONFIG_FDT_REGMAP) += regmap/fdt_regmap_drivers.o

carray-fdt_regmap_drivers-$(CONFIG_FDT_REGMAP_SYSCON) += fdt_regmap_syscon
libsbiutils-objs-$(CONFIG_FDT_REGMAP_SYSCON) += regmap/fdt_regmap_syscon.o

libsbiutils-objs-$(CONFIG_REGMAP) += regmap/regmap.o
