// CDP1802
// Copyright 2024 © Yasuo Kuwahara
// MIT License

#include "CDP1802.h"
#include <cstdlib>
#include <cstring>

CDP1802::CDP1802() {
#if CDP1802_TRACE
	memset(tracebuf, 0, sizeof(tracebuf));
	tracep = tracebuf;
#endif
}

void CDP1802::Reset() {
	memset(r, 0, sizeof(r));
	d = x = p = df = q = t = ie = 0;
}

#define mkc(x, c)	(d = t16 = (x) + ((c) && df), df = (t16 & 0x100) != 0)
#define mkb(x, b)	(d = t16 = (x) - ((b) && !df), df = !(t16 & 0x100))
#define br(x)		(r[p] = (x) ? (r[p] & 0xff00) | imm8() : r[p] + 1)
#define lbr(x)		(r[p] = (x) ? (t8 = imm8(), t8 << 8 | imm8()) : r[p] + 2)

int CDP1802::Execute(int n) {
	int clock = 0;
	do {
#if CDP1802_TRACE
		tracep->pc = r[p];
		tracep->p = p;
		tracep->index = tracep->opn = 0;
#endif
		u16 t16;
		u8 t8, op = imm8();
		switch (op) {
			case 0x00:
#if CDP1802_TRACE
				StopTrace();
#else
				exit(0);
#endif
				break;
			           case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
			case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
				d = ld(r[op & 0xf]); break; // ldn
			case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
			case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
				setR(op & 0xf, r[op & 0xf] + 1); break; // inc
			case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
			case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
				setR(op & 0xf, r[op & 0xf] - 1); break; // dec
			case 0x30: br(1); break; // br
			case 0x31: br(q); break; // bq
			case 0x32: br(!d); break; // bz
			case 0x33: br(df); break; // bdf
			case 0x34: br(ef(1)); break; // b1
			case 0x35: br(ef(2)); break; // b2
			case 0x36: br(ef(3)); break; // b3
			case 0x37: br(ef(4)); break; // b4
			case 0x38: br(0); break; // nbr
			case 0x39: br(!q); break; // bnq
			case 0x3a: br(d); break; // bnz
			case 0x3b: br(!df); break; // bnf
			case 0x3c: br(!ef(1)); break; // bn1
			case 0x3d: br(!ef(2)); break; // bn2
			case 0x3e: br(!ef(3)); break; // bn3
			case 0x3f: br(!ef(4)); break; // bn4
			case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
			case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
				d = ld(r[op & 0xf]++); break; // lda
			case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
			case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
				st(r[op & 0xf], d); break; // str
			case 0x60: setR(x, r[x] + 1); break; // irx
			           case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
				output(op & 7, ld(r[x]++)); // out
			           case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
				d = input(op & 7); st(r[x], d); break; // inp
			case 0x70: t8 = ld(r[x]++); x = t8 >> 4; p = t8 & 0xf; ie = 1; break; // ret
			case 0x71: t8 = ld(r[x]++); x = t8 >> 4; p = t8 & 0xf; ie = 0; break; // dis
			case 0x72: d = ld(r[x]++); break; // ldxa
			case 0x73: st(r[x]--, d); break; // stxd
			case 0x74: mkc(d + ld(r[x]), 1); break; // adc
			case 0x75: mkb(ld(r[x]) - d, 1); break; // sdb
			case 0x76: t8 = df != 0; df = d & 1; d = t8 << 7 | d >> 1; break; // shrc(rshr)
			case 0x77: mkb(d - ld(r[x]), 1); break; // smb
			case 0x78: st(r[x], t); break; // sav
			case 0x79: t = x << 4 | (p & 0xf); x = p; st(r[2]--, t); break; // mark
			case 0x7a: q = 0; Q(); break; // req
			case 0x7b: q = 1; Q(); break; // seq
			case 0x7c: mkc(d + imm8(), 1); break; // adci
			case 0x7d: mkb(imm8() - d, 1); break; // sdbi
			case 0x7e: mkc(d << 1, 1); break; // shlc(rshl)
			case 0x7f: mkb(d - imm8(), 1); break; // smbi
			case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
			case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
				d = r[op & 0xf]; break; // glo
			case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
			case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
				d = r[op & 0xf] >> 8; break; // ghi
			case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
			case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
				setR(op & 0xf, (r[op & 0xf] & 0xff00) | d); break; // plo
			case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
			case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
				setR(op & 0xf, d << 8 | (r[op & 0xf] & 0xff)); break; // phi
			case 0xc0: lbr(1); break; // lbr
			case 0xc1: lbr(q); break; // lbq
			case 0xc2: lbr(!d); break; // lbz
			case 0xc3: lbr(df); break; // lbdf
			case 0xc4: break; // nop
			case 0xc5: if (!q) r[p] += 2; break; // lsnq
			case 0xc6: if (d) r[p] += 2; break; // lsnz
			case 0xc7: if (!df) r[p] += 2; break; // lsnf
			case 0xc8: r[p] += 2; break; // lskp
			case 0xc9: lbr(!q); break; // lbnq
			case 0xca: lbr(d); break; // lbnz
			case 0xcb: lbr(!df); break; // lbnf
			case 0xcc: if (ie) r[p] += 2; break; // lsie
			case 0xcd: if (q) r[p] += 2; break; // lsq
			case 0xce: if (!d) r[p] += 2; break; // lsz
			case 0xcf: if (df) r[p] += 2; break; // lsdf
			case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd7:
			case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
				p = op & 0xf; break; // sep
			case 0xe0: case 0xe1: case 0xe2: case 0xe3: case 0xe4: case 0xe5: case 0xe6: case 0xe7:
			case 0xe8: case 0xe9: case 0xea: case 0xeb: case 0xec: case 0xed: case 0xee: case 0xef:
				x = op & 0xf; break; // sex
			case 0xf0: d = ld(r[x]); break; // ldx
			case 0xf1: d |= ld(r[x]); break; // or
			case 0xf2: d &= ld(r[x]); break; // and
			case 0xf3: d ^= ld(r[x]); break; // xor
			case 0xf4: mkc(d + ld(r[x]), 0); break; // add
			case 0xf5: mkb(ld(r[x]) - d, 0); break; // sd
			case 0xf6: df = d & 1; d >>= 1; break; // shr
			case 0xf7: mkb(d - ld(r[x]), 0); break; // sm
			case 0xf8: d = imm8(); break; // ldi
			case 0xf9: d |= imm8(); break; // ori
			case 0xfa: d &= imm8(); break; // ani
			case 0xfb: d ^= imm8(); break; // xri
			case 0xfc: mkc(d + imm8(), 0); break; // adi
			case 0xfd: mkb(imm8() - d, 0); break; // sdi
			case 0xfe: mkc(d << 1, 0); break; // shl
			case 0xff: mkb(d - imm8(), 0); break; // smi
			default: fprintf(stderr, "Illegal op:%02x\n", op); exit(1);
		}
#if CDP1802_TRACE
		tracep->d = d;
		tracep->x = x;
		tracep->df = df;
#if CDP1802_TRACE > 1
		if (++tracep >= tracebuf + TRACEMAX - 1) StopTrace();
#else
		if (++tracep >= tracebuf + TRACEMAX) tracep = tracebuf;
#endif
#endif
		clock += (op & 0xf0) == 0xc0 ? 24 : 16;
	} while (clock < n);
	return clock - n;
}

#if CDP1802_TRACE
#include <string>
void CDP1802::StopTrace() {
	TraceBuffer *endp = tracep;
	int i = 0, j;
	FILE *fo;
	if (!(fo = fopen((std::string(getenv("HOME")) + "/Desktop/trace.txt").c_str(), "w"))) exit(1);
	do {
		if (++tracep >= tracebuf + TRACEMAX) tracep = tracebuf;
		fprintf(fo, "%4d %d %04X ", i++, tracep->p, tracep->pc);
		for (j = 0; j < 3; j++) fprintf(fo, j < tracep->opn ? "%02X " : "   ", tracep->op[j]);
		fprintf(fo, "%02X %d %X ", tracep->d, tracep->df, tracep->x);
		for (Acs *p = tracep->acs; p < tracep->acs + tracep->index; p++) {
			switch (p->type) {
				case acsLoad:
					fprintf(fo, "L %04X %02X ", p->adr, p->data);
					break;
				case acsStore:
					fprintf(fo, "S %04X %02X ", p->adr, p->data);
					break;
				case acsStoreR:
					fprintf(fo, "%04X->R%X ", p->data, p->adr);
					break;
			}
		}
		fprintf(fo, "\n");
	} while (tracep != endp);
	fclose(fo);
	fprintf(stderr, "trace dumped.\n");
	exit(1);
}
#endif	// CDP1802_TRACE
