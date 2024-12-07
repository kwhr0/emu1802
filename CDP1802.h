// CDP1802
// Copyright 2024 © Yasuo Kuwahara
// MIT License

#include <cstdint>
#include <cstdio>

#define CDP1802_TRACE			0

#if CDP1802_TRACE
#define CDP1802_TRACE_LOG(adr, data, type) \
	if (tracep->index < ACSMAX) tracep->acs[tracep->index++] = { adr, (u16)data, type }
#else
#define CDP1802_TRACE_LOG(adr, data, type)
#endif

class CDP1802 {
	using u8 = uint8_t;
	using u16 = uint16_t;
public:
	CDP1802();
	void Reset();
	void SetMemoryPtr(u8 *p) { m = p; }
	int Execute(int n);
private:
	void setR(u8 index, u16 data) {
		r[index] = data;
		CDP1802_TRACE_LOG(index, data, acsStoreR);
	}
	// memory and I/O customize -- start
	u8 imm8() {
		u8 o = m[r[p]++];
#if CDP1802_TRACE
		if (tracep->opn < 3) tracep->op[tracep->opn++] = o;
#endif
		return o;
	}
	u8 ld(u16 adr) {
		u8 data = m[adr];
		CDP1802_TRACE_LOG(adr, data, acsLoad);
		return data;
	}
	void st(u16 adr, u8 data) {
		m[adr] = data;
		CDP1802_TRACE_LOG(adr, data, acsStore);
	}
	u8 ef(u8 num) { return 0; }
	u8 input(u8 port) { return 0; }
	void Q() { printf("Q=%d\n", q); }
	void output(u8 port, u8 data) {
		if (port == 5 || port == 7) {
			putchar(data);
			fflush(stdout);
		}
		else printf("OUT(%d)=%02x\n", port, data);
	}
	// memory and I/O customize -- end
	u8 *m;
	u16 r[16];
	u8 d, x, p, df, q, t, ie;
#if CDP1802_TRACE
	static constexpr int TRACEMAX = 10000;
	static constexpr int ACSMAX = 1;
	enum {
		acsLoad = 1, acsStore, acsStoreR
	};
	struct Acs {
		u16 adr, data;
		u8 type;
	};
	struct TraceBuffer {
		u16 pc;
		u8 op[3];
		u8 d, df, p, x, index, opn;
		Acs acs[ACSMAX];
	};
	TraceBuffer tracebuf[TRACEMAX];
	TraceBuffer *tracep;
public:
	void StopTrace();
#endif
};
