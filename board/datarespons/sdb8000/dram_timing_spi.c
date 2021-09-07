#include <common.h>
#include <stdlib.h>
#include <inttypes.h>
#include <linux/errno.h>
#include <spi_flash.h>
#include <asm/arch/ddr.h>
#include "../common/libnvram/libnvram.h"
#include "dram_timing_spi.h"

#define CONFIG_DR_NVRAM_PLATFORM_OFFSET 0x40000
#define CONFIG_DR_NVRAM_PLATFORM_SIZE 0x10000

struct input_data {
	uint8_t *begin; // libnvram_it_begin()
	uint8_t *end;   // libnvram_it_end()
};

static uint32_t letou32(const uint8_t* in)
{
	return	  ((uint32_t) in[0] << 0)
			| ((uint32_t) in[1] << 8)
			| ((uint32_t) in[2] << 16)
			| ((uint32_t) in[3] << 24);
}

static int find_key(struct libnvram_entry* entry, const char* key, const struct input_data* in)
{
	for (uint8_t* it = in->begin; it != in->end; it = libnvram_it_next(it)) {
		libnvram_it_deref(it, entry);
		if (strncmp(key, (char*) entry->key, entry->key_len) == 0)
			return 0;
	}
	return -ENOENT;
}

static int parse_str(char** str, const char* key, const struct input_data* in)
{
	struct libnvram_entry entry;
	int r = find_key(&entry, key, in);
	if (r)
		return r;

	if (entry.value_len < 1 || entry.value[entry.value_len - 1] != '\0')
		return -EINVAL;

	*str = malloc_simple(entry.value_len);
	if (!*str)
		return -ENOMEM;

	memcpy(*str, entry.value, entry.value_len);
	return 0;
}

static int parse_u32(uint32_t** data, uint32_t* len, const char* key, const struct input_data* in)
{
	struct libnvram_entry entry;
	int r = find_key(&entry, key, in);
	if (r)
		return r;

	if (entry.value_len < 4 || entry.value_len % 4 != 0)
		return -EINVAL;

	const uint32_t n = entry.value_len / 4;
	uint32_t *buf = malloc_simple(n * sizeof(uint32_t));
	if (!buf)
		return -ENOMEM;

	for (uint32_t i = 0; i < n; ++i) {
		buf[i] = letou32(entry.value + i * 4);
	}

	*len = n;
	*data = buf;

	return 0;
}

static int parse_dram_cfg_param(struct dram_cfg_param** param, uint32_t* len, const char* key, const struct input_data* in)
{
	struct libnvram_entry entry;
	int r = find_key(&entry, key, in);
	if (r)
		return r;

	if (entry.value_len < 8 || entry.value_len % 8 != 0)
		return -EINVAL;

	const uint32_t n = entry.value_len / 8;
	struct dram_cfg_param *buf = malloc_simple(n * sizeof(struct dram_cfg_param));
	if (!buf)
		return -ENOMEM;

	for (uint32_t i = 0; i < n; ++i) {
		buf[i].reg = letou32(entry.value + i * 8);
		buf[i].val = letou32(entry.value + i * 8 + 4);
	}

	*len = n;
	*param = buf;

	return 0;
}

static const char* fsp_key(uint32_t index)
{
	const char *fsp_keys[] = {
			"fsp_msg_0",
			"fsp_msg_1",
			"fsp_msg_2",
			"fsp_msg_3",
	};
	if (index > 3)
		return "fsp_msg_INVALID";
	return fsp_keys[index];
}

static int parse_fsp_msg(struct dram_fsp_msg** fsp_msg, uint32_t* len, const char* key, const struct input_data* in)
{

	struct libnvram_entry entry;
	int r = 0;
	uint32_t n = 0;
	uint32_t bytes = 0;

	// Get total size needed
	while (n < UINT32_MAX) {
		const char* fspkey = fsp_key(n);
		r = find_key(&entry, fspkey, in);
		if (r)
			break;
		if (entry.value_len < 8 || entry.value_len % 8 != 0)
			return -EINVAL;
		bytes += sizeof(struct dram_fsp_msg) + sizeof(struct dram_cfg_param) * ((entry.value_len - 8) / 8);
		n++;
	}

	if (n < 1)
		return -EINVAL;

	// We allocate space for dram_fsp_msg[] + all dram_fsp_msg.fsp_cfg[] at end
	struct dram_fsp_msg *ptr_fsp = malloc_simple(bytes);
	if (!ptr_fsp)
		return -ENOMEM;
	struct dram_cfg_param *ptr_cfg = (struct dram_cfg_param*) (ptr_fsp + n);

	// fill data
	for (uint32_t i = 0; i < n; i++) {
		const char *fspkey = fsp_key(i);
		r = find_key(&entry, fspkey, in);
		if (r)
			return -EBADF;

		ptr_fsp[i].drate = letou32(entry.value);
		ptr_fsp[i].fw_type = letou32(entry.value + 4);
		ptr_fsp[i].fsp_cfg = ptr_cfg;
		ptr_fsp[i].fsp_cfg_num = (entry.value_len - 8) / 8;

		for (uint32_t j = 0; j < ptr_fsp[i].fsp_cfg_num; ++j) {
			ptr_cfg->reg = letou32(entry.value + 8 + j * 8);
			ptr_cfg->val = letou32(entry.value + 8 + j * 8 + 4);
			ptr_cfg++;
		}
	}

	*fsp_msg = ptr_fsp;
	*len = n;

	return 0;
}

static int load_fsp_table(unsigned int fsp_table[4], const struct input_data* in)
{
	uint32_t len;
	uint32_t *arr = NULL;
	int r = parse_u32(&arr, &len, "fsp_table", in);
	if (r)
		return r;
	if (len != 4)
		return -EINVAL;

	fsp_table[0] = arr[0];
	fsp_table[1] = arr[1];
	fsp_table[2] = arr[2];
	fsp_table[3] = arr[3];

	return 0;
}

static int load_dram_timing(struct dram_timing_info* dram_timing, const struct libnvram_header* hdr, const uint8_t* buf, uint32_t len)
{
	struct input_data in;
	int r = 0;

	in.begin = libnvram_it_begin(buf, len, hdr);
	in.end = libnvram_it_end(buf, len, hdr);

	char *name = NULL;
	r = parse_str(&name, "name", &in);
	if (r) {
		printf("dram_timing: \"name\" invalid: %d\n", r);
		return r;
	}

	char *version = NULL;
	r = parse_str(&version, "version", &in);
	if (r) {
		printf("dram_timing: \"version\" invalid: %d\n", r);
		return r;
	}

	printf("dram_timing: %s [%s]\n", name, version);

	r = parse_dram_cfg_param(&dram_timing->ddrc_cfg, &dram_timing->ddrc_cfg_num, "ddrc_cfg", &in);
	if (r) {
		printf("dram_timing: \"ddrc_cfg\" invalid: %d\n", r);
		return r;
	}

	r = parse_dram_cfg_param(&dram_timing->ddrphy_cfg, &dram_timing->ddrphy_cfg_num, "ddrphy_cfg", &in);
	if (r) {
		printf("dram_timing: \"ddrphy_cfg\" invalid: %d\n", r);
		return r;
	}

	r = parse_fsp_msg(&dram_timing->fsp_msg, &dram_timing->fsp_msg_num, "fsp_msg", &in);
	if (r) {
		printf("dram_timing: \"fsp_msg\" invalid: %d\n", r);
		return r;
	}

	r = parse_dram_cfg_param(&dram_timing->ddrphy_trained_csr, &dram_timing->ddrphy_trained_csr_num, "ddrphy_trained_csr", &in);
	if (r) {
		printf("dram_timing: \"ddrphy_trained_csr\" invalid: %d\n", r);
		return r;
	}

	r = parse_dram_cfg_param(&dram_timing->ddrphy_pie, &dram_timing->ddrphy_pie_num, "ddrphy_pie", &in);
	if (r) {
		printf("dram_timing: \"ddrphy_pie\" invalid: %d\n", r);
		return r;
	}

	r = load_fsp_table(dram_timing->fsp_table, &in);
	if (r) {
		printf("dram_timing: \"fsp_table\" invalid: %d\n", r);
		return r;
	}

	return 0;
}

int load_dram_timing_spi(struct dram_timing_info* dram_timing)
{
	struct spi_flash *flash = NULL;
	const uint32_t hdr_len = libnvram_header_len();
	int r = 0;

	if (hdr_len > CONFIG_DR_NVRAM_PLATFORM_SIZE)
		return -EINVAL;

	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
			CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE);

	if (!flash) {
		printf("dram_timing: SPI probe failed\n");
		return -ENODEV;
	}
	uint8_t *hdr_buf = malloc_simple(hdr_len);
	if (!hdr_buf)
		return -ENOMEM;

	r = spi_flash_read(flash, CONFIG_DR_NVRAM_PLATFORM_OFFSET, hdr_len, hdr_buf);
	if (r) {
		printf("dram_timing: SPI failed reading header: %d\n", r);
		return r;
	}

	struct libnvram_header hdr;
	r = libnvram_validate_header(hdr_buf, hdr_len, &hdr);
	if (r) {
		printf("dram_timing: failed validating header: %d\n", r);
		return r;
	}
	if ((hdr.len + hdr_len) > CONFIG_DR_NVRAM_PLATFORM_SIZE)
		return -EINVAL;

	uint8_t *data_buf = malloc_simple(hdr.len);
	if (!data_buf)
			return -ENOMEM;

	r = spi_flash_read(flash, CONFIG_DR_NVRAM_PLATFORM_OFFSET + hdr_len, hdr.len, data_buf);
	if (r) {
		printf("dram_timing: SPI failed reading data: %d\n", r);
		return r;
	}

	return load_dram_timing(dram_timing, &hdr, data_buf, hdr.len);
}
