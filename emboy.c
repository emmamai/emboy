#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define ROMSIZE 1048576

#define u8 uint8_t
#define u16 uint16_t
#define dreg(x, y) union{struct{u8 y;u8 x;};u16 x##y;};

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
	uint16_t sp;
	uint16_t pc;
} regs;

#define f( z, n, h, c ) regs.fz=z;regs.fn=n;regs.fh=h;regs.fc=c;

u8* ri[] = {&regs.b,&regs.c,&regs.d,&regs.e,&regs.h,&regs.l,0,&regs.a};

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
uint8_t rom[ROMSIZE];
uint8_t iram[8192];

uint8_t mem_read( uint16_t addr ) {
	return 	addr < 0x8000 ? rom[addr] :
			addr < 0xA000 ? 0 :
			addr < 0xC000 ? 0 :
			addr < 0xFE00 ? iram[addr & 0x1FFF] :
			addr < 0xFEA0 ? 0 :
			addr < 0xFF80 ? 0 :
			addr < 0xFFFF ? 0 :
			0;
}

uint16_t mem_read16( uint16_t address  ) {
	return ( mem_read( address + 1 ) << 8 ) + mem_read( address ); // interrupt enable flag
}

void mem_write( uint16_t address, uint8_t value ) {
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

uint8_t reg_read_indirect( ireg_t r ) {
	if ( r == IREG_HL )
		return mem_read( regs.hl );
	return *ri[r];
}

void reg_write_indirect( ireg_t r, uint8_t value ) {
	if ( r == IREG_HL )
		mem_write( regs.hl, value );
	else
		*ri[r] = value;
}


void alu_op( uint8_t op, ireg_t r ) {
	uint8_t carry = regs.f;
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

#define push(v) mem_write(sp--,val);
#define pop(v) v=mem_read(sp++);

void cpu_run_cycle( void ) {
	uint8_t opcode = mem_read( regs.pc );
	uint8_t op_selector = ( opcode >> 3 ) & 0x07;
	uint8_t reg_selector = opcode & 0x07;
	uint16_t a, b;

	printf( "Cycle Start - PC: %04x AF: %04x BC: %04x DE: %04x HL: %04x (HL): %04x SP: %04x imm8: %02x, imm16: %02x Ins: %02x\n",
			regs.pc, regs.af, regs.bc, regs.de, regs.hl, mem_read( regs.hl ), regs.sp,
			mem_read( regs.pc + 1 ), mem_read16( regs.pc + 1 ), opcode );

	if ( opcode <  0x40 ) {
		switch( opcode & 0x07 ) {
			case 0x00: //done? concerned about this one
				if ( opcode & 0x28 != 0 ) {
					regs.pc++;
					return;
				}
				a = 1;
				if ( opcode == 0x08 )
					mem_write( mem_read16( regs.pc++ ), regs.sp );
				else 
					b = mem_read( regs.pc+1 );
					a = opcode & 0x20 ? 
							opcode & 0x10 ? 
								opcode & 0x08 ? regs.fc : !regs.fc 
								: opcode & 0x08 ? regs.fz : !regs.fz
						: b;
				regs.pc += (int8_t)a;
				return;
			case 0x01: //done
				if ( opcode & 0x80 ) {
					*(&regs.bc + ( ( ( opcode & 0x30 ) >> 4 ))) = mem_read16( regs.pc + 1 );
					regs.pc += 3;
				} else {
					regs.hl = ( a = regs.hl ) + *(&regs.bc + ( ( ( opcode & 0x30 ) >> 4 )));
					f( regs.fz, 0, regs.hl < a, (regs.hl&0xF0) != (a&0xF0) )
					regs.pc += 1;
				}
				return;
			case 0x02: //done
				if ( opcode & 0x08 )
					regs.a = mem_read( opcode & 0x20 ? regs.hl : opcode & 0x10 ? regs.de : regs.bc );
				else
					mem_write( opcode & 0x20 ? regs.hl : opcode & 0x10 ? regs.de : regs.bc, regs.a );
				regs.pc++;
				return;
			case 0x03: //done
				*(&regs.bc + ( ( ( opcode & 0x30 ) >> 4 ))) += opcode & 0x8 ? -1 : 1;
				regs.pc++;
				return;
			case 0x04: //done
			case 0x05: //done
				a = reg_read_indirect( op_selector ), b = a;
				reg_write_indirect( op_selector, a + ( opcode & 0x01 ? -1 : 1 ) );
				f( a==0, opcode & 0x01,  (a&0xF0) != (b&0xf0), regs.fc );
				regs.pc++;
				return;
			case 0x06: //done
				reg_write_indirect( op_selector, mem_read( regs.pc+1 ) );
				regs.pc+=2;
				return;
			case 0x07:
			default:
				break;
		}

		
	} else if ( opcode < 0x80 ) {
		if ( opcode == 0x76 ) // HALT
			return;
		reg_write_indirect( op_selector, reg_read_indirect( reg_selector ) );
		regs.pc++;
		return;
	} else if ( opcode < 0xC0 ) {
		alu_op( op_selector, reg_read_indirect( reg_selector ) );
		regs.pc++;
		return;
	} else {
		switch ( opcode ) {
			case 0xC3:
				regs.pc = mem_read16( regs.pc + 1 );
				return;
			default:
				break;
		}

	}
	printf( "bad opcode %02x\n", opcode );
	exit( -1 );
}

int main( int argc, char** argv ) {
	FILE* f = fopen( argv[1], "r" );

	printf( "%s:%ib\n", argv[1], fread( rom, 1, ROMSIZE, f ) );
	while (1) { cpu_run_cycle(); usleep( argc > 2 ? 500000 : 2 ); };
}
