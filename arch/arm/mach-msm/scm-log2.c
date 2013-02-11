/*
 *
 * Copyright (C) 2009 HTC, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <asm/uaccess.h>

#include <mach/scm.h>
#include <mach/msm_iomap.h>

#define DEBUG_MAX_RW_BUF 4096
#define TZ_SCM_LOG_PHYS		MSM_TZLOG_PHYS
#define TZ_SCM_LOG_SIZE		MSM_TZLOG_SIZE
#define INT_SIZE        4

struct htc_tzlog_dev {
    char *buffer;
    int *pw_cursor;
    int *pr_cursor;
};

struct htc_tzlog_dev HTC_TZLOG;
struct htc_tzlog_dev *htc_tzlog = (struct htc_tzlog_dev *)&HTC_TZLOG;

enum tzdbg_stats_type {
    TZDBG_LOG = 0,
    TZDBG_STATS_MAX,
};

struct tzdbg_stat {
    char *name;
    char *data;
};

struct tzdbg {
    void __iomem *virt_iobase;
    struct tzdbg_t *diag_buf;
    char *disp_buf;
    int debug_tz[TZDBG_STATS_MAX];
    struct tzdbg_stat stat[TZDBG_STATS_MAX];
};

static struct tzdbg tzdbg = {
    .stat[TZDBG_LOG].name = "log",
};

static int _disp_tz_htc_log_stats(char __user *ubuf, size_t count, loff_t *offp)
{
    char *buf = htc_tzlog->buffer;
    int *pw_cursor = htc_tzlog->pw_cursor;
    int *pr_cursor = htc_tzlog->pr_cursor;
    int r_cursor, w_cursor, ret;

    if (buf != 0) {
        /* update r_cursor */
        r_cursor = *pr_cursor;
        w_cursor = *pw_cursor;

        if (r_cursor < w_cursor) {
            if ((w_cursor - r_cursor) > count) {
                ret = copy_to_user(ubuf, buf + r_cursor, count);
                if (ret == count)
                    return -EFAULT;

                *pr_cursor = r_cursor + count;
                return count;
            } else {
                ret = copy_to_user(ubuf, buf + r_cursor, (w_cursor - r_cursor));
                if (ret == (w_cursor - r_cursor))
                    return -EFAULT;

                *pr_cursor = w_cursor;
                return (w_cursor - r_cursor);
            }
        }

        if (r_cursor > w_cursor) {
            int buf_end = TZ_SCM_LOG_SIZE - 2*INT_SIZE - 1;
            int left_len = buf_end - r_cursor;

            if (left_len > count) {
                ret = copy_to_user(ubuf, buf + r_cursor, count);
                if (ret == count)
                    return -EFAULT;

                *pr_cursor = r_cursor + count;
                return count;
            } else {
                ret = copy_to_user(ubuf, buf + r_cursor, left_len);
                if (ret == left_len)
                    return -EFAULT;

                *pr_cursor = 0;
                return left_len;
            }
        }

        if (r_cursor == w_cursor) {
            pr_info("No New Trust Zone log\n");
            return 0;
        }
    }

    return 0;
}

static ssize_t tzdbgfs_read(struct file *file, char __user *buf,
    size_t count, loff_t *offp)
{
	return _disp_tz_htc_log_stats(buf, count, offp);
}


static int tzdbgfs_open(struct inode *inode, struct file *pfile)
{
    pfile->private_data = inode->i_private;
    return 0;
}

const struct file_operations tzdbg_fops = {
    .owner   = THIS_MODULE,
    .read    = tzdbgfs_read,
    .open    = tzdbgfs_open,
};

static int  tzdbgfs_init(struct platform_device *pdev)
{
    int rc = 0;
    int i;
    struct dentry *dent_dir;
    struct dentry *dent;

    dent_dir = debugfs_create_dir("tzdbg", NULL);
    if (dent_dir == NULL) {
        dev_err(&pdev->dev, "tzdbg debugfs_create_dir failed\n");
        return -ENOMEM;
    }

    for (i = 0; i < TZDBG_STATS_MAX; i++) {
        tzdbg.debug_tz[i] = i;
        dent = debugfs_create_file(tzdbg.stat[i].name,
                S_IRUGO, dent_dir,
                &tzdbg.debug_tz[i], &tzdbg_fops);
        if (dent == NULL) {
            dev_err(&pdev->dev, "TZ debugfs_create_file failed\n");
            rc = -ENOMEM;
            goto err;
        }
    }
    tzdbg.disp_buf = kzalloc(DEBUG_MAX_RW_BUF, GFP_KERNEL);
    if (tzdbg.disp_buf == NULL) {
        pr_err("%s: Can't Allocate memory for tzdbg.disp_buf\n",
            __func__);
        goto err;
    }
    platform_set_drvdata(pdev, dent_dir);
    return 0;
err:
    debugfs_remove_recursive(dent_dir);

    return rc;
}

static int scm_log_probe(struct platform_device *pdev)
{
    htc_tzlog->buffer = devm_ioremap_nocache(&pdev->dev,
        TZ_SCM_LOG_PHYS, TZ_SCM_LOG_SIZE);
    if (htc_tzlog->buffer == NULL) {
        pr_err("%s: ioremap fail...\n", __func__);
        kfree(htc_tzlog);
        return -EFAULT;
    }

    htc_tzlog->pr_cursor = (int *)((int)(htc_tzlog->buffer) +
                 TZ_SCM_LOG_SIZE - 2 * INT_SIZE);
    htc_tzlog->pw_cursor = (int *)((int)(htc_tzlog->buffer) +
                 TZ_SCM_LOG_SIZE - INT_SIZE);

    pr_info("tzlog buffer address %x\n", TZ_SCM_LOG_PHYS);
    memset(htc_tzlog->buffer, 0, TZ_SCM_LOG_SIZE);

    secure_log_operation(0, 0, TZ_SCM_LOG_PHYS, 32 * 64, 0);

    pr_info("[TZ] --LOG START---\n");
    pr_info("%s", htc_tzlog->buffer);
    pr_info("[TZ] --- LOG END---\n");

    secure_log_operation(TZ_SCM_LOG_PHYS, TZ_SCM_LOG_SIZE, 0, 0, 0);

    if (tzdbgfs_init(pdev)) {
		pr_info("[TZ] -- init fail\n");
        goto err;
	}

    return 0;
err:
    return -ENXIO;
}

static struct platform_driver scm_log_driver = {
	.probe = scm_log_probe,
	.driver = { .name = "scm-log2", },
};


static int __init scm_device_init(void)
{
	return platform_driver_register(&scm_log_driver);
}

static void __exit scm_device_exit(void)
{
	platform_driver_unregister(&scm_log_driver);
}

module_init(scm_device_init);
module_exit(scm_device_exit);
