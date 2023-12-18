#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

int main() {

	FILE *log_file_f, *energy_now_f, *energy_full_f;
	char *buffer;
	unsigned int buf_len;
	unsigned int last_newline;
	int found_newline;
	time_t unix_time_before, unix_time_after, unix_time_diff;
	unsigned int time_diff_h, time_diff_m, time_diff_s;
	struct tm time_before, time_after;
	unsigned int energy_before, energy_after, energy_full;
	float perc_diff, perc_per_hour;
	char temp_str[100];

	// Copy contents of /var/log/battery into a buffer
	log_file_f = fopen("/var/log/battery", "r");
	
	fseek(log_file_f, 0, SEEK_END);
	buf_len = ftell(log_file_f);
	buffer = malloc(sizeof(char) * (buf_len + 100));
	
	fseek(log_file_f, 0, SEEK_SET);
	fread(buffer, 1, buf_len, log_file_f);

	fclose(log_file_f);

	// Remove last line, after extracting data from it
	last_newline = buf_len - 1;
	found_newline = 0;
	while (last_newline > 0) {
		if (buffer[last_newline] == '\n') {
			found_newline = 1;
			break;
		} else {
			last_newline--;
		}
	}

	sscanf(&buffer[last_newline + found_newline], "%lu %u", \
			&unix_time_before, &energy_before);

	for (int i = last_newline + found_newline; i < buf_len; i++) {
		buffer[i] = 0;
	}

	// Calculate time difference
	unix_time_after = time(NULL);
	unix_time_diff = unix_time_after - unix_time_before;

	time_diff_h = unix_time_diff / 3600;
	time_diff_m = (unix_time_diff - time_diff_h * 3600) / 60;
	time_diff_s = unix_time_diff - time_diff_h * 3600 - time_diff_m * 60;

	time_before = *localtime(&unix_time_before);
	time_after = *localtime(&unix_time_after);

	// Calculate percent per hour battery usage during sleep mode
	energy_now_f = fopen("/sys/class/power_supply/BAT0/energy_now", "r");
	energy_full_f = fopen("/sys/class/power_supply/BAT0/energy_full", "r");
	fscanf(energy_now_f, "%d", &energy_after);
	fscanf(energy_full_f, "%d", &energy_full);
	fclose(energy_now_f);
	fclose(energy_full_f);

	perc_diff = (float) (int) (energy_before - energy_after) / energy_full * 100;
	perc_per_hour = perc_diff / ((float) unix_time_diff / 3600);

	// Write everything to the buffer
	sprintf(temp_str, "%02d-%02d-%04d %02d:%02d:%02d        " \
			"%02d-%02d-%04d %02d:%02d:%02d        " \
			"%02d:%02d:%02d        " \
			"%.2f%%        %.3f%%/hour\n", \
			time_before.tm_mday, time_before.tm_mon + 1, time_before.tm_year + 1900, \
			time_before.tm_hour, time_before.tm_min, time_before.tm_sec, \
			time_after.tm_mday, time_after.tm_mon + 1, time_after.tm_year + 1900, \
			time_after.tm_hour, time_after.tm_min, time_after.tm_sec, \
			time_diff_h, time_diff_m, time_diff_s, \
			perc_diff, perc_per_hour);
	strcat(buffer, temp_str);

	// Write contents of buffer to /var/log/battery
	remove("/var/log/battery");
	log_file_f = fopen("/var/log/battery", "w");
	fprintf(log_file_f, "%s", buffer);
	fclose(log_file_f);

	// Set appropriate mode for log file
	chmod("/var/log/battery", S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	return 0;
}
