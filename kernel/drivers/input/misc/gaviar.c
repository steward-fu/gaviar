/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
 
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>

#define VBR0 39
#define VBR1 38

static ssize_t vbr0_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int rc = -1;
    unsigned long v = 0;
 
    rc = kstrtoul(buf, 0, &v);
    if (rc) {
        return rc;
    }
    gpio_set_value(VBR0, !!v);
 
    return count;
}
static DEVICE_ATTR_WO(vbr0);

static ssize_t vbr1_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int rc = -1;
    unsigned long v = 0;
 
    rc = kstrtoul(buf, 0, &v);
    if (rc) {
        return rc;
    }
    gpio_set_value(VBR1, !!v);
 
    return count;
}
static DEVICE_ATTR_WO(vbr1);

static int vbr_probe(struct platform_device *pdev)
{
    gpio_request(VBR0, "MO0");
    gpio_request(VBR1, "MO1");
    gpio_direction_output(VBR0, 0);
    gpio_direction_output(VBR1, 0);
    gpio_set_value(VBR0, 0);
    gpio_set_value(VBR1, 0);

    device_create_file(&pdev->dev, &dev_attr_vbr0);
    device_create_file(&pdev->dev, &dev_attr_vbr1);

    return 0;
}

static int vbr_remove(struct platform_device *pdev)
{
    device_remove_file(&pdev->dev, &dev_attr_vbr1);
    device_remove_file(&pdev->dev, &dev_attr_vbr0);

    gpio_set_value(VBR0, 1);
    gpio_set_value(VBR1, 1);
    gpio_free(VBR1);
    gpio_free(VBR0);

    return 0;
}
 
static const struct of_device_id fb_of_match[] = { { .compatible = "allwinner,gaviar" }, {} };
MODULE_DEVICE_TABLE(of, fb_of_match);

static struct platform_driver vbr_driver = {
    .probe = vbr_probe,
    .remove = vbr_remove,
    .driver = {
        .name = "gaviar-vbr",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(fb_of_match),
    },
};

static int __init myvbr_init(void)
{
    return platform_driver_register(&vbr_driver);
}

static void __exit myvbr_cleanup(void)
{
    platform_driver_unregister(&vbr_driver);
}

module_init(myvbr_init);
module_exit(myvbr_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steward Fu <steward.fu@gmail.com>");
MODULE_DESCRIPTION("Vibration driver for Gaviar handheld");
