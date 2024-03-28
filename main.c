#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define LOG_FILE "/var/log/battery"
#define CAPACITY_JOULE 56 * 3600 // 56 Wh

// options: 1 means enabled, 0 disabled
#define TIME_BEFORE			1
#define TIME_AFTER			1
#define TIME_DIFF			1
#define PERC_DIFF			0
#define PERC_PER_HOUR		0
#define ENERGY_CONSUMED		0
#define POWER_DRAW			1


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
	const char last_char = *(dest + strlen(dest) - 1);
	const char *delimiter = "        ";

	if (last_char == '\n') {
		// first field
		strcpy(dest + strlen(dest), src);
	} else {
		// not first field
		strcpy(dest + strlen(dest), delimiter);
		strcpy(dest + strlen(dest), src);
	}
}


void before_suspend() {
	FILE *log_file_f, *energy_now_f;
	unsigned int energy_before;
	time_t unix_time_before;

	log_file_f = fopen(LOG_FILE, "a");
	energy_now_f = fopen("/sys/class/power_supply/BAT0/charge_now", "r");

	unix_time_before = time(NULL);
	fscanf(energy_now_f, "%u", &energy_before);

	fprintf(log_file_f, "%lu %u", unix_time_before, energy_before);

	fclose(energy_now_f);
	fclose(log_file_f);
}


void after_suspend() {
	FILE *log_file_f;
	char *buffer;
	unsigned int buf_len;
	unsigned int charge_before, charge_after, charge_full, charge_full_design;
	time_t unix_time_before, unix_time_after, unix_time_diff;
	unsigned int time_diff_h, time_diff_m, time_diff_s;
	float perc_diff, perc_per_hour;
	float energy_consumed, power_draw;


	// Copy contents of log file into a buffer
	log_file_f = fopen(LOG_FILE, "r");
	
	fseek(log_file_f, 0, SEEK_END);
	buf_len = ftell(log_file_f);
	buffer = malloc(sizeof(char) * (buf_len + 200));
	
	fseek(log_file_f, 0, SEEK_SET);
	fread(buffer, 1, buf_len, log_file_f);

	fclose(log_file_f);


	// Get all values needed, first from sysfs and then from log file
	unsigned int last_nl_pos /*last newline position*/, found_newline;

	charge_after = get_value("/sys/class/power_supply/BAT0/charge_now");
	charge_full = get_value("/sys/class/power_supply/BAT0/charge_full");
	charge_full_design = get_value("/sys/class/power_supply/BAT0/charge_full_design");

	last_nl_pos = buf_len - 1;
	found_newline = 0;
	while (last_nl_pos > 0) {
		if (buffer[last_nl_pos] == '\n') {
			found_newline = 1;
			break;
		} else {
			last_nl_pos--;
		}
	}

	sscanf(&buffer[last_nl_pos + found_newline], "%lu %u",
		&unix_time_before, &charge_before);

	for (int i = last_nl_pos + found_newline; i < buf_len; i++) {
		buffer[i] = 0;
	}
	

	// Calculate time difference
	unix_time_after = time(NULL);
	unix_time_diff = unix_time_after - unix_time_before;

	time_diff_h = unix_time_diff / 3600;
	time_diff_m = (unix_time_diff - time_diff_h * 3600) / 60;
	time_diff_s = unix_time_diff - time_diff_h * 3600 - time_diff_m * 60;


	// Calculate percent per hour battery usage during sleep mode
	perc_diff = (float) (int) (charge_before - charge_after) / charge_full * 100;
	perc_per_hour = perc_diff / ((float) unix_time_diff / 3600);


	// Calculate power draw in watts
	energy_consumed = (float) (int) (charge_before - charge_after) / charge_full_design * CAPACITY_JOULE;
	power_draw = (float) energy_consumed / unix_time_diff;


	// Write everything to the buffer
#if TIME_BEFORE
	char time_before_str[20];
	struct tm time_before = *localtime(&unix_time_before);
	sprintf(time_before_str, "%02d-%02d-%04d %02d:%02d:%02d", 
			time_before.tm_mday, time_before.tm_mon + 1, time_before.tm_year + 1900,
			time_before.tm_hour, time_before.tm_min, time_before.tm_sec);
	append_field(buffer, time_before_str);
#endif

#if TIME_AFTER
	char time_after_str[20];
	struct tm time_after = *localtime(&unix_time_after);
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

#if PERC_DIFF
	char perc_diff_str[20];
	sprintf(perc_diff_str, "%.2f%%", perc_diff);
	append_field(buffer, perc_diff_str);
#endif

#if PERC_PER_HOUR
	char perc_per_hour_str[20];
	sprintf(perc_per_hour_str, "%.3f%%/h", perc_per_hour);
	append_field(buffer, perc_per_hour_str);
#endif

#if ENERGY_CONSUMED
	char energy_consumed_str[20];
	sprintf(energy_consumed_str, "%.0f J", energy_consumed);
	append_field(buffer, energy_consumed_str);
#endif

#if POWER_DRAW
	char power_draw_str[20];
	sprintf(power_draw_str, "%.3f W", power_draw);
	append_field(buffer, power_draw_str);
#endif
	
	strcat(buffer, "\n");


	// Write contents of buffer to log file
	remove(LOG_FILE);
	log_file_f = fopen(LOG_FILE, "w");
	fprintf(log_file_f, "%s", buffer);
	fclose(log_file_f);


	// Set appropriate mode for log file
	chmod(LOG_FILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}


int main(int argc, char **argv) {
	if (strcmp(get_name(argv[0]), "measure_start") == 0) {
		before_suspend();
	} else if (strcmp(get_name(argv[0]), "measure_end") == 0) {
		after_suspend();
	}

	return 0;
}
