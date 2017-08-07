/*  Anyone reading this:
This is BAD CODE and I'm writing it like that on purpose. 
I can and do write better code. Just not here. */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define ROMSIZE 1048576

#define u8 uint8_t
#define u16 uint16_t
#define dreg(x, y) union{struct{u8 y; u8 x;};u16 x##y;};

struct regs_s{ 
	union{struct{union{struct{
					u8 fl:4;
					u8 fc:1;
					u8 fh:1;
					u8 fn:1;
					u8 fz:1;
				};
				u8 f;
			};
			u8 a;
		};
		u16 af;
	};
	dreg(b,c)
	dreg(d,e)
	dreg(h,l)
	u16 sp;
	u16 pc;
	u8 ifl;
} regs;


union {
	struct {
		u8 z:3;
		u8 y:3;
		u8 x:2;
	};
	u8 i;
} opcode;

#define f( z, n, h, c ) regs.fz=(z);regs.fn=(n);regs.fh=(h);regs.fc=(c);

u8* ri[] = {&regs.b,&regs.c,&regs.d,&regs.e,&regs.h,&regs.l,0,&regs.a};
u16* rp[] = {&regs.bc,&regs.de,&regs.hl,&regs.sp};
u16* rp2[] = {&regs.bc,&regs.de,&regs.hl,&regs.af};

typedef enum {
	IREG_B = 0,
	IREG_C = 1,
	IREG_D = 2,
	IREG_E = 3,
	IREG_H = 4,
	IREG_L = 5,
	IREG_HL = 6,
	IREG_A = 7
} ireg_t;

typedef enum { 
	OP_ADD = 0,
	OP_ADC = 1,
	OP_SUB = 2,
	OP_SBC = 3,
	OP_AND = 4,
	OP_XOR = 5,
	OP_OR = 6,
	OP_CP = 7
} alu_op_t;

uint32_t romsize;
u8 rom[ROMSIZE];
u8 iram[8192];

u8 mem_read( u16 addr ) {
	return 	addr < 0x8000 ? rom[addr] :
			addr < 0xA000 ? 0 :
			addr < 0xC000 ? 0 :
			addr < 0xFE00 ? iram[addr & 0x1FFF] :
			addr < 0xFEA0 ? 0 :
			addr < 0xFF80 ? 0 :
			addr < 0xFFFF ? 0 :
			0;
}

#define mem_read16( addr ) ((mem_read(addr+1)<<8)+mem_read(addr))

void mem_write( u16 address, u8 value ) {
	if ( address < 0x8000 ) {
		; // ROM
	} else if ( address < 0xA000 ) {
		; // CharRAM, bg data
	} else if ( address < 0xC000 ) {
		; // CartRAM
	} else if ( address < 0xFE00 ) {
		iram[address & 0x1FFF] = value; // RAM
	} else if ( address < 0xFEA0 ) {
		; // OAM
	} else if ( address < 0xFF80 ) {
		; // hardware registers
	} else if ( address < 0xFFFF ) {
		; // zero page
	}
	return; // interrupt enable flag
}

void mem_write16( u16 address, u16 value ) {
	mem_write( address, (u8)(value & 0xFF) );
	mem_write( address+1, (u8)(value>>8)  );
}

u8 reg_read_indirect( ireg_t r ) {
	return r == IREG_HL ? mem_read( regs.hl ) : *ri[r];
}

void reg_write_indirect( ireg_t r, u8 value ) {
	if ( r == IREG_HL )
		mem_write( regs.hl, value );
	else
		*ri[r] = value;
}

void alu_op( u8 op, ireg_t r ) {
	u8 carry = regs.f;
	u16 a = regs.a, s = reg_read_indirect( r );
	u8 al = a & 0x0f, sl = s & 0x0f, hc;

	printf( "ALU op %02x\n", op );

	switch( op & 0x07 ) {
		case OP_ADD:
		case OP_ADC:
			regs.a = a = a + s + ( op & 0x1 & regs.f );
			regs.f = f( a & 0xFF == 0, 0, al + sl > 0x0F, a > 0xFF );
			break;
		case OP_SUB:
		case OP_SBC:
		case OP_CP:
			a = ( a - s ) - ( op & 0x1 & regs.f );
			regs.a = op & 0x4 ? regs.a : a ;
			f( a & 0xFF == 0, 1, al - sl > 0x0F, a > 0xFF );
			break;
		case OP_AND:
			regs.a = a = ( a & s );
			f( a & 0xFF == 0, 0, 1, 0 );
			break;
		case OP_XOR:
			regs.a = a = ( a ^ s );
			f( a & 0xFF == 0, 0, 0, 0 );
			break;
		case OP_OR:
			regs.a = a = ( a | s );
			f( a & 0xFF == 0, 0, 0, 0 );
			break;
			a = ( a - s ) - ( op & 0x1 & regs.f );
			f( a & 0xFF == 0, 1, al - sl > 0x0F, a > 0xFF );
			break;
		default:
			break;
	}
}

void cb( u8 opcode ) {
	printf( "cb unimplemented\n" );
	exit( 0 );
}

#define push(v) mem_write(regs.sp--,v);
#define pop(v) mem_read(regs.sp++)

void cpu_run_cycle( void ) {
	opcode.i = mem_read( regs.pc );
	u8 q = opcode.y & 0x01;
	u8 p = ( opcode.y >> 1 );
	u16 a, b;
	
	u8 nznc = ((!(p&1)&regs.fz)|(p&regs.fc))==q;

	printf( "Cycle Start - PC: %04x AF: %04x BC: %04x DE: %04x HL: %04x (HL): %04x SP: %04x imm8: %02x, imm16: %02x Ins: %02x\n",
			regs.pc, regs.af, regs.bc, regs.de, regs.hl, mem_read( regs.hl ), regs.sp,
			mem_read( regs.pc + 1 ), mem_read16( regs.pc + 1 ), opcode );

	if ( opcode.i <  0x40 ) { // 7/8
		switch( opcode.z ) { 
			case 0x00:
				if ( ( opcode.i & 0x28 ) == 0 ) {
					if ( ( opcode.i & 0x10 ) == 0 )
						regs.pc++;
					return;
				}
				a = 1;
				if ( opcode.i == 0x08 )
					mem_write( mem_read16( regs.pc++ ), regs.sp );
				else 
					b = mem_read( regs.pc+1 );
					a = (( p & 0x2 ? nznc : 1 ) ? b : 0) + 2;
				regs.pc += (int8_t)a;
				return;
			case 0x01: //done
				if ( opcode.i & 0x80 ) {
					regs.hl = ( a = regs.hl ) + *(&regs.bc + ( ( ( opcode.i & 0x30 ) >> 4 )));
					f( regs.fz, 0, regs.hl < a, (regs.hl&0xF0) != (a&0xF0) )
					regs.pc += 1;
				} else {
					*(&regs.bc + ( ( ( opcode.i & 0x30 ) >> 4 ))) = mem_read16( regs.pc + 1 );
					regs.pc += 3;
				}
				return;
			case 0x02: //done
				if ( opcode.i & 0x08 )
					regs.a = mem_read( opcode.i & 0x20 ? opcode.i&0x1?regs.hl--:regs.hl++ : opcode.i & 0x10 ? regs.de : regs.bc );
				else
					mem_write( opcode.i & 0x20 ? regs.hl : opcode.i & 0x10 ? regs.de : regs.bc, regs.a );
				regs.pc++;
				return;
			case 0x03: //done
				rp[p] += q ? -1 : 1;
				regs.pc++;
				return;
			case 0x04: //done
			case 0x05: //done
				a = reg_read_indirect( opcode.y ), b = a;
				a += ( opcode.i & 0x01 ? -1 : 1 );
				reg_write_indirect( opcode.y, a );
				f( (u8)a==0?1:0, opcode.i & 0x01,  (a&0xF0) != (b&0xf0), regs.fc );
				regs.pc++;
				return;
			case 0x06: //done
				reg_write_indirect( opcode.y, mem_read( regs.pc+1 ) );
				regs.pc+=2;
				return;
			case 0x07: //NOT DONE
			default:
				break;
		}
	} else if ( opcode.i < 0x80 ) { // DONE
		if ( opcode.i == 0x76 ) // HALT
			return;
		reg_write_indirect( opcode.y, reg_read_indirect( opcode.z ) );
		regs.pc++;
		return;
	} else if ( opcode.i < 0xC0 ) { // DONE
		alu_op( opcode.y, reg_read_indirect( opcode.z ) );
		regs.pc++;
		return;
	} else {
		switch ( opcode.z ) {
			case 0x00:
				if ( p & 0x2 ) {
					if ( p & 0x1 )
						regs.a = mem_read( q?mem_read16(++regs.pc):(0xff00+mem_read(regs.pc+1)) );
					else
						mem_write( q?mem_read16(++regs.pc):(0xff00+mem_read(regs.pc+1)), regs.a );
					regs.pc+=2;
				} else {
					if ( nznc )
						regs.pc = (pop()) + ((u16)(pop()) << 8);
					else
						regs.pc++;
				}
				return;
			case 0x01:
				if ( q )
					*rp2[opcode.x] = (pop()) + ((u16)(pop())<<8);
				else if ( p < 0x3 )
					regs.pc = p&2?(pop()) + ((u16)(pop()) << 8):mem_read(regs.hl);
				else
					regs.sp = regs.hl;
				regs.pc++;
				return;
			case 0x02:
				if ( p & 0x2 ) {
					if ( p & 0x1 )
						regs.a = mem_read( q ? mem_read16( ++regs.pc ) :  0xFF00 + regs.c );
					else
						mem_write16( q ? mem_read( ++regs.pc ) : 0xFF00 + regs.c, regs.a );
					regs.pc += 2;
				} else
					if ( nznc )
						regs.pc = mem_read16( regs.pc );
					else
						regs.pc += 3;
				return;
			case 0x03: //done
				if ( p & 0x2 )
					regs.ifl = q;
				if ( opcode.i == 0xcb )
					cb( opcode.i );
				regs.pc = opcode.i == 0xc3 ? mem_read16( regs.pc + 1 ) : regs.pc + 1;
				return;
			case 0x04:
				if ( nznc ) {
					push( regs.pc >> 8 )
					push( (u8)regs.pc )
					regs.pc = mem_read16( ++regs.pc );
				} else
					regs.pc++;
				return;
			case 0x05: //done
				if ( q ) {
					push( regs.pc >> 8 )
					push( (u8)regs.pc )
					regs.pc = mem_read16( ++regs.pc );
				} else {
					push( *rp[opcode.x] >> 8 )
					push( *rp[opcode.x] )
					regs.pc++;
				}
				return;
			case 0x06: //done
				alu_op( opcode.y, mem_read( regs.pc + 1 ) );
				regs.pc += 2;
				return;
			case 0x07: //done
				push( regs.pc >> 8 )
				push( (u8)regs.pc )
				regs.pc = opcode.y << 3;
				return;
			default:
				break;
		}

	}
	printf( "bad opcode %02x\n", opcode.i );
	exit( -1 );
}

int main( int argc, char** argv ) {
	FILE* f = fopen( argv[1], "r" );

	printf( "%s:%ik\n", argv[1], fread( rom, 1, ROMSIZE, f ) / 1024 );
	regs.af = 0x01b0;
	regs.bc = 0x0013;
	regs.de = 0x00d8;
	regs.hl = 0x014d;
	regs.sp = 0xfffe;
	regs.pc = 0x100;
	while (1) { cpu_run_cycle(); usleep( argc > 2 ? 500000 : 2 ); getchar(); };
}
