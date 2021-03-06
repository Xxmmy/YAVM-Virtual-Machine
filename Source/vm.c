#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "vm.h"

typedef struct
{
	int * bp; //AKA locals, bp[0] points to argument 1
	int * pc; //address of old code
	int * sp; //current top of the stack
}Frame;

#define MAX_STACK 1024
#define MAX_METHODS 64

static YAVM_Method methods[MAX_METHODS];
static int globals[1024];
static int stack[MAX_STACK] = {15}; // used for locals and args

static float * fbp;

Frame currentFrame;
static Frame callStack[MAX_STACK];// *call* stack used for function calling/returning
static Frame * csp = callStack; 

int retValue; 

#define SIGN24 0x800000 // (2 ^ 24)/2
#define SIGN16 0x8000 // (2 ^ 16)/2

#define OP ((instruction) & 0xFF)			//opcode
#define RA currentFrame.bp[((instruction >> 8 ) & 0xFF)] //tos[rA] 
#define RB currentFrame.bp[((instruction >> 16) & 0xFF)] //tos[rB]
#define RC currentFrame.bp[((instruction >> 24) & 0xFF)] //tos[rC]

#define FRA fbp[((instruction >> 8 ) & 0xFF)]
#define FRB fbp[((instruction >> 16) & 0xFF)] //tos[rB]
#define FRC fbp[((instruction >> 24) & 0xFF)] //tos[rC]

#define IMM8    ((instruction >> 8) & 0xFF)
#define IMM16   ((instruction >> 16) & 0xFFFF)
#define SIMM16 (((instruction >> 16) & 0xFFFF) - SIGN16)
#define IMM24   ((instruction >> 8) & 0xFFFFFF)
#define SIMM24 (((instruction >> 8) & 0xFFFFFF) - SIGN24)


int YAVM_init(void)
{
	
	currentFrame.sp = stack;
	currentFrame.bp = stack;
	
}

int YAVM_setMethod(YAVM_Method method, int index)
{
	if(index > MAX_METHODS)
		return -1;
	methods[index] = method;
	return 0;
}

int YAVM_run(int * program)
{
	int numofinst = 0;
	int instruction;
	if(program == NULL)
		return -1;

	currentFrame.pc = program;

next_opcode:
	numofinst ++;
	instruction = *currentFrame.pc;
	fbp = (float*)currentFrame.bp;

	++currentFrame.pc;

	switch(OP){
	case ADD:
		RA = RB + RC;
		break;
	case SUB:
		RA = RB - RC;
		break;
	case MUL:
		RA = RB * RC;
		break;
	case DIV:
		RA = RB / RC;
		break;
	case MOD:
		RA = RB % RC;
		break;
	case SQRT:
		RA = (int)sqrt((float)RB);
		break;
	case EXP:
		RA = (int)pow((float)RB, RC);
		break;
	case ADDIMM:
		RA += IMM16;
		break;
	case SUBIMM:
		RA -= IMM16;
		break;
	case MULIMM:
		RA *= IMM16;
		break;
	case DIVIMM:
		RA /= IMM16;
		break;
	case MODIMM:
		RA %= IMM16;
		break;
	case SQRTIMM:
		RA = (int)sqrt((float)IMM16);
		break;
	case EXPIMM:
		RA = (int)pow((float)RA, IMM16);
		break;
	case FADD:
		FRA = FRB + FRC;
		break;
	case FSUB:
		FRA = FRB - FRC;
		break;
	case FMUL:
		FRA = FRB * FRC;
		break;
	case FDIV:
		FRA = FRB / FRC;
		break;
	case FMOD:
		FRA = fmod(FRB, FRC);
		break;
	case FSQRT:
		FRA = sqrt(FRB);
		break;
	case FEXP:
		FRA = pow(FRB, FRC);
		break;
	case PUSH:
		*(++currentFrame.sp) = RA;
		break;
	case POP:
		RA = *(currentFrame.sp--);
		break;
	case MOV:
		RA = RB;
		break;
	case LOADG:
		RA = globals[IMM16];
		break;
	case SETG:
		globals[IMM16] = RA;
		break;
	case MOVIMM16:
		RA = IMM16;
		break;
	case CMP:
		currentFrame.pc += (RA == RB);
		break;
	case GT:
		currentFrame.pc += (RA > RB) ? 1: 0;
		break;
	case GTE:
		currentFrame.pc += (RA >= RB) ? 1: 0;
		break;
	case JE:
		currentFrame.pc += RA ? SIMM16 : 0;
		break;
	case JMP:
		currentFrame.pc += SIMM24;
		break;
	case CALL:
		*(++csp) = currentFrame; // save current frame
		csp->sp-=methods[IMM16].argSize;
		//instead of removing pushed arguments via pop or subbing the 
		//stack pointer remove them here
		 /*set the new base pointer to consume the top argSize items + 1 
			turning them into locals */
		currentFrame.bp = (currentFrame.sp - methods[IMM16].argSize)+1;

		currentFrame.sp += methods[IMM16].localsSize;//TODO check for stack overflow
		currentFrame.pc = methods[IMM16].code;
		break;

	case RET:
		retValue = RA; //store the return value of this function in retvalue, TODO: possibly make a SETRET too.
		currentFrame = *(csp--); //restore previous frame
		break;
	case LOADRET:
		RA = retValue; // load the return value of last called function into RA
	
	case PRINT:
		printf("%d \n",RA);
		break;
	case END:
		return numofinst;
	}
	goto next_opcode;
}

int YAVM_setStack(int index, int value)
{
	
	if(index>MAX_STACK)
		return -1;
	stack[index] = value;
	return 0;

}

// method's arguments consume the top n positions of the stack where n are the # of args
// when a method is called a value is pushed always as the first argument that points 