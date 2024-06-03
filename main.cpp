#include <fstream>
#include <iostream>
#include <vector>

#define INF 1000000000

using namespace std;

enum opcode {  // enum of opcode
    ADD,
    SUB,
    MUL,
    INC,
    AND,
    OR,
    XOR,
    NOT,
    SLLI,
    SRLI,
    LI,
    LD,
    ST,
    JMP,
    BEQZ,
    HLT
};

void print(vector<int> &v) {
    for (auto i : v) {
        cout << i << " ";
    }
    cout << "\n";
}

int main() {
    vector<int> RF(16), D$(256), I$(256);
    ifstream in;
    in.open("./input/RF.txt");  // opening RF.txt
    int idx = 0;
    string str;
    while (in >> hex >> RF[idx]) {  // reading input in RF array
        idx++;
    }
    RF[0] = 0;
    in.close();
    in.open("./input/DCache.txt");  // opening DCache.txt
    idx = 0;
    while (in >> hex >> D$[idx]) {  // reading input in D$ array
        idx++;
    }
    in.close();
    in.open("./input/ICache.txt");  // opening ICache.txt
    idx = 0;
    while (in >> hex >> I$[idx]) {  // taking input in I$ array
        idx++;
    }
    in.close();
    int IF = INF;  // initialising variables to default values
    int ID = INF;
    int EX = INF;
    int MEM = INF;
    int WB = INF;
    int IR = INF;
    int PC = 0;
    int ALUOutput = INF;
    int ALUOutput_ = INF;
    int A = INF;
    int B = INF;
    int A_ = INF;
    int B_ = INF;
    int LMD = INF;
    bool HALT_FLAG = false;           // HALT_FLAG is raised when HALT command is encountered
    bool STALL = false;               // STALL flag is raised then there is RAW hazard
    bool CONDITIONAL_BRANCH = false;  // CONDITIONAL_BRANCH flag is raised if command BEQZ is encountered
    bool NON_COND_BRANCH = false;     // NON_COND_BRANCH flag is raised if command JMP is encountered
    vector<int> reg_in_use(16, 0);    // value in reg_in_use shows how many writes to register is yet to happen
    int instructions = 0;
    int arith_inst = 0;
    int logical_inst = 0;
    int sft_inst = 0;
    int mem_inst = 0;
    int li_inst = 0;
    int ctrl_inst = 0;
    int hlt_inst = 0;
    int cycle = 0;
    int stall = 0;
    int c_stall = 0;
    int d_stall = 0;
    auto state_change = [&IF, &ID, &EX, &MEM, &WB, &STALL](int IF_) -> void {  // this function changes the state of instruction in cyclic order i.e form IF -> ID -> EX -> MEM -> WB
        if (STALL) {                                                           // if there is RAW stall we do not forward instruction in ID to EX;
            WB = MEM;
            MEM = EX;
            EX = INF;
            IF = IF_;
            return;
        }
        WB = MEM;
        MEM = EX;
        EX = ID;
        ID = IF;
        IF = IF_;
    };
    auto _IF_ = [&IR, &PC, &I$, &HALT_FLAG, &RF](int IF) -> void {
        IR = IF;
        PC += 2;
        switch (IF >> 12) {
            case HLT:
                HALT_FLAG = true;
                break;
        }
        PC = PC & 0xff;
    };
    auto _ID_ = [&RF, &A, &B, &reg_in_use, &CONDITIONAL_BRANCH, &IF, &A_, &B_, &STALL, &PC, &I$, &NON_COND_BRANCH, &d_stall](int ID) -> void {  // this function does instruction decoding, it identifies the register from input has to be read
        A_ = INF;
        B_ = INF;
        switch (ID >> 12) {
            case ADD:
            case SUB:
            case MUL:
            case AND:
            case OR:
            case XOR:
                A_ = (ID & 0xf0) >> 4;
                A = RF[A_];
                B_ = ID & 0xf;
                B = RF[B_];
                break;
            case NOT:
                A = (ID & 0xf0) >> 4;
                A = RF[A];
                B = INF;
                break;
            case SLLI:
            case SRLI:
            case LD:
                A_ = (ID & 0xf0) >> 4;
                A = RF[A_];
                B_ = INF;
                break;
            case ST:
                A_ = (ID & 0xf0) >> 4;
                A = RF[A_];
                B_ = (ID & 0xf00) >> 8;
                break;
            case BEQZ:
                A_ = (ID & 0xf00) >> 8;
                A = RF[A_];
                B_ = INF;
                break;
            case INC:
                A_ = (ID & 0xf00) >> 8;
                A = RF[A_];
                B_ = INF;
                break;
        }
        if ((A_ != INF && reg_in_use[A_] != 0) || (B_ != INF && reg_in_use[B_] != 0)) {  // if any of the register from which input has to be taken are not weritten back the we raise STALL flag
            d_stall++;
            STALL = true;
            IF = INF;
            return;
        } else {
            STALL = false;
            if ((ID >> 12) != HLT) IF = (I$[PC] << 8) + I$[PC + 1];
        }
        if ((ID >> 12) <= LD) {  // if no STALL is then we increment the count of rd in reg_in_use register by one;
            reg_in_use[((ID & 0xf00) >> 8)]++;
        }
        if ((ID >> 12) == JMP) {  // if instruction is JMP the we set NON_COND_BRANCH flag and set the instruction in IF to default
            NON_COND_BRANCH = true;
            IF = INF;
        } else if ((ID >> 12) == BEQZ) {  // if instruction is BEQZ CONDITIONAL_BRANCH flag is set and set the instruction in IF to default
            CONDITIONAL_BRANCH = true;
            IF = INF;
        }
    };
    auto _EX_ = [&ALUOutput, &A, &B, &PC, &CONDITIONAL_BRANCH, &NON_COND_BRANCH](int EX) -> void {  // this finction preforms the execute state, i.e. it does all the ALU operation
        switch (EX >> 12) {
            case LD:
            case ST:
                ALUOutput = A + (EX & 0xf);
                break;
            case ADD:
                ALUOutput = A + B;
                break;
            case SUB:
                ALUOutput = A - B;
                break;
            case MUL:
                ALUOutput = A * B;
                break;
            case AND:
                ALUOutput = A & B;
                break;
            case OR:
                ALUOutput = A | B;
                break;
            case XOR:
                ALUOutput = A ^ B;
                break;
            case INC:
                ALUOutput = A + 1;
                break;
            case NOT:
                ALUOutput = ~A;
                break;
            case SLLI:
                ALUOutput = A << (EX & 0xf);
                break;
            case SRLI:
                ALUOutput = A >> (EX & 0xf);
                break;
            case LI:
                ALUOutput = EX & 0xff;
                break;
            case BEQZ:
                ALUOutput = (A == 0) ? PC + (EX & 0xff) * 2 : PC;
                break;
            case JMP:
                ALUOutput = PC + ((EX & 0xff0) >> 4) * 2;
        }
        ALUOutput = ALUOutput & 0xff;                                       // ensuring that ALUOutput is a 8 bit number
        if (ALUOutput & 0x80 && !CONDITIONAL_BRANCH && !NON_COND_BRANCH) {  // ALUOperation is a signed then we make ALUOutput a negative number in 32 bit register, if ALUOutput is for breanch instruction we take it as unsigned as it represets PC which is unsigned
            ALUOutput = ALUOutput | 0xffffff00;
        }
    };
    auto _MEM_ = [&RF, &ALUOutput, &D$, &LMD, &ALUOutput_, &CONDITIONAL_BRANCH, &IF, &I$, &PC, &NON_COND_BRANCH](int MEM) -> void {  // here all the all the memory operation are done
        switch (MEM >> 12) {
            case LD:
                LMD = D$[ALUOutput];  // reading from D$
                break;
            case ST:
                D$[ALUOutput] = (RF[(MEM & 0xf00) >> 8]) & 0xff;  // writing to D$
                break;
            case BEQZ:
                CONDITIONAL_BRANCH = false;       // if CONDITIONAL_BRANCH we set we unset it, as now we can take another instruction in IF stage
                PC = ALUOutput;                   // changing PC
                IF = (I$[PC] << 8) + I$[PC + 1];  // assiging the stalled instruction to IF
                break;
            case JMP:
                NON_COND_BRANCH = false;          // if NON_COND_BRANCH we set we unset it, as now we can take another instruction in IF stage
                PC = ALUOutput;                   // changing PC
                IF = (I$[PC] << 8) + I$[PC + 1];  // assiging the stalled instruction to IF
                break;
        }
        ALUOutput_ = ALUOutput;
    };
    auto _WB_ = [&LMD, &ALUOutput_, &RF, &reg_in_use, &instructions, &arith_inst, &logical_inst, &sft_inst, &mem_inst, &ctrl_inst, &hlt_inst, &c_stall, &li_inst](int WB) -> void {  // in this function write back to registers happens
        instructions++;                                                                                                                                                              // incrementing the number of instructions
        switch (WB >> 12) {
            case ADD:
            case SUB:
            case MUL:
            case INC:
            case AND:
            case OR:
            case XOR:
            case NOT:
            case SLLI:
            case SRLI:
            case LI:
                RF[(WB & 0xf00) >> 8] = ALUOutput_;  // writing to rd
                reg_in_use[(WB & 0xf00) >> 8]--;     // decrementing the count of rd in reg_in_use as one write back happened so number of times to be written to register is one less
                break;
            case LD:
                RF[(WB & 0xf00) >> 8] = LMD;      // writing to rd
                reg_in_use[(WB & 0xf00) >> 8]--;  // decrementing the count of rd in reg_in_use as one write back happened so number of times to be written to register is one less
                break;
        }
        int i = (WB >> 12);
        if (i >= ADD && i <= INC) {
            arith_inst++;  // instruction is arithmatic then increasing count of arith_inst
        } else if (i >= AND && i <= NOT) {
            logical_inst++;  // instruction is logical then increasing count of logical_inst
        } else if (i >= SLLI && i <= SRLI) {
            sft_inst++;  // instruction is shift then increasing count of sft_inst
        } else if (i == LI) {
            li_inst++;  // instruction is load immediate then increasing count of li_inst
        } else if (i >= LD && i <= ST) {
            mem_inst++;  // instruction is memory instruction then increasing count of mem_inst
        } else if (i >= JMP && i <= BEQZ) {
            ctrl_inst++;   // instruction is control instruction then increasing count of ctrl_inst
            c_stall += 2;  // if instruction is control instruction then increasing the count of c_stall by 2 as per control instruction there is 2 stalls
        } else if (i == HLT) {
            hlt_inst++;  // // instruction is halt then increasing count of hlt_inst
        }
    };
    while (true) {
        cycle++;                                                                                                        // counting the number of cycles
        state_change((HALT_FLAG | CONDITIONAL_BRANCH | STALL | NON_COND_BRANCH) ? INF : ((I$[PC] << 8) + I$[PC + 1]));  // calling state change instruction, if either of the flag is raised we pass the default value to IF else we pass the instruction value at PC to IF
        if (WB != INF) {                                                                                                // if WB is not default then we call _WB_ function
            _WB_(WB);
        }
        if (MEM != INF) {  // if MEM is not default then we call _MEM_ function
            _MEM_(MEM);
        }
        if (EX != INF) {  // if EX is not default then we call _EX_ function
            _EX_(EX);
        }
        if (ID != INF) {  // if ID is not default then we call _ID_ function
            _ID_(ID);
        }
        if (IF != INF) {  // if IF is not default then we call _IF_ function
            _IF_(IF);
        }
        if (WB == INF && MEM == INF && EX == INF && ID == INF && IF == INF && HALT_FLAG) {  // if WB MEM EX ID IF all have default value and halt flag is raised the we break out of loop
            break;
        }
    }
    cycle--;                    // as cycle was incremented even when every instruction in every stage was in default value so we decrement it
    stall = d_stall + c_stall;  // total stall is sum of data stall and control stall
    ofstream out_, out;
    out_.open("./output/DCache.txt");  // opening DCache.txt in output folder
    for (auto i : D$) {                // writing to DCache.txt
        out_ << hex << (i >> 4) << (i & 0xf);
        out_ << "\n";
    }
    out_.close();
    out.open("./output/Output.txt");  // opening Output.txt in output folder and writing to it
    out << "Total number of instructions executed        : " << instructions << "\n";
    out << "Number of instructions in each class\n";
    out << "Arithmetic instructions                      : " << arith_inst << "\n";
    out << "Logical instructions                         : " << logical_inst << "\n";
    out << "Shift instructions                           : " << sft_inst << "\n";
    out << "Memory instructions                          : " << mem_inst << "\n";
    out << "Load immediate instructions                  : " << li_inst << "\n";
    out << "Control instructions                         : " << ctrl_inst << "\n";
    out << "Halt instructions                            : " << hlt_inst << "\n";
    out << "Cycles Per Instruction                       : " << (((double)cycle) / instructions) << "\n";
    out << "Total number of stalls                       : " << stall << "\n";
    out << "Data stalls (RAW)                            : " << d_stall << "\n";
    out << "Control stalls                               : " << c_stall << "\n";
    out.close();
}