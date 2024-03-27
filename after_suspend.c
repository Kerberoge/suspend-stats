#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define LOG_FILE "/var/log/battery"
#define CAPACITY_JOULE 56 * 3600 // 56 Wh

int main() {

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
	FILE *charge_now_f, *charge_full_f, *charge_full_design_f;
	unsigned int last_nl_pos /*last newline position*/, found_newline;

	charge_now_f = fopen("/sys/class/power_supply/BAT0/charge_now", "r");
	charge_full_f = fopen("/sys/class/power_supply/BAT0/charge_full", "r");
	charge_full_design_f = fopen("/sys/class/power_supply/BAT0/charge_full_design", "r");
	fscanf(charge_now_f, "%d", &charge_after);
	fscanf(charge_full_f, "%d", &charge_full);
	fscanf(charge_full_design_f, "%d", &charge_full_design);
	fclose(charge_now_f);
	fclose(charge_full_f);
	fclose(charge_full_design_f);

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
	energy_consumed = (float) (charge_before - charge_after) / charge_full_design * CAPACITY_JOULE;
	power_draw = (float) energy_consumed / unix_time_diff;


	// Write everything to the buffer
	char temp_str[200];
	struct tm time_before = *localtime(&unix_time_before);
	struct tm time_after = *localtime(&unix_time_after);

	sprintf(temp_str, "%02d-%02d-%04d %02d:%02d:%02d        "
			"%02d-%02d-%04d %02d:%02d:%02d        "
			"%02d:%02d:%02d        "
			"%.2f%%        %.3f%%/hour        "
			"%.3f J        %.3f W\n",
			time_before.tm_mday, time_before.tm_mon + 1, time_before.tm_year + 1900,
			time_before.tm_hour, time_before.tm_min, time_before.tm_sec,
			time_after.tm_mday, time_after.tm_mon + 1, time_after.tm_year + 1900,
			time_after.tm_hour, time_after.tm_min, time_after.tm_sec,
			time_diff_h, time_diff_m, time_diff_s,
			perc_diff, perc_per_hour,
			energy_consumed, power_draw);
	strcat(buffer, temp_str);


	// Write contents of buffer to log file
	remove(LOG_FILE);
	log_file_f = fopen(LOG_FILE, "w");
	fprintf(log_file_f, "%s", buffer);
	fclose(log_file_f);


	// Set appropriate mode for log file
	chmod(LOG_FILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);


	return 0;
}
