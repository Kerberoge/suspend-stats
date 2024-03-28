#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv) {

	FILE *log_file_f, *energy_now_f;
	unsigned int energy_before;
	time_t unix_time_before;

	log_file_f = fopen("/var/log/battery", "a");
	energy_now_f = fopen("/sys/class/power_supply/BAT0/charge_now", "r");

	unix_time_before = time(NULL);
	fscanf(energy_now_f, "%u", &energy_before);

	fprintf(log_file_f, "%lu %u", unix_time_before, energy_before);

	fclose(energy_now_f);
	fclose(log_file_f);
	
	return 0;
}
