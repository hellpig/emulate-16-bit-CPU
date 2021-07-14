/*
    Emulate a hypothetical very-simple 16-bit CPU!
    I got the idea from:  https://github.com/CodingKraken/K88

    My goal isn't to exactly replicate a certain architecture,
    but to just capture the main ideas of a CPU!
    I'd rather play around with hypothetical architectures to understand
    the main idea of how CPUs, assembly languages, and machine code work
    than worry about the details of any specific implementation.
    My goal is similar to the goal of this amazing project by Ben Eater...
      https://www.youtube.com/watch?v=dXdoim96v5A&list=PLowKtXNTBypGqImE405J2565dvjafglHU&index=36
    in that I don't care to replicate any specific architecture,
    except, unlike Ben Eater, I don't have to buy any hardware!
    This brilliant talk by Richard Feynman also shares my goal...
      https://www.youtube.com/watch?v=EKWGGDXe5MA

    Just read through this file to find all of my documentation!
    Then try to write your own program in assembly and machine code.
    For you to learn, I hope you'll have to create some CPU instructions to do it!

    I haven't even implemented all the instructions yet because you'll probably
    want to define them differently.
    I only implemented the instructions necessary to run the Fibonacci-sequence program
    found below.

    (c) 2021 Bradley Knockel
*/


#include <iostream>
#include <unistd.h>   // for usleep() (Windows can get unistd.h via Cygwin or Mingw-w64)

const int millisecondsPerInstruction = 50;



/*
    The following 2 functions are for setting or reading flags,
    which are the following labeled bits of a uint16_t register...
        FEDCBA9876543210

    Flags...
        0:  1 if comparison is greater than
        1:  1 if comparison is equal to
        2:  1 if comparison is less than

    In the following functions, bitpos varies from 0 to 15 to match the above labels.
*/

uint16_t getbit(uint16_t num, int bitpos) {
    uint16_t mask = 1 << bitpos;
    return (num & mask);
}

void setbit(uint16_t &num, int bitpos, bool set) {
    uint16_t mask = 1 << bitpos;
    if(set)
        num |= mask;
    else
        num &= ~mask;
}





/*
    The following 4 functions are for interpreting the 4 nibbles of the 
    first uint16_t of an instruction...
       1111 2222 3333 4444
    For example, getNibble2 will return
       0000 0000 0000 2222
*/

uint16_t getNibble1(uint16_t num) {
    return (num >> 12);
}

uint16_t getNibble2(uint16_t num) {
    return (num >> 8) & 0x000F;
}

uint16_t getNibble3(uint16_t num) {
    return (num >> 4) & 0x000F;
}

uint16_t getNibble4(uint16_t num) {
    return (num & 0x000F);
}







/*
    ROM is where the program's machine code will be put.
    Think of it as flash memory that is only written to when a program is assembled.
    ROM can also contain data used for initializing variables.
    0xFFFF = 2^16 = 65536 is the max that 16-bit addresses can address.
    I decided to make a uint16_t (instead of a uint8_t) the fundamental memory chunk.
*/
uint16_t ROM[0xFFFF];


/*
    RAM is where the program can read and write.
    RAM is erased then randomly set whenever the emulated CPU is reset
        (whenever you rerun this code).
    0xFFFF = 2^16 = 65536 is the max that 16-bit addresses can address.
    I decided to make a uint16_t (instead of a uint8_t) the fundamental memory chunk.
*/
uint16_t RAM[0xFFFF];


/*
    Register must be uint16_t since this is a 16-bit computer!
        reg[0] is program counter
        reg[1] is flags
        reg[2] is register 2
        reg[3] is register 3
        reg[4] is register 4
    Feel free to make more registers! No more than 16 for compatibility with my default instruction set.

    All are initialized to 0
*/
uint16_t reg[5] = {0};




/*
    My instructions in machine code will all be 2 uint16_t values...
      (1) first uint16_t specifies opcode (4 bits) and up to 3 modes, registers, or flags (4 bits each)
      (2) many instructions will require a uint16_t RAM or ROM address
    Feel free to change these conventions!

    Assembly code for my instruction set of 4-bit opcodes...
        0:  ADD A B C    --> add registers A and B; store in register C
        1:  SUB A B C    --> subtract register B from register A; store in register C
        2:  NOT A        --> inverts the contents of register A
        3:  AND A B      --> set flag based on (A and B)
        4:  OR A B       --> set flag based on (A or B)
        5:  CMP A B      --> set flags by comparing registers A to B
        6:  CPY A B      --> copy register A to B
        7:  OUT A        --> prints register A
        8:  MOV A, RAM   --> copy register A to RAM address
        9:  LD A, RAM    --> copy value in RAM address to register A
        A:  LDV A, VAL   --> copy value (in ROM) to register A
        B:
        C:
        D:
        E:  J MODE FLAG, ROM --> jump program counter to ROM address if FLAG is...
              0  <-- MODE = 0
              1  <-- MODE = 1
              either  <-- MODE = 2
        F:  HLT          --> halt until CPU is reset
    Feel free to change the above!

    Maybe CMP and OUT commands could have an option to interpret the integers as *signed*?
    If I didn't want to cast to the int16_t type and just use uint16_t instead... 
     - ADD and SUB don't need to be changed because two's complement signed integers
        add and subtract exactly like unsigned integers.
     - To compare, equality is the same, but comparing numbers with different sign bits is different:
        the number with the sign bit is always smaller than the other one.
     - To output a negative number, the sign bit (1) prints a negative sign
        then (2) prints (32768 - (number in remaining 15 bits))

    To get floating point numbers, half-precision (FP16) is not implemented on modern CPUs,
    so emulating this would be difficult!

    Examples in assembly code and machine code (* means the nibble is irrelevant)...
        ADD 2 3 4        --> adds registers 2 and 3 into 4
            0x0234
            0x****
        MOV 4, 0x0000    --> copies register 4 to RAM[0x0000]
            0x84**
            0x0000
        LDV 3, 0x0001    --> copies 0x0001 from ROM into register 3
            0xA3**
            0x0001
        J 1 2, 0x0000    --> sets program counter to 0x0000 if flag register's bit2 is 1
            0xE12*
            0x0000
        HLT
            0xF***
            0x****

    If you wanted, you could also create lines of assembly that are not CPU instructions!
    For example...
            SET VAL ROM
    which could store data in ROM at assembly time.

*/

bool halt = false;

void runInstruction(uint16_t instruction, uint16_t address) {

    uint16_t n1,n2,n3,n4;
    n1 = getNibble1(instruction);   // opcode
    n2 = getNibble2(instruction);
    n3 = getNibble3(instruction);
    n4 = getNibble4(instruction);

    reg[0] += 2;  // program counter increments to next instruction

    switch (n1) {

      /* ADD */
      case 0x0:
        reg[n4] = reg[n2] + reg[n3];
        break;

      /* CMP */
      case 0x5:
        setbit(reg[1], 0, 0);
        setbit(reg[1], 1, 0);
        setbit(reg[1], 2, 0);
        if (reg[n2] > reg[n3])
            setbit(reg[1], 0, 1);
        else if (reg[n2] == reg[n3])
            setbit(reg[1], 1, 1);
        else if (reg[n2] < reg[n3])
            setbit(reg[1], 2, 1);
        break;

      /* CPY */
      case 0x6:
        reg[n3] = reg[n2];
        break;

      /* OUT */
      case 0x7:
        std::cout << reg[n2] << std::endl;
        break;

      /* LDV */
      case 0xA:
        reg[n2] = address;
        break;

      /* J */
      case 0xE:
        if (!n2) {
            if (!getbit(reg[1], n3))  reg[0] = address;
        } else if (n2 == 1) {
            if (getbit(reg[1], n3))  reg[0] = address;
        } else {
            reg[0] = address;
        }
        break;

      /* undefined instructions are HLT */
      default:
        halt = true;
        break;

    }

}





int main() {

    // initialize ROM[] to HLT
    for (int i=0; i < 0xFFFF; i++)
        ROM[i] = 0xFFFF;



    /*
      Assembly code to generate the Fibonacci sequence...
        LDV 2, 0x0000
        LDV 3, 0x0001
        ADD 2 3 4
        OUT 4
        CPY 3 2
        CPY 4 3
        ADD 2 3 4
        CMP 4 3
        J 1 0, 0x0006
        HLT
      The corresponding assembled machine code follows...
    */
    ROM[0] = 0xA200;
    ROM[1] = 0x0000;
    ROM[2] = 0xA300;
    ROM[3] = 0x0001;
    ROM[4] = 0x0234;
    ROM[5] = 0x0000;
    ROM[6] = 0x7400;
    ROM[7] = 0x0000;
    ROM[8] = 0x6320;
    ROM[9] = 0x0000;
    ROM[10] = 0x6430;
    ROM[11] = 0x0000;
    ROM[12] = 0x0234;
    ROM[13] = 0x0000;
    ROM[14] = 0x5430;
    ROM[15] = 0x0000;
    ROM[16] = 0xE100;
    ROM[17] = 0x0006;



    while(!halt) {
        runInstruction( ROM[reg[0]], ROM[reg[0]+1] );
        usleep( millisecondsPerInstruction * 1000 );

        //std::cout << reg[0] << ' ' << reg[1] << ' ' << reg[2] << ' ' << reg[3] << ' ' << reg[4] << std::endl;
    }

    return 0;
}
