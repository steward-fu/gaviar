#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#include "debug.h"
#include "loop.h"
#include "mininit.h"

#define ROOTFS_EXT2     "rootfs.ext2"
#define ROOTFS_SQUASHFS "rootfs.squashfs"

int create_mount_point(const char *path)
{
    if (mkdir(path, 0755)) {
        if (errno != EEXIST) {
            WARNING("Failed to create '%s' mount point: %d\n", path, errno);
            return -1;
        }
    }
    return 0;
}

FILE *logfile;
char logbuf[LOG_BUF_SIZE];

int main(int argc, char **argv, char **envp)
{
    bool mount_ext2 = !access(ROOTFS_EXT2, R_OK);

    logfile = stderr;

    // Mount devtmpfs to get a full set of device nodes.
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) && errno != EBUSY) {
        INFO("Couldn't mount devtmpfs on /dev: %d\n", errno);
        // If there are sufficient static device nodes in the fs containing
        // mininit, we can boot without devtmpfs, so don't give up yet.
    }

    // Write our log messages to the kernel log.
    FILE *kmsg = fopen("/dev/kmsg", "w");
    if (kmsg) {
#ifndef __KLIBC__
        setlinebuf(kmsg);
#endif // __KLIBC__
        logfile = kmsg;
    }
    INFO("OpenDingux mininit 2.0.2\n");
    if (!kmsg) {
        WARNING("Failed to open '/dev/kmsg': %d\n", errno);
    }

    const char *boot_mount = mount_boot();
    if (!boot_mount) {
        return -1;
    }

    if (chdir(boot_mount)) {
        ERROR("Unable to change to '%s' directory: %d\n", boot_mount, errno);
        return -1;
    }

    // Get free loop device.
    int devnr = logetfree();
    if (devnr < 0) {
        // We're running early in the boot sequence, probably /dev/loop0
        // is still available.
        devnr = 0;
    }
    char loop_dev[9 + 10 + 1];
    sprintf(loop_dev, "/dev/loop%i", devnr);

    // Set the rootfs as the backing file for the loop device.
    const char *rootfs_img = mount_ext2 ? ROOTFS_EXT2 : ROOTFS_SQUASHFS;
    losetup(loop_dev, rootfs_img);

    // Mount the loop device that was just set up.
    DEBUG("Loop-mounting '%s' on '/root'\n", rootfs_img);
    if (mount(loop_dev, "/root", mount_ext2 ? "ext2" : "squashfs", MS_RDONLY, NULL)) {
        ERROR("Failed to mount the rootfs image: %d\n", errno);
        return -1;
    }
    INFO("%s mounted on /root\n", rootfs_img);

    // Make the freshly mounted rootfs image the working directory.
    if (chdir("/root")) {
        ERROR("Unable to change to '/root' directory: %d\n", errno);
        return -1;
    }

    // Move the devtmpfs mount to inside the rootfs tree.
    DEBUG("Moving '/dev' mount\n");
    if (mount("/dev", "dev", NULL, MS_MOVE, NULL)) {
        ERROR("Unable to move the '/dev' mount: %d\n", errno);
        return -1;
    }

    // Re-open the console device at the new location.
    int fd = open("dev/console", O_RDWR, 0);
    if (fd < 0) {
        ERROR("Unable to re-open console: %d\n", fd);
        return -1;
    }
    if (dup2(fd, 0) != 0 || dup2(fd, 1) != 1 || dup2(fd, 2) != 2) {
        ERROR("Unable to duplicate console handles\n");
        return -1;
    }
    if (fd > 2) close(fd);

    // Open the old root while we can still access it.
    fd = open_dir_to_clean();

    // Now let's switch to the new root
    if (switch_root()) {
        if (fd >= 0) close(fd);
        return -1;
    }

    // Make the freshly switched root the root of this process.
    if (chroot(".")) {
        ERROR("'chroot' to new root failed: %d\n", errno);
        if (fd >= 0) close(fd);
        return -1;
    }

    // And make it the working directory as well.
    if (chdir("/")) {
        ERROR("'chdir' to new root failed: %d\n", errno);
        if (fd >= 0) close(fd);
        return -1;
    }

    INFO("root switch done\n");

    // Clean up the initramfs and then release it.
    if (fd >= 0) {
        DEBUG("Removing initramfs contents\n");
        const char *executable = argv[0];
        while (*executable == '/') ++executable;
        if (unlinkat(fd, executable, 0)) {
            DEBUG("Failed to remove '%s' executable: %d\n", executable, errno);
        }
        if (unlinkat(fd, "dev/console", 0)) {
            DEBUG("Failed to remove '/dev/console': %d\n", errno);
        }
        if (unlinkat(fd, "dev", AT_REMOVEDIR)) {
            DEBUG("Failed to remove '/dev' directory: %d\n", errno);
        }
        if (unlinkat(fd, "boot", AT_REMOVEDIR)) {
            DEBUG("Failed to remove '/boot' mount point: %d\n", errno);
        }
        if (unlinkat(fd, "root", AT_REMOVEDIR)) {
            DEBUG("Failed to remove '/root' directory: %d\n", errno);
        }
        if (close(fd)) {
            DEBUG("Failed to close initramfs: %d\n", errno);
        }
    }

    // Try to locate the init program.
    const char *inits[] = {
        "/sbin/init",
        "/etc/init",
        "/bin/init",
        "/bin/sh",
        NULL,
    };
    for (int i = 0; ; i++) {
        if (!inits[i]) {
            ERROR("Unable to find the 'init' executable\n");
            return -1;
        }
        DEBUG("Checking for 'init' executable: %s\n", inits[i]);
        if (!access(inits[i], X_OK)) {
            argv[0] = (char *)inits[i];
            break;
        }
    }
    INFO("starting %s\n", argv[0]);

    if (kmsg) {
        logfile = stderr;
        fclose(kmsg);
    }

    // Execute the 'init' executable
    execvpe(argv[0], argv, envp);
    ERROR("Exec of 'init' failed: %d\n", errno);
    return -1;
}
