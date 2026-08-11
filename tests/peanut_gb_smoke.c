#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#ifndef PEANUT_GB_HEADER
#define PEANUT_GB_HEADER "../core/Peanut-GB/peanut_gb.h"
#endif
#include PEANUT_GB_HEADER

#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	GB_HEADER_SIZE = 0x150,
	GB_ROM_SIZE_CODE_OFFSET = 0x148,
	GB_RAM_SIZE_CODE_OFFSET = 0x149,
	DEFAULT_FRAMES = 3000,
	HOST_LCD_WIDTH = 160,
	HOST_LCD_HEIGHT = 144
};

struct host_context {
	uint8_t *rom;
	size_t rom_size;
	uint8_t *cart_ram;
	size_t cart_ram_size;
	uint64_t rom_reads;
	uint64_t cart_ram_reads;
	uint64_t cart_ram_writes;
	uint64_t out_of_bounds_rom_reads;
	uint64_t out_of_bounds_cart_ram_accesses;
	uint64_t rendered_lines;
	uint64_t pixel_hash;
	unsigned completed_frames;
	enum gb_error_e core_error;
	uint16_t core_error_address;
	uint16_t core_error_rom_bank;
	uint16_t core_error_sp;
	uint8_t core_error_banking_mode;
	jmp_buf core_error_jump;
};

static struct host_context *get_host(struct gb_s *gb)
{
	return (struct host_context *)gb->direct.priv;
}

static uint8_t host_rom_read(struct gb_s *gb, const uint_fast32_t address)
{
	struct host_context *host = get_host(gb);
	host->rom_reads++;

	if (address >= host->rom_size) {
		host->out_of_bounds_rom_reads++;
		return 0xFF;
	}

	return host->rom[address];
}

static uint8_t host_cart_ram_read(struct gb_s *gb,
				  const uint_fast32_t address)
{
	struct host_context *host = get_host(gb);
	host->cart_ram_reads++;

	if (address >= host->cart_ram_size) {
		host->out_of_bounds_cart_ram_accesses++;
		return 0xFF;
	}

	return host->cart_ram[address];
}

static void host_cart_ram_write(struct gb_s *gb,
				const uint_fast32_t address,
				const uint8_t value)
{
	struct host_context *host = get_host(gb);
	host->cart_ram_writes++;

	if (address >= host->cart_ram_size) {
		host->out_of_bounds_cart_ram_accesses++;
		return;
	}

	host->cart_ram[address] = value;
}

static void host_core_error(struct gb_s *gb, const enum gb_error_e error,
			    const uint16_t address)
{
	struct host_context *host = get_host(gb);
	host->core_error = error;
	host->core_error_address = address;
	host->core_error_rom_bank = gb->selected_rom_bank;
	host->core_error_banking_mode = gb->cart_mode_select;
	host->core_error_sp = gb->cpu_reg.sp.reg;
	longjmp(host->core_error_jump, 1);
}

static void host_draw_line(struct gb_s *gb,
			   const uint8_t pixels[HOST_LCD_WIDTH],
			   const uint_fast8_t line)
{
	struct host_context *host = get_host(gb);
	host->rendered_lines++;

	if (line >= HOST_LCD_HEIGHT) {
		host->core_error = GB_INVALID_WRITE;
		host->core_error_address = line;
		longjmp(host->core_error_jump, 1);
	}

	/* FNV-1a gives a deterministic rendering fingerprint without output files. */
	host->pixel_hash ^= line;
	host->pixel_hash *= UINT64_C(1099511628211);
	for (size_t x = 0; x < HOST_LCD_WIDTH; x++) {
		host->pixel_hash ^= pixels[x];
		host->pixel_hash *= UINT64_C(1099511628211);
	}
}

static bool parse_frames(const char *value, unsigned *frames)
{
	char *end = NULL;
	errno = 0;
	const unsigned long parsed = strtoul(value, &end, 10);

	if (errno != 0 || end == value || *end != '\0' || parsed == 0 ||
	    parsed > UINT_MAX)
		return false;

	*frames = (unsigned)parsed;
	return true;
}

static bool read_rom(const char *path, struct host_context *host)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr, "%s: cannot open: %s\n", path, strerror(errno));
		return false;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fprintf(stderr, "%s: cannot seek: %s\n", path, strerror(errno));
		fclose(file);
		return false;
	}

	const long file_size = ftell(file);
	if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
		fprintf(stderr, "%s: cannot determine size: %s\n",
			path, strerror(errno));
		fclose(file);
		return false;
	}

	host->rom_size = (size_t)file_size;
	host->rom = malloc(host->rom_size);
	if (host->rom == NULL) {
		fprintf(stderr, "%s: cannot allocate %zu ROM bytes\n",
			path, host->rom_size);
		fclose(file);
		return false;
	}

	const size_t bytes_read = fread(host->rom, 1, host->rom_size, file);
	if (bytes_read != host->rom_size) {
		fprintf(stderr, "%s: short read (%zu/%zu bytes)\n",
			path, bytes_read, host->rom_size);
		fclose(file);
		free(host->rom);
		host->rom = NULL;
		return false;
	}

	if (fclose(file) != 0) {
		fprintf(stderr, "%s: close failed: %s\n", path, strerror(errno));
		free(host->rom);
		host->rom = NULL;
		return false;
	}

	return true;
}

static bool validate_rom_size(const char *path, const struct host_context *host)
{
	if (host->rom_size < GB_HEADER_SIZE) {
		fprintf(stderr, "%s: too small for a Game Boy ROM header\n", path);
		return false;
	}

	const uint8_t rom_size_code = host->rom[GB_ROM_SIZE_CODE_OFFSET];
	const uint8_t ram_size_code = host->rom[GB_RAM_SIZE_CODE_OFFSET];
	if (rom_size_code > 8 || ram_size_code > 4) {
		fprintf(stderr,
			"%s: unsupported size code (ROM=%u, RAM=%u)\n",
			path, rom_size_code, ram_size_code);
		return false;
	}

	const size_t declared_size = (size_t)(32 * 1024) << rom_size_code;
	if (declared_size != host->rom_size) {
		fprintf(stderr, "%s: header declares %zu bytes, file has %zu\n",
			path, declared_size, host->rom_size);
		return false;
	}

	return true;
}

static void exercise_joypad(struct gb_s *gb, const unsigned frame)
{
	/* Joypad bits are active low. Start pulses move common title screens on;
	 * subsequent short presses exercise gameplay-specific CPU and MBC paths. */
	gb->direct.joypad = 0xFF;
	if ((frame >= 180 && frame < 183) ||
	    (frame >= 600 && frame < 603) ||
	    (frame >= 1200 && frame < 1203) ||
	    (frame >= 1800 && frame % 1800 < 3)) {
		gb->direct.joypad &= (uint8_t)~(1U << 3);
		return;
	}

	if (frame < 1500)
		return;

	const unsigned phase = frame % 240;
	if (phase < 3)
		gb->direct.joypad &= (uint8_t)~(1U << 4); /* Right */
	else if (phase >= 30 && phase < 33)
		gb->direct.joypad &= (uint8_t)~(1U << 0); /* A */
	else if (phase >= 60 && phase < 63)
		gb->direct.joypad &= (uint8_t)~(1U << 6); /* Up */
	else if (phase >= 90 && phase < 93)
		gb->direct.joypad &= (uint8_t)~(1U << 1); /* B */
	else if (phase >= 120 && phase < 123)
		gb->direct.joypad &= (uint8_t)~(1U << 5); /* Left */
	else if (phase >= 150 && phase < 153)
		gb->direct.joypad &= (uint8_t)~(1U << 0); /* A */
	else if (phase >= 180 && phase < 183)
		gb->direct.joypad &= (uint8_t)~(1U << 7); /* Down */
	else if (phase >= 210 && phase < 213)
		gb->direct.joypad &= (uint8_t)~(1U << 1); /* B */
}

static bool verify_mbc5_ram_banking(struct gb_s *gb,
				    struct host_context *host)
{
	if (gb->mbc != 5 || gb->num_ram_banks < 2)
		return true;

	/* MBC5 selects RAM banks independently of the MBC1 mode bit. Exercise
	 * writes as well as reads so a save cannot silently alias every bank 0. */
	__gb_write(gb, 0x0000, 0x0A);
	for (uint8_t bank = 0; bank < gb->num_ram_banks; bank++) {
		__gb_write(gb, 0x4000, bank);
		__gb_write(gb, 0xA123, (uint8_t)(0x40U + bank));
	}

	bool passed = true;
	for (uint8_t bank = 0; bank < gb->num_ram_banks; bank++) {
		const size_t offset = (size_t)bank * CRAM_BANK_SIZE + 0x123;
		__gb_write(gb, 0x4000, bank);
		if (offset >= host->cart_ram_size ||
		    host->cart_ram[offset] != (uint8_t)(0x40U + bank) ||
		    __gb_read(gb, 0xA123) != (uint8_t)(0x40U + bank)) {
			passed = false;
			break;
		}
	}

	__gb_write(gb, 0x0000, 0x00);
	__gb_write(gb, 0x4000, 0x00);
	memset(host->cart_ram, 0, host->cart_ram_size);
	return passed;
}

static int smoke_rom(const char *path, const unsigned frames)
{
	struct host_context *host = calloc(1, sizeof(*host));
	struct gb_s *gb = calloc(1, sizeof(*gb));
	int result = EXIT_FAILURE;

	if (host == NULL || gb == NULL) {
		fprintf(stderr, "%s: cannot allocate host emulator state\n", path);
		goto cleanup;
	}
	host->pixel_hash = UINT64_C(1469598103934665603);
	host->core_error = GB_UNKNOWN_ERROR;

	if (!read_rom(path, host) || !validate_rom_size(path, host))
		goto cleanup;

	const enum gb_init_error_e init_result =
		gb_init(gb, host_rom_read, host_cart_ram_read,
			host_cart_ram_write, host_core_error, host);
	if (init_result != GB_INIT_NO_ERROR) {
		fprintf(stderr, "%s: gb_init failed with code %d\n",
			path, init_result);
		goto cleanup;
	}

	if (gb_get_save_size_s(gb, &host->cart_ram_size) != 0) {
		fprintf(stderr, "%s: invalid cartridge RAM size\n", path);
		goto cleanup;
	}
	if (host->cart_ram_size > 0) {
		host->cart_ram = calloc(host->cart_ram_size, 1);
		if (host->cart_ram == NULL) {
			fprintf(stderr, "%s: cannot allocate %zu cartridge RAM bytes\n",
				path, host->cart_ram_size);
			goto cleanup;
		}
	}
	if (!verify_mbc5_ram_banking(gb, host)) {
		fprintf(stderr, "%s: MBC5 cartridge RAM banking failed\n", path);
		goto cleanup;
	}

	gb_init_lcd(gb, host_draw_line);
	if (setjmp(host->core_error_jump) == 0) {
		for (unsigned frame = 0; frame < frames; frame++) {
			exercise_joypad(gb, frame);
			gb_run_frame(gb);
			host->completed_frames++;
		}
	}

	const bool passed =
		host->completed_frames == frames &&
		host->core_error == GB_UNKNOWN_ERROR &&
		host->out_of_bounds_rom_reads == 0 &&
		host->out_of_bounds_cart_ram_accesses == 0;

	printf("%s: %s\n", path, passed ? "PASS" : "FAIL");
	printf("  mbc=%d rom_banks=%u cart_ram=%zu frames=%u/%u\n",
		gb->mbc, (unsigned)(gb->num_rom_banks_mask + 1),
		host->cart_ram_size, host->completed_frames, frames);
	printf("  rom_reads=%llu ram_reads=%llu ram_writes=%llu lines=%llu "
	       "pixel_hash=%016llX\n",
		(unsigned long long)host->rom_reads,
		(unsigned long long)host->cart_ram_reads,
		(unsigned long long)host->cart_ram_writes,
		(unsigned long long)host->rendered_lines,
		(unsigned long long)host->pixel_hash);

	if (host->core_error != GB_UNKNOWN_ERROR)
		fprintf(stderr,
			"  core_error=%d at %04X bank=%u mode=%u sp=%04X\n",
			host->core_error, host->core_error_address,
			host->core_error_rom_bank, host->core_error_banking_mode,
			host->core_error_sp);
	if (host->out_of_bounds_rom_reads != 0 ||
	    host->out_of_bounds_cart_ram_accesses != 0)
		fprintf(stderr, "  out_of_bounds: rom=%llu cart_ram=%llu\n",
			(unsigned long long)host->out_of_bounds_rom_reads,
			(unsigned long long)
				host->out_of_bounds_cart_ram_accesses);

	result = passed ? EXIT_SUCCESS : EXIT_FAILURE;

cleanup:
	if (host != NULL) {
		free(host->cart_ram);
		free(host->rom);
	}
	free(gb);
	free(host);
	return result;
}

static void print_usage(const char *program)
{
	fprintf(stderr,
		"Usage: %s [--frames COUNT] ROM.gb [ROM.gb ...]\n", program);
}

int main(int argc, char **argv)
{
	unsigned frames = DEFAULT_FRAMES;
	int first_rom = 1;

	if (argc >= 2 && strcmp(argv[1], "--frames") == 0) {
		if (argc < 4 || !parse_frames(argv[2], &frames)) {
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
		first_rom = 3;
	}

	if (first_rom >= argc) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	int result = EXIT_SUCCESS;
	for (int i = first_rom; i < argc; i++) {
		if (smoke_rom(argv[i], frames) != EXIT_SUCCESS)
			result = EXIT_FAILURE;
	}

	return result;
}
