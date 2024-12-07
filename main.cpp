#include "CDP1802.h"
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <getopt.h>

static uint8_t m[0x10000];
static CDP1802 cpu;

static int getHex(char *&p, int n) {
	int r = 0;
	do {
		int c = *p++ - '0';
		if (c > 10) c -= 'A' - '0' - 10;
		if (c >= 0 && c < 16) r = r << 4 | c;
	} while (--n > 0);
	return r;
}

static void loadIntelHex(FILE *fi) {
	int ofs = 0;
	char s[256];
	while (fgets(s, sizeof(s), fi)) if (*s == ':') {
		char *p = s + 1;
		int n = getHex(p, 2), a = getHex(p, 4), t = getHex(p, 2);
		if (!t)
			while (--n >= 0) {
				if (ofs + a < 0x10000) m[ofs + a++] = getHex(p, 2);
			}
		else if (t == 2)
			ofs = getHex(p, 4) << 4;
//		else if (t == 4)
//			ofs = getHex(p, 4) << 16;
		else break;
	}
}

struct TimeSpec : timespec {
	TimeSpec() {}
	TimeSpec(double t) {
		tv_sec = floor(t);
		tv_nsec = long(1e9 * (t - tv_sec));
	}
	TimeSpec(time_t s, long n) {
		tv_sec = s;
		tv_nsec = n;
	}
	operator double() const { return tv_sec + 1e-9 * tv_nsec; }
	TimeSpec operator+(const TimeSpec &t) const {
		time_t s = tv_sec + t.tv_sec;
		long n = tv_nsec + t.tv_nsec;
		if (n < 0) {
			s++;
			n -= 1000000000;
		}
		return TimeSpec(s, n);
	}
	TimeSpec operator-(const TimeSpec &t) const {
		time_t s = tv_sec - t.tv_sec;
		long n = tv_nsec - t.tv_nsec;
		if (n < 0) {
			s--;
			n += 1000000000;
		}
		return TimeSpec(s, n);
	}
	TimeSpec &operator+=(const TimeSpec &t) { return *this = *this + t; }
};

static const TimeSpec SLICE(1. / 100.);
static TimeSpec tref, tcur;

static void exitfunc() {
	double duration = (tref += SLICE) - tcur;
	if (duration < 0.) printf("time exceeded: %.0f%%\n", -100 * duration / SLICE);
}

int main(int argc, char *argv[]) {
	int c, mhz = 1000;
	while ((c = getopt(argc, argv, "c:")) != -1)
		switch (c) {
			case 'c':
				sscanf(optarg, "%d", &mhz);
				break;
		}
	if (argc <= optind) {
		fprintf(stderr, "Usage: emu1802 [-c <clock freq. [MHz]> (default: 1000)] <hex file>\n");
		return 1;
	}
	FILE *fi = fopen(argv[optind], "r");
	if (!fi) return 1;
	loadIntelHex(fi);
	cpu.SetMemoryPtr(m);
	cpu.Reset();
	atexit(exitfunc);
	clock_gettime(CLOCK_MONOTONIC_RAW, &tref);
	while (1) {
		double c = 1e6 * mhz * SLICE;
		cpu.Execute(c);
		clock_gettime(CLOCK_MONOTONIC_RAW, &tcur);
		TimeSpec duration = (tref += SLICE) - tcur;
		if (duration >= 0.) {
			nanosleep(&duration, nullptr);
			clock_gettime(CLOCK_MONOTONIC_RAW, &tref);
		}
	}
	return 0;
}
