#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define LOG_FILE "/var/log/battery"
#define TEMP_FILE "/tmp/pre_suspend_data"

// options: 1 means enabled, 0 disabled
#define TIME_BEFORE			1
#define TIME_AFTER			1
#define TIME_DIFF			1
#define PERC_DIFF			1
#define PERC_PER_HOUR		0
#define ENERGY_CONSUMED		0
#define POWER_DRAW			1
#define S0_RES_PERC			1


int file_exists(const char *path) {
	FILE *f = fopen(path, "r");

	if (!f)
		return 0;

	fclose(f);
	return 1;
}


// gets value from file
uint64_t get_value(const char *path) {
	FILE *f;
	uint64_t value;

	f = fopen(path, "r");
	fscanf(f, "%lu", &value);
	fclose(f);

	return value;
}


// strcat but with spaces between fields
void append_field(char *dest, const char *src) {
	const char *delimiter = "    ";

	if (strlen(dest) == 0) {
		// first field
		strcpy(dest, src);
	} else {
		// not first field, append a delimiter first
		strcpy(dest + strlen(dest), delimiter);
		strcpy(dest + strlen(dest), src);
	}
}


void before_suspend(void) {
	time_t unix_time_before;
	uint32_t charge_before;
	uint64_t s0_res_before;
	FILE *temp_file_f;

	unix_time_before = time(NULL);

	if (file_exists("/sys/class/power_supply/BAT0/charge_now"))
		charge_before = get_value("/sys/class/power_supply/BAT0/charge_now");
	else if (file_exists("/sys/class/power_supply/BAT0/energy_now"))
		charge_before = get_value("/sys/class/power_supply/BAT0/energy_now");
	// Get S0 residency (how much time is spent in any S0ix sleep state)
	if (file_exists("/sys/kernel/debug/pmc_core/slp_s0_residency_usec"))
		s0_res_before = get_value("/sys/kernel/debug/pmc_core/slp_s0_residency_usec");

	temp_file_f = fopen(TEMP_FILE, "a");
	fprintf(temp_file_f, "%lu %u %lu", unix_time_before, charge_before, s0_res_before);
	fclose(temp_file_f);
}


void after_suspend(void) {
	FILE *temp_file_f, *log_file_f;
	char *buffer;
	uint32_t charge_before, charge_after, charge_full, charge_full_design;
	uint32_t voltage;
	float capacity_joule;
	uint64_t s0_res_before, s0_res_after;
	time_t unix_time_before, unix_time_after, unix_time_diff;
	uint32_t time_diff_h, time_diff_m, time_diff_s;


	// Create buffer
	buffer = malloc(200);
	memset(buffer, 0, 200);


	// Get all values needed, first from temporary file and then from sysfs
	temp_file_f = fopen(TEMP_FILE, "r");
	fscanf(temp_file_f, "%lu %u %lu", &unix_time_before, &charge_before, &s0_res_before);
	fclose(temp_file_f);
	remove(TEMP_FILE);

	if (file_exists("/sys/class/power_supply/BAT0/charge_now")) {
		// charge_now is expressed in µAh
		charge_after = get_value("/sys/class/power_supply/BAT0/charge_now");
		charge_full = get_value("/sys/class/power_supply/BAT0/charge_full");
		charge_full_design = get_value("/sys/class/power_supply/BAT0/charge_full_design");
		// P = I * V
		voltage = get_value("/sys/class/power_supply/BAT0/voltage_min_design");
		capacity_joule = charge_full_design * voltage / 1e12 * 3600;
	} else if (file_exists("/sys/class/power_supply/BAT0/energy_now")) {
		// energy_now is expressed in µWh
		charge_after = get_value("/sys/class/power_supply/BAT0/energy_now");
		charge_full = get_value("/sys/class/power_supply/BAT0/energy_full");
		charge_full_design = get_value("/sys/class/power_supply/BAT0/energy_full_design");
		capacity_joule = charge_full_design / 1e6 * 3600;
	}

	if (file_exists("/sys/kernel/debug/pmc_core/slp_s0_residency_usec"))
		s0_res_after = get_value("/sys/kernel/debug/pmc_core/slp_s0_residency_usec");


	// Calculate time difference
	unix_time_after = time(NULL);
	unix_time_diff = unix_time_after - unix_time_before;

	time_diff_h = unix_time_diff / 3600;
	time_diff_m = (unix_time_diff - time_diff_h * 3600) / 60;
	time_diff_s = unix_time_diff - time_diff_h * 3600 - time_diff_m * 60;


	// Write everything to the buffer
#if TIME_BEFORE
	struct tm time_before = *localtime(&unix_time_before);
	char time_before_str[20];
	sprintf(time_before_str, "%02d-%02d-%04d %02d:%02d:%02d",
			time_before.tm_mday, time_before.tm_mon + 1, time_before.tm_year + 1900,
			time_before.tm_hour, time_before.tm_min, time_before.tm_sec);
	append_field(buffer, time_before_str);
#endif

#if TIME_AFTER
	struct tm time_after = *localtime(&unix_time_after);
	char time_after_str[20];
	sprintf(time_after_str, "%02d-%02d-%04d %02d:%02d:%02d",
			time_after.tm_mday, time_after.tm_mon + 1, time_after.tm_year + 1900,
			time_after.tm_hour, time_after.tm_min, time_after.tm_sec);
	append_field(buffer, time_after_str);
#endif

#if TIME_DIFF
	char time_diff_str[20];
	sprintf(time_diff_str, "%02d:%02d:%02d", time_diff_h, time_diff_m, time_diff_s);
	append_field(buffer, time_diff_str);
#endif

	// Note on casts inside calculations:
	// - an operation on two ints always returns an int, even when assigned to a float:
	// 		float f = 1 / 2; // f = 0.000000
	// - an operation on an int and a float returns a float
	// - constants like 1e6 are interpreted as floats, but whole numbers like 100 are not
	// - a cast is applied to the term right after it (terms inside parentheses are seen
	// 		as one term)
	// - when required, make sure to apply the cast to the first term of the expression;
	// 		a cast on the last term is not going to help if the first two terms (which
	// 		are integers) already produce a 0

#if PERC_DIFF || PERC_PER_HOUR
	float perc_diff = (float) (charge_after - charge_before) / charge_full * 100;
#endif

#if PERC_DIFF
	char perc_diff_str[20];
	sprintf(perc_diff_str, "%+5.1f%%", perc_diff);
	append_field(buffer, perc_diff_str);
#endif

#if PERC_PER_HOUR
	float perc_per_hour = perc_diff / ((float) unix_time_diff / 3600);
	char perc_per_hour_str[20];
	sprintf(perc_per_hour_str, "%+7.3f%%/h", perc_per_hour);
	append_field(buffer, perc_per_hour_str);
#endif

#if ENERGY_CONSUMED || POWER_DRAW
	// Energy in joule
	float energy_consumed = (float) (charge_after - charge_before)
		/ charge_full_design * capacity_joule;
#endif

#if ENERGY_CONSUMED
	char energy_consumed_str[20];
	sprintf(energy_consumed_str, "%+5.0fJ", energy_consumed);
	append_field(buffer, energy_consumed_str);
#endif

#if POWER_DRAW
	// Power draw in watts
	float power_draw = energy_consumed / unix_time_diff;
	char power_draw_str[20];
	sprintf(power_draw_str, "%+7.3fW", power_draw);
	append_field(buffer, power_draw_str);
#endif

#if S0_RES_PERC
	// S0 residency, as a percentage of total sleep time
	float s0_res_perc = (s0_res_after - s0_res_before) / 1e4 / unix_time_diff;
	char s0_res_perc_str[20];
	sprintf(s0_res_perc_str, "%5.1f%%", s0_res_perc);
	append_field(buffer, s0_res_perc_str);
#endif

	strcat(buffer, "\n");


	// Write contents of buffer to log file
	log_file_f = fopen(LOG_FILE, "a");
	fprintf(log_file_f, "%s", buffer);
	fclose(log_file_f);


	// Set appropriate mode for log file
	chmod(LOG_FILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}


int main(int argc, char **argv) {
	char *name = argv[0] + strlen(argv[0]) - 1;

	// get short name program was called with
	for (; name > argv[0] && *(name - 1) != '/'; name--);

	if (strcmp(name, "measure_start") == 0)
		before_suspend();
	else if (strcmp(name, "measure_end") == 0)
		after_suspend();

	return 0;
}
