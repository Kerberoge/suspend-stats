# Suspend Stats
This program gathers statistics about suspending and writes them to `/var/log/suspend`. It is solely intended for usage on laptops or other devices that can enter a sleep state and operate on a battery.

Examples of the data collected include:
- start and end time of the suspend period
- time spent in suspend
- battery consumption during suspend, in both joules and percents
- [S0ix](https://unix.stackexchange.com/q/784449) residency (how much time was spent in any S0 state).

# Usage
The best way to use this program is by running it as a suspend hook before and after suspending. Depending on the program you use to suspend, this should be done as follows:
- `zzz`: Create a symlink called `measure_start` in `/etc/zzz.d/suspend` and one called `measure_end` in `/etc/zzz.d/resume`. See https://man.voidlinux.org/zzz.8#FILES.
- `elogind`: Create the symlinks mentioned above in `/usr/local/bin`, and then create a script that calls either `measure_start` or `measure_end`, depending on the arguments passed to it. See https://wiki.gentoo.org/wiki/Elogind#Suspend.2FHibernate_Resume.2FThaw_hook_scripts.

# Example
Entries in `/var/log/suspend` look roughly like this (not all options are enabled):
```
05-04-2025 18:41:21    05-04-2025 21:21:05    02:39:44     -2.2%     -0.236W     99.9%
```
From left to right: start time, end time, time spent in suspend, battery percentage consumed, average power draw during suspend, S0ix residency percentage.
