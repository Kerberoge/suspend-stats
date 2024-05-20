#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define LOG_FILE "/var/log/battery"
#define TEMP_FILE "/tmp/pre_suspend_data"
#define CAPACITY_JOULE 37 * 3600 // 37 Wh

// options: 1 means enabled, 0 disabled
#define TIME_BEFORE			1
#define TIME_AFTER			1
#define TIME_DIFF			1
#define PERC_DIFF			1
#define PERC_PER_HOUR		0
#define ENERGY_CONSUMED		0
#define POWER_DRAW			1


int file_exists(const char *path) {
	FILE *f = fopen(path, "r");

	if (!f)
		return 0;

	fclose(f);
	return 1;
}


// gets short name program was called with
const char *get_name(const char *path) {
	int slash_pos;

	for (slash_pos = strlen(path) - 1; slash_pos >= 0 && path[slash_pos] != '/'; slash_pos--);

	return path + slash_pos + 1;
}


// gets value from file
unsigned int get_value(const char *path) {
	FILE *f;
	unsigned int value;

	f = fopen(path, "r");
	fscanf(f, "%d", &value);
	fclose(f);

	return value;
}


// strcat but with spaces between fields
void append_field(char *dest, const char *src) {
	const char *delimiter = "        ";

	if (strlen(dest) == 0) {
		// first field
		strcpy(dest, src);
	} else {
		// not first field, append a delimiter first
		strcpy(dest + strlen(dest), delimiter);
		strcpy(dest + strlen(dest), src);
	}
}


void store_temp_data(long unsigned int time, unsigned int charge) {
	FILE *temp_file;
	temp_file = fopen(TEMP_FILE, "a");
	fprintf(temp_file, "%lu %u", time, charge);
	fclose(temp_file);
}


void retrieve_temp_data(long unsigned int *time, unsigned int *charge) {
	FILE *temp_file;
	temp_file = fopen(TEMP_FILE, "r");
	fscanf(temp_file, "%lu %u", time, charge);
	fclose(temp_file);
	remove(TEMP_FILE);
}


void write_buffer(char *buffer) {
	FILE *log_file_f = fopen(LOG_FILE, "a");
	fprintf(log_file_f, "%s", buffer);
	fclose(log_file_f);
}


void before_suspend(void) {
	time_t unix_time_before;
	unsigned int charge_before;

	unix_time_before = time(NULL);

	if (file_exists("/sys/class/power_supply/BAT0/charge_now"))
		charge_before = get_value("/sys/class/power_supply/BAT0/charge_now");
	else if (file_exists("/sys/class/power_supply/BAT0/energy_now"))
		charge_before = get_value("/sys/class/power_supply/BAT0/energy_now");

	store_temp_data(unix_time_before, charge_before);
}


void after_suspend(void) {
	char *buffer;
	unsigned int charge_before, charge_after, charge_full, charge_full_design;
	time_t unix_time_before, unix_time_after, unix_time_diff;
	unsigned int time_diff_h, time_diff_m, time_diff_s;


	// Create buffer
	buffer = malloc(200);
	memset(buffer, 0, 200);


	// Get all values needed, first from sysfs and then from temporary file
	if (file_exists("/sys/class/power_supply/BAT0/charge_now")) {
		charge_after = get_value("/sys/class/power_supply/BAT0/charge_now");
		charge_full = get_value("/sys/class/power_supply/BAT0/charge_full");
		charge_full_design = get_value("/sys/class/power_supply/BAT0/charge_full_design");
	} else if (file_exists("/sys/class/power_supply/BAT0/energy_now")) {
		charge_after = get_value("/sys/class/power_supply/BAT0/energy_now");
		charge_full = get_value("/sys/class/power_supply/BAT0/energy_full");
		charge_full_design = get_value("/sys/class/power_supply/BAT0/energy_full_design");
	}

	retrieve_temp_data(&unix_time_before, &charge_before);


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

#if PERC_DIFF || PERC_PER_HOUR
	float perc_diff = (float) (int) (charge_after - charge_before) / charge_full * 100;
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
	float energy_consumed = (float) (int) (charge_after - charge_before) / charge_full_design * CAPACITY_JOULE;
#endif

#if ENERGY_CONSUMED
	char energy_consumed_str[20];
	sprintf(energy_consumed_str, "%+5.0fJ", energy_consumed);
	append_field(buffer, energy_consumed_str);
#endif

#if POWER_DRAW
	// Power draw in watts
	float power_draw = (float) energy_consumed / unix_time_diff;
	char power_draw_str[20];
	sprintf(power_draw_str, "%+7.3fW", power_draw);
	append_field(buffer, power_draw_str);
#endif

	strcat(buffer, "\n");


	// Write contents of buffer to log file
	write_buffer(buffer);


	// Set appropriate mode for log file
	chmod(LOG_FILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}


int main(int argc, char **argv) {
	if (strcmp(get_name(argv[0]), "measure_start") == 0)
		before_suspend();
	else if (strcmp(get_name(argv[0]), "measure_end") == 0)
		after_suspend();

	return 0;
}
