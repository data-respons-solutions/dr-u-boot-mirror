#include <common.h>
#include <stdlib.h>
#include <string.h>
#include <env.h>
#include <part.h>
#include <inttypes.h>
#include <command.h>
#include <fs.h>
#include "../common/include/nvram.h"


/* Depends:
 * SYS_BOOT_DEV --> boot device num
 * SYS_BOOT_IFACE --> boot iface
 */

#define FIT_IMAGE "/boot/fitImage"


static const char* sys_boot_part = "SYS_BOOT_PART";
static const char* sys_boot_swap = "SYS_BOOT_SWAP";
static const char* sys_boot_attempts = "SYS_BOOT_ATTEMPTS";
static const char* sys_fit_conf = "SYS_FIT_CONF";

static const char* syslabel_default = "rootfs1";


enum swap_state {
	SWAP_NORMAL,
	SWAP_INIT,
	SWAP_ONGOING,
	SWAP_FAILED,
	SWAP_ROLLBACK,
	SWAP_INVAL,
};

static enum swap_state find_state(ulong* attempts)
{
	if (!nvram_get(sys_boot_part) || !nvram_get(sys_boot_swap))
		return SWAP_INVAL;

	const int part_swap_equal = !strcmp(nvram_get(sys_boot_part), nvram_get(sys_boot_swap));
	if (part_swap_equal && !nvram_get(sys_boot_attempts))
		return SWAP_NORMAL;

	if (part_swap_equal && nvram_get(sys_boot_attempts))
		return SWAP_ROLLBACK;

	if (!nvram_get(sys_boot_attempts))
		return SWAP_INIT;

	*attempts = nvram_get_ulong(sys_boot_attempts, 10, ULONG_MAX);
	if (*attempts == ULONG_MAX)
		return SWAP_INVAL;

	if (*attempts < 3)
		return SWAP_ONGOING;

	return SWAP_FAILED;
}

static int nvram_root_swap(char** rootfs_label)
{
	int r = nvram_init();
	if (r) {
		printf("BOOT: failed nvram_init [%d]: %s\n", r, errno_str(r));
		return r;
	}

	char* label = nvram_get(sys_boot_part);
	ulong attempts = ULONG_MAX;

	switch(find_state(&attempts)) {
	case SWAP_NORMAL:
		printf("BOOT: normal boot\n");
		break;
	case SWAP_INIT:
		printf("BOOT: root swap initiated\n");
		nvram_set_ulong(sys_boot_attempts, 1);
		label = nvram_get(sys_boot_swap);
		break;
	case SWAP_ONGOING:
		nvram_set_ulong(sys_boot_attempts, ++attempts);
		printf("BOOT: root swap ongoing: attempt: %s\n", nvram_get(sys_boot_attempts));
		label = nvram_get(sys_boot_swap);
		break;
	case SWAP_FAILED:
		printf("BOOT: root swap failed: rollback from %s to %s\n", nvram_get(sys_boot_swap), nvram_get(sys_boot_part));
		nvram_set(sys_boot_swap, nvram_get(sys_boot_part));
		break;
	case SWAP_ROLLBACK:
		printf("BOOT: root swap rollback has occured\n");
		break;
	case SWAP_INVAL:
		printf("BOOT: root swap invalid state -- reset to defaults\n");
		if (nvram_set(sys_boot_part, syslabel_default))
			return -ENOMEM;
		if (nvram_set(sys_boot_swap, syslabel_default))
			return -ENOMEM;
		if (nvram_set(sys_boot_attempts, NULL))
			return -ENOMEM;
		label = nvram_get(sys_boot_part);
		break;
	}

	r = nvram_commit();
	if (r) {
		printf("BOOT: failed commiting nvram [%d]: %s\n", r, errno_str(r));
		return r;
	}

	*rootfs_label = label;
	return 0;
}

static int load_fit(const char* label)
{
	/* Find partition */
	printf("BOOT: partition \"%s\" on device %s %d\n", label, SYS_BOOT_IFACE, SYS_BOOT_DEV);
	struct blk_desc* dev = blk_get_dev(SYS_BOOT_IFACE, SYS_BOOT_DEV);
	if (!dev) {
		printf("BOOT: failed getting device\n");
		return -EFAULT;
	}
	disk_partition_t part_info;
	const int partnr = part_get_info_by_name(dev, label, &part_info);
	if (partnr < 1) {
		printf("BOOT: failed getting partition\n");
		return -EFAULT;
	}
	printf("BOOT: partition uuid: %s\n", part_info.uuid);

	/* Read image */
	int r = fs_set_blk_dev_with_part(dev, partnr);
	if (r) {
		printf("BOOT: failed setting fs pointer\n");
		return -EFAULT;
	}
	loff_t fit_size = 0;
	r = fs_read(FIT_IMAGE, FIT_ADDR, 0, 0, &fit_size);
	fs_close();
	if (r) {
		printf("BOOT: Failed reading image\n");
		return -EFAULT;
	}

	/* Set kernel cmdline */
	const char *root_partuuid = "root=PARTUUID=";
	const int cmdline_size = strlen(root_partuuid) + strlen(part_info.uuid) + 1;
	char *cmdline = malloc(cmdline_size);
	if (!cmdline)
		return -ENOMEM;
	strcat(cmdline, root_partuuid);
	strcat(cmdline, part_info.uuid);
	r = env_set("cmdline", cmdline);
	free(cmdline);
	if (r)
		return -ENOMEM;

	return 0;
}

static int boot_fit(void)
{
	/* Build bootm args -- 0x[FIT_ADDR]#[CONFIG] */
	char fit_addr[17];
	sprintf(fit_addr, "%lx", (unsigned long) FIT_ADDR);
	int arglen = strlen(fit_addr) + 1;
	/* check optional config */
	const char *conf = nvram_get(sys_fit_conf);
	if (conf) {
		/*     += # + conf */
		arglen += 1 + strlen(conf);
	}
	char *arg = malloc(arglen);
	if (!arg)
		return -ENOMEM;
	strcat(arg, fit_addr);
	if (conf) {
		strcat(arg, "#");
		strcat(arg, conf);
	}
	char *boot_args[] = {"bootm", arg};
	do_bootm(NULL, 0, 2, boot_args);

	/* If we're here the fit config might not have been found.
	 * Make an attempt with default config */
	strcpy(arg, fit_addr);
	do_bootm(NULL, 0, 2, boot_args);

	/* Shouldn't be here -- boot failed */
	free(arg);
	return -EFAULT;
}

static int do_system_boot(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[])
{
	int r = 0;
	char* rootfs_label = NULL;
	r = nvram_root_swap(&rootfs_label);
	if (r) {
		printf("BOOT: no rootfs label found [%d]: %s\n", r, errno_str(r));
		return r;
	}

	r = load_fit(rootfs_label);
	if (r) {
		printf("BOOT: failed loading image [%d]: %s\n", r, errno_str(r));
		return r;
	}

	r = boot_fit();
	if (r) {
		printf("BOOT: failed booting image [%d]: %s\n", r, errno_str(r));
		return r;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	system_boot, 1, 0, do_system_boot, "Boot system (linux)",
	"With root swap\n"
);
