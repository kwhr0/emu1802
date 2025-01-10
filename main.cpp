#include "main.h"
#include "CDP1802.h"
#include "PSG.h"
#include <stdio.h>
#include <getopt.h>
#include <algorithm>
#include <string>
#include <vector>
#include <SDL2/SDL.h>

static uint8_t m[0x10000];
static CDP1802 cpu;
static PSG psg;
static FILE *fi;
static int timing, keyboard, mhz;
static bool exit_flag;

int get_timing() {
	return timing;
}

int get_keyboard() {
	return keyboard;
}

void file_seek(int sector) {
	if (!fi) return;
	fseek(fi, sector << 9, SEEK_SET);
}

int file_getc() {
	if (!fi) return 0;
	return getc(fi);
}

void psg_set(int adr, int data) {
	psg.Set(adr, data);
}

void emu_exit() {
	exit_flag = true;
}

struct Sym {
	Sym(int _adr, const char *_s = "") : adr(_adr), n(0), s(_s) {}
	int adr, n;
	std::string s;
};

static std::vector<Sym> sym;

static bool cmp_adr(const Sym &a, const Sym &b) { return a.adr < b.adr; }
static bool cmp_n(const Sym &a, const Sym &b) { return a.n > b.n; }

static void dumpProfile() {
	if (sym.empty()) return;
	std::sort(sym.begin(), sym.end(), cmp_n);
	int t = 0;
	for (auto i = sym.begin(); i != sym.end() && i->n; i++) t += i->n;
	printf("------\n");
	for (auto i = sym.begin(); i != sym.end() && double(i->n) / t >= 1e-3; i++)
		printf("%4.1f%% %s", 100. * i->n / t, i->s.c_str());
	sym.clear();
}

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

static SDL_AudioSpec audiospec;
static void audio_callback(void *userdata, uint8_t *stream, int len) {
	cpu.Execute(1e6 * mhz * double(len / 4) / audiospec.freq);
	timing++;
	if (!sym.empty()) {
		auto it = upper_bound(sym.begin(), sym.end(), Sym(cpu.GetPC()), cmp_adr);
		if (it != sym.begin() && it != sym.end()) it[-1].n++;
	}
	int16_t *snd = (int16_t *)stream;
	for (int i = 0; i < len / 2; i += 2) {
		int16_t l, r;
		psg.Update(l, r);
		snd[i] = l;
		snd[i + 1] = r;
	}
}

static void audio_init() {
	SDL_Init(SDL_INIT_AUDIO);
	SDL_AudioSpec spec;
	SDL_zero(spec);
	spec.freq = 48000;
	spec.format = AUDIO_S16SYS;
	spec.channels = 2;
	spec.samples = 512;
	spec.callback = audio_callback;
	int id = SDL_OpenAudioDevice(nullptr, 0, &spec, &audiospec, 0);
	if (id <= 0) exit(1);
	SDL_PauseAudioDevice(id, 0);
}

static void exitfunc() {
	fclose(fi);
	dumpProfile();
	SDL_Quit();
}

int main(int argc, char *argv[]) {
	int c;
	mhz = 1000;
	while ((c = getopt(argc, argv, "c:")) != -1)
		switch (c) {
			case 'c':
				sscanf(optarg, "%d", &mhz);
				break;
		}
	if (argc <= optind) {
		fprintf(stderr, "Usage: emu1802 [-c <clock freq. [MHz]> (default: %d)] <hex file>\n", mhz);
		return 1;
	}
	char path[256];
	strcpy(path, argv[optind]);
	fi = fopen(path, "r");
	if (!fi) {
		fprintf(stderr, "Cannot open %s\n", path);
		return 1;
	}
	loadIntelHex(fi);
	fclose(fi);
	char *p = strrchr(path, '.');
	if (p) {
		strcpy(p, ".adr");
		fi = fopen(path, "r");
		if (fi) {
			char s[256];
			while (fgets(s, sizeof(s), fi)) {
				int adr;
				if (sscanf(s, "%x", &adr) == 1 && strlen(s) > 5) sym.emplace_back(adr, s + 5);
			}
			fclose(fi);
			sym.emplace_back(0xffff);
		}
	}
	fi = fopen((std::string(getenv("HOME")) + "/fat.dmg").c_str(), "rb");
	if (!fi) fprintf(stderr, "Warning: ~/fat.dmg cannot open.\n");
	audio_init();
	cpu.SetMemoryPtr(m);
	cpu.Reset();
	atexit(exitfunc);
	SDL_Init(SDL_INIT_VIDEO);
	constexpr int width = 64, height = 32, mag = 8;
	SDL_Window *window = SDL_CreateWindow("emu1802", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mag * width, mag * height, 0);
	if (!window) exit(1);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) exit(1);
	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(SDL_SWSURFACE, width, height, 1, SDL_PIXELFORMAT_INDEX1LSB);
	if (!surface) exit(1);
	static constexpr SDL_Color colors[] = { { 0, 0, 0, 255 }, { 255, 255, 255, 255 } };
	SDL_SetPaletteColors(surface->format->palette, colors, 0, 2);
	int x = 0, y = 0, pitch = width / 8;
	while (!exit_flag) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			static SDL_Keycode sym;
			switch (e.type) {
				case SDL_KEYDOWN:
					if (e.key.repeat) break;
					switch (sym = e.key.keysym.sym) {
						case SDLK_RIGHT: keyboard = 28; break;
						case SDLK_LEFT:  keyboard = 29; break;
						case SDLK_UP:    keyboard = 30; break;
						case SDLK_DOWN:  keyboard = 31; break;
						default: keyboard = sym & SDLK_SCANCODE_MASK ? 0 : sym; break;
					}
					break;
				case SDL_KEYUP:
					if (e.key.keysym.sym == sym) keyboard = 0;
					break;
				case SDL_QUIT:
					exit_flag = true;
					break;
			}
		}
		for (y = 0; y < height; y++)
			for (x = 0; x < pitch; x++)
				((char *)surface->pixels)[surface->pitch * y + x] = m[0xff00 + pitch * y + x];
		SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);
		SDL_DestroyTexture(texture);
	}
	return 0;
}
