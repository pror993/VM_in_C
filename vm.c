#include <stdio.h>
#include <stdint.h>
#include <signal.h>
/* unix only */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

#define MEMORY_MAX (1<<16)
uint16_t memory[MEMORY_MAX];
//65536 locations in memory, each location is 16 bits wide

enum
{
    REG_R0 = 0,
    REG_R1,
    REG_R2,
    REG_R3,
    REG_R4,
    REG_R5,
    REG_R6,
    REG_R7,
    REG_PC, // program counter
    REG_COND, // condition flag
    REG_COUNT
};
uint16_t reg[REG_COUNT];


enum{
    FL_POS = 1 << 0, // P
    FL_ZRO = 1 << 1, // Z
    FL_NEG = 1 << 2, // N
};


enum
{
    OP_BR = 0, // branch
    OP_ADD, // add
    OP_LD, // load
    OP_ST, // store
    OP_JSR, // jump register
    OP_AND, // bitwise and
    OP_LDR, // load register
    OP_STR, // store register
    OP_RTI, // unused
    OP_NOT, // bitwise not
    OP_LDI, // load indirect
    OP_STI, // store indirect
    OP_JMP, // jump
    OP_RES, // reserved (unused)
    OP_LEA, // load effective address
    OP_TRAP // execute trap
};


//sign extend a value with bit_count bits to 16 bits, different for negative and positive numbers, if the number is negative, we need to fill the leftmost bits with 1s, if the number is positive, we need to fill the leftmost bits with 0s
uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

void update_flags(uint16_t r) //update the condition flags based on the value in the register r, if the value is 0, set the Z flag, if the value is negative, set the N flag, if the value is positive, set the P flag
{
    if (reg[r] == 0)
    {
        reg[REG_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[REG_COND] = FL_NEG;
    }
    else
    {
        reg[REG_COND] = FL_POS;
    }
}
uint16_t swap16(uint16_t x) {
    return (x << 8) | (x >> 8);
}

uint16_t sign_extend(uint16_t x, int bit_count);
void update_flags(uint16_t r);

struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

uint16_t mem_read(uint16_t address) { 
    if(address == 0xFE00) //keyboard status register
    {
        if(check_key()) {
            memory[address] = 1 << 15;
            char c = getchar(); 
            memory[0xFE02] = (uint16_t)c;
        } 
        else memory[address] = 0; 
    }
    return memory[address];
}

void mem_write(uint16_t address, uint16_t val) {
    memory[address] = val;
}
void read_image_file(FILE* file) {
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);
    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);
    while (read-- > 0) {
        *p = swap16(*p);
        ++p;
    }
}

int read_image(const char* image_path) {
    FILE* file = fopen(image_path, "rb");
    if (!file) {
        perror("Failed to open file");
        return 0;
    }
    read_image_file(file);
    fclose(file);
    return 1;
}


int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
    /* show usage string */
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

    for (int j = 1; j < argc; ++j)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }
    disable_input_buffering();

    //CAN ONLY SET ONE CONDITION FLAG AT A TIME, setting the Z flag
    reg[REG_COND] = FL_ZRO;


    //setting the PC to the starting position of the program, which is 0x3000 by default
    enum {PC_START = 0x3000};
    reg[REG_PC] = PC_START;

    int running = 1;
    while (running)
    {
        //fetch
        uint16_t instr = mem_read(reg[REG_PC]++);
        uint16_t op = instr >> 12; //opcode is the first 4 bits of the instruction

        switch (op)
        {
            case OP_ADD:
                {
                    uint16_t r0 = (instr >> 9) & 0x7; // destination register
                    uint16_t r1 = (instr >> 6) & 0x7; // first operand
                    uint16_t imm_flag = (instr >> 5) & 0x1; // immediate mode flag

                    if (imm_flag)
                    {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5); // immediate value
                        reg[r0] = reg[r1] + imm5;
                    }
                    else
                    {
                        uint16_t r2 = instr & 0x7; // second operand
                        reg[r0] = reg[r1] + reg[r2];
                    }
                    update_flags(r0);
                }
                break;
            case OP_AND:
                {
                    uint16_t r0 = (instr >> 9) & 0x7; // destination register
                    uint16_t r1 = (instr >> 6) & 0x7; // first source register
                    uint16_t imm_flag = (instr >> 5) & 0x1; // immediate mode flag

                    if (imm_flag)
                    {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5); // immediate value
                        reg[r0] = reg[r1] & imm5;
                    }
                    else
                    {
                        uint16_t r2 = instr & 0x7; // second operand
                        reg[r0] = reg[r1] & reg[r2];
                    }
                    update_flags(r0);

                }
                break;
            case OP_NOT:
                {
                    uint16_t r0 = (instr >> 9) & 0x7; // destination register
                    uint16_t r1 = (instr >> 6) & 0x7; 
                    reg[r0] = ~reg[r1];
                    update_flags(r0);
                }
                break;
            case OP_BR:
                {
                    uint16_t cond_flag = (instr >> 9) & 0x7;
                    uint16_t PC_offset = sign_extend(instr & 0x1FF, 9); 
                    if (cond_flag & reg[REG_COND]) reg[REG_PC]+=PC_offset;
                }
                break;
            case OP_JMP:
                {
                    uint16_t base_register = (instr >> 6) & 0x7;
                    reg[REG_PC]= reg[base_register];
                }
                break;
            case OP_JSR:
                {
                    reg[REG_R7] = reg[REG_PC];
                    uint16_t PC_offset = sign_extend(instr & 0x7FF, 11); 
                    uint16_t mode = (instr >> 11) & 0x1;

                    if(mode) reg[REG_PC]+=PC_offset;
                    else reg[REG_PC] = reg[instr >> 6 & 0x7];
                }
                break;
            case OP_LD:
                {
                    uint16_t DR = instr>>9 & 0x7 ;
                    uint16_t PC_offset = sign_extend(instr & 0x1FF, 9); 
                    reg[DR] = mem_read(reg[REG_PC] + PC_offset);
                    update_flags(DR);
                }
                break;
            case OP_LDI:        
                {
                    uint16_t DR = instr>>9 & 0x7 ;
                    uint16_t PC_offset = sign_extend(instr & 0x1FF, 9);
                    reg[DR] = mem_read(mem_read(reg[REG_PC] + PC_offset));
                    update_flags(DR);
                }
                break;
            case OP_LDR:
                {
                    uint16_t DR = instr>>9 &0x7;
                    uint16_t BaseR = instr>>6 &0x7;
                    uint16_t offset6 = sign_extend(instr & 0x3F, 6);
                    reg[DR]= mem_read(reg[BaseR]+offset6);
                    update_flags(DR);
                }
                break;
            case OP_LEA:
                {
                    uint16_t DR = instr>>9 & 0x7;
                    uint16_t PC_offset = sign_extend(instr & 0x1FF, 9);
                    reg[DR] = reg[REG_PC] + PC_offset;
                    update_flags(DR);
                }
                break;
            case OP_ST:
                {
                    uint16_t SR = instr>>9 & 0x7;
                    uint16_t PC_offset = sign_extend(instr & 0x1FF, 9);
                    mem_write(reg[REG_PC] + PC_offset, reg[SR]);
                }
                break;
            case OP_STI:
                {
                    uint16_t SR = instr>>9 & 0x7;
                    uint16_t PC_offset = sign_extend(instr & 0x1FF, 9);
                    mem_write(mem_read(reg[REG_PC] + PC_offset), reg[SR]);
                }
                break;  
            case OP_STR:
                {
                    uint16_t SR = instr>>9 & 0x7;
                    uint16_t BaseR = instr>>6 & 0x7;
                    uint16_t offset6 = sign_extend(instr & 0x3F, 6);
                    mem_write(reg[BaseR] + offset6, reg[SR]);
                }
                break;
            case OP_TRAP:
                {
                    reg[REG_R7] = reg[REG_PC];
                    uint16_t trapvect8 = instr & 0xFF;
                    switch(trapvect8)
                    {
                        case 0x20: //GETC
                            reg[REG_R0] = (uint16_t)getchar();
                            update_flags(REG_R0);   
                            break;
                        case 0x21: //OUT
                            putchar((char)reg[REG_R0]);
                            fflush(stdout);
                            break;
                        case 0x22: //PUTS   
                            {
                                uint16_t* c = memory + reg[REG_R0];
                                while(*c)
                                {
                                    putchar((char)*c);
                                    ++c;
                                }
                                fflush(stdout);
                            }
                            break;
                        case 0x23: //IN
                            {    
                                printf("Enter a character: ");
                                char c = getchar();
                                putchar(c);
                                reg[REG_R0] = (uint16_t)c;
                                update_flags(REG_R0);
                            }
                            break;
                        case 0x24: //PUTSP
                            {
                                uint16_t* c = memory + reg[REG_R0];
                                while(*c)
                                {
                                    char char1 = (*c) & 0xFF;
                                    putchar(char1);
                                    char char2 = (*c) >> 8;
                                    if(char2) putchar(char2);
                                    ++c;
                                }
                                fflush(stdout);
                            }
                            break;
                        case 0x25: //HALT
                            puts("HALT");
                            fflush(stdout);
                            running = 0;    
                            break;
                    }
                }
                break;
            case OP_RES:
            case OP_RTI:
            default:
                break;
        }
    }
    restore_input_buffering();
}


 