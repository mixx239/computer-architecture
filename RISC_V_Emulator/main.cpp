#include <iostream>
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>


const int MEMORY_SIZE = 512 * 1024;
const int CACHE_SIZE = 2 * 1024;
const int CACHE_INDEX_LEN = 4;
const int CACHE_OFFSET_LEN = 5;
const int CACHE_LINE_SIZE = 1 << CACHE_OFFSET_LEN;   
const int CACHE_SET_COUNT = 1 << CACHE_INDEX_LEN; 
const int CACHE_LINE_COUNT = CACHE_SIZE / CACHE_LINE_SIZE;
const int CACHE_WAY = CACHE_LINE_COUNT / CACHE_SET_COUNT;
const int ADDRESS_LEN = 19;
const int CACHE_TAG_LEN = ADDRESS_LEN - CACHE_INDEX_LEN - CACHE_OFFSET_LEN;

uint32_t regs[32];
uint32_t pc = 0;

uint64_t total_accesses_lru = 0;
uint64_t hits_lru = 0;
uint64_t total_accesses_bp = 0;
uint64_t hits_bp = 0;
uint64_t total_instr_lru = 0;
uint64_t hits_instr_lru = 0;
uint64_t total_data_lru = 0;
uint64_t hits_data_lru = 0;
uint64_t total_instr_bp = 0;
uint64_t hits_instr_bp = 0;
uint64_t total_data_bp = 0;
uint64_t hits_data_bp = 0;

struct Register {
    uint32_t pc;
    uint32_t x[31];
};

struct MemoryFragment {
    uint32_t address;
    std::vector<uint8_t> data;
};

struct CacheLine {
    uint32_t tag;
    uint8_t data[CACHE_LINE_SIZE]; 
    bool valid;
    bool dirty; 
};

struct InputData {
    Register reg;
    std::vector<MemoryFragment> memory;
    bool has_output = false;
    std::string output_file;
    uint32_t output_addr = 0;
    uint32_t output_size = 0;
};

uint32_t lru_counter[CACHE_SET_COUNT][CACHE_WAY] = {};
CacheLine cache_lru[CACHE_SET_COUNT][CACHE_WAY];

uint8_t bp_state[CACHE_SET_COUNT] = {};
CacheLine cache_bp[CACHE_SET_COUNT][CACHE_WAY];


InputData Parser(int argc, char* argv[]) {
    InputData input;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            std::ifstream file(argv[i+1], std::ios::binary);
            if (!file) {
                std::cerr << "cannot open input file\n";
                break;
            }

            file.read(reinterpret_cast<char*>(&input.reg.pc), sizeof(uint32_t));
            for (int j = 0; j < 31; ++j) {
                file.read(reinterpret_cast<char*>(&input.reg.x[j]), sizeof(uint32_t));
            }
            while (file.peek() != EOF) {
                MemoryFragment fragment;
                uint32_t addr, size;
                
                file.read(reinterpret_cast<char*>(&addr), sizeof(uint32_t));
                file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));

                fragment.address = addr;
                fragment.data.resize(size);
                file.read(reinterpret_cast<char*>(fragment.data.data()), size);
                
                input.memory.push_back(fragment);
            }
            ++i;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 3 < argc) {
            input.output_file = argv[i+1];
            input.output_addr = std::stoul(argv[i+2], nullptr, 16);
            input.output_size = std::stoul(argv[i+3]);
            input.has_output = true;
            i += 3;
        }
    }
    return input;
}


int cache_access_lru(uint32_t addr, bool is_inst, std::vector<uint8_t>& memory) {
    uint32_t index = (addr >> CACHE_OFFSET_LEN) & (CACHE_SET_COUNT - 1);
    uint32_t tag = addr >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN);

    total_accesses_lru++;
    if (is_inst) total_instr_lru++;
    else total_data_lru++;

    for (int way = 0; way < CACHE_WAY; ++way) {
        CacheLine& line = cache_lru[index][way];
        if (line.valid && (line.tag == tag)) {
            hits_lru++;
            if (is_inst) hits_instr_lru++;
            else hits_data_lru++;

            for (int i = 0; i < CACHE_WAY; ++i)
                if (i != way)
                    lru_counter[index][i]++;
            lru_counter[index][way] = 0;

            return way;
        }
    }

    int new_way = 0;
    for (int way = 1; way < CACHE_WAY; ++way) {
        if (lru_counter[index][way] > lru_counter[index][new_way])
            new_way = way;
    }

    CacheLine& current_line = cache_lru[index][new_way];

    if (current_line.valid && current_line.dirty) {
        uint32_t current_addr = (current_line.tag << (CACHE_INDEX_LEN + CACHE_OFFSET_LEN)) | (index << CACHE_OFFSET_LEN);
        std::memcpy(memory.data() + current_addr, current_line.data, CACHE_LINE_SIZE);
    }

    std::memcpy(current_line.data, memory.data() + addr, CACHE_LINE_SIZE);
    current_line.tag = tag;
    current_line.valid = true;
    current_line.dirty = false;

    for (int i = 0; i < CACHE_WAY; ++i)
        if (i != new_way)
            lru_counter[index][i]++;
    lru_counter[index][new_way] = 0;

    return new_way;
}



int cache_access_bp(uint32_t addr, bool is_inst, std::vector<uint8_t>& memory) {
    uint32_t index = (addr >> CACHE_OFFSET_LEN) & ((1 << CACHE_INDEX_LEN) - 1);
    uint32_t tag = addr >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN);

    total_accesses_bp++;
    if (is_inst) total_instr_bp++;
    else total_data_bp++;

    for (int way = 0; way < CACHE_WAY; ++way) {
        CacheLine& line = cache_bp[index][way];
        if (line.valid && line.tag == tag) {
            hits_bp++;
            if (is_inst) hits_instr_bp++;
            else hits_data_bp++;

            bp_state[index] |= (1 << way);
            return way;
        }
    }


    int new_way = -1;
    for (int way = 0; way < CACHE_WAY; ++way) {
        if ((bp_state[index] & (1 << way)) == 0) {
            new_way = way;
            break;
        }
    }

    if (new_way == -1) {
        bp_state[index] = 0;
        new_way = 0;
    }

    CacheLine& current_line = cache_bp[index][new_way];

    if (current_line.valid && current_line.dirty) {
        uint32_t current_addr = (current_line.tag << (CACHE_INDEX_LEN + CACHE_OFFSET_LEN)) | (index << CACHE_OFFSET_LEN);
        std::memcpy(memory.data() + current_addr, current_line.data, CACHE_LINE_SIZE);
    }

    std::memcpy(current_line.data, memory.data() + addr, CACHE_LINE_SIZE);
    current_line.tag = tag;
    current_line.valid = true;
    current_line.dirty = false;

    bp_state[index] |= (1 << new_way);

    return new_way;
}



uint32_t read_lru(uint32_t addr, std::vector<uint8_t>& memory, bool is_inst) {
    int way = cache_access_lru(addr, is_inst, memory);
    uint32_t index = (addr >> CACHE_OFFSET_LEN) & ((1 << CACHE_INDEX_LEN) - 1);
    uint32_t offset = addr & (CACHE_LINE_SIZE - 1);
    return *reinterpret_cast<uint32_t*>(&cache_lru[index][way].data[offset]);
}

void write_lru(uint32_t addr, uint32_t value, std::vector<uint8_t>& memory) {
    int way = cache_access_lru(addr, false, memory);
    uint32_t index = (addr >> CACHE_OFFSET_LEN) & ((1 << CACHE_INDEX_LEN) - 1);
    uint32_t offset = addr & (CACHE_LINE_SIZE - 1);

    CacheLine& line = cache_lru[index][way];
    std::memcpy(&line.data[offset], &value, sizeof(uint32_t));
    line.dirty = true;
}

uint32_t read_bp(uint32_t addr, std::vector<uint8_t>& memory, bool is_inst) {
    int way = cache_access_bp(addr, is_inst, memory);
    uint32_t index = (addr >> CACHE_OFFSET_LEN) & ((1 << CACHE_INDEX_LEN) - 1);
    uint32_t offset = addr & (CACHE_LINE_SIZE - 1);
    return *reinterpret_cast<uint32_t*>(&cache_bp[index][way].data[offset]);
}

void write_bp(uint32_t addr, uint32_t value, std::vector<uint8_t>& memory) {
    int way = cache_access_bp(addr, false, memory);
    uint32_t index = (addr >> CACHE_OFFSET_LEN) & ((1 << CACHE_INDEX_LEN) - 1);
    uint32_t offset = addr & (CACHE_LINE_SIZE - 1);
    CacheLine& line = cache_bp[index][way];
    std::memcpy(&line.data[offset], &value, sizeof(uint32_t));
    line.dirty = true;
}




void execute(uint32_t instr, std::vector<uint8_t>& memory, bool is_lru) {
    uint32_t opcode = instr & 0x7F;
    uint32_t rd = (instr >> 7) & 0x1F;
    uint32_t funct3 = (instr >> 12) & 0x7;
    uint32_t rs1 = (instr >> 15) & 0x1F;
    uint32_t rs2 = (instr >> 20) & 0x1F;
    uint32_t funct7 = (instr >> 25) & 0x7F;

    int32_t imm_i = (int32_t)(instr) >> 20;
    int32_t imm_s = ((int32_t)(instr & 0xFE000000) >> 20) | ((instr >> 7) & 0x1F);
    int32_t imm_b = ((instr >> 31) << 12) | (((instr >> 7) & 1) << 11) | (((instr >> 25) & 0x3F) << 5) | (((instr >> 8) & 0xF) << 1);
    if (imm_b & (1 << 12)) imm_b |= 0xFFFFE000;
    int32_t imm_u = instr & 0xFFFFF000;
    int32_t imm_j = ((instr >> 31) << 20) | (((instr >> 12) & 0xFF) << 12) | (((instr >> 20) & 1) << 11) | (((instr >> 21) & 0x3FF) << 1);
    if (imm_j & 0x100000) imm_j |= 0xFFE00000;

    switch (opcode) {
        case 0x13:
        switch (funct3) {
            case 0x0: regs[rd] = regs[rs1] + imm_i; break; // ADDI
            case 0x2: regs[rd] = (int32_t)regs[rs1] < imm_i; break; // SLTI
            case 0x3: regs[rd] = regs[rs1] < (uint32_t)imm_i; break; // SLTIU
            case 0x4: regs[rd] = regs[rs1] ^ imm_i; break; // XORI
            case 0x6: regs[rd] = regs[rs1] | imm_i; break; // ORI
            case 0x7: regs[rd] = regs[rs1] & imm_i; break; // ANDI
            case 0x1: regs[rd] = regs[rs1] << (imm_i & 0x1F); break; // SLLI
            case 0x5: {
                if (funct7 == 0x00)
                    regs[rd] = regs[rs1] >> (imm_i & 0x1F); // SRLI
                else if (funct7 == 0x20)
                    regs[rd] = (int32_t)regs[rs1] >> (imm_i & 0x1F); // SRAI
                break;
            }
        }
        break;

        case 0x33:
        switch (funct3) {
            case 0x0: {
                if (funct7 == 0x00)
                    regs[rd] = regs[rs1] + regs[rs2]; // ADD
                else if (funct7 == 0x20)
                    regs[rd] = regs[rs1] - regs[rs2]; // SUB
                else if (funct7 == 0x01)
                    regs[rd] = regs[rs1] * regs[rs2]; // MUL
                break;
            }
            case 0x1: {
                if (funct7 == 0x00)
                    regs[rd] = regs[rs1] << (regs[rs2] & 0x1F); // SLL
                else if (funct7 == 0x01)
                    regs[rd] = ((int64_t)(int32_t)regs[rs1] * (int64_t)(int32_t)regs[rs2]) >> 32; // MULH
                break;
            }
            case 0x2: regs[rd] = (int32_t)regs[rs1] < (int32_t)regs[rs2]; break; // SLT
            case 0x3: regs[rd] = regs[rs1] < regs[rs2]; break; // SLTU
            case 0x4: {
                if (funct7 == 0x00) regs[rd] = regs[rs1] ^ regs[rs2]; // XOR
                else if (funct7 == 0x01) { // DIV
                    if (regs[rs2] != 0) {
                        regs[rd] = (int32_t)regs[rs1] / (int32_t)regs[rs2];
                    } else {
                        regs[rd] = -1;
                    }
                }
                break;
            }
            case 0x5: {
                if (funct7 == 0x00)
                    regs[rd] = regs[rs1] >> (regs[rs2] & 0x1F); // SRL
                else if (funct7 == 0x20)
                    regs[rd] = (int32_t)regs[rs1] >> (regs[rs2] & 0x1F); // SRA
                else if (funct7 == 0x01) { // DIVU
                    if (regs[rs2] != 0) {
                        regs[rd] = regs[rs1] / regs[rs2];
                    } else {
                        regs[rd] = 0xFFFFFFFF;
                    }
                }
                break;
            }
            case 0x6: {
                if (funct7 == 0x00)
                    regs[rd] = regs[rs1] | regs[rs2]; // OR
                else if (funct7 == 0x01) { // REM
                    if (regs[rs2] != 0) {
                        regs[rd] = (int32_t)regs[rs1] % (int32_t)regs[rs2]; 
                    } else {
                        regs[rd] = regs[rs1];
                    }
                }
                break;
            }
            case 0x7: {
                if (funct7 == 0x00)
                    regs[rd] = regs[rs1] & regs[rs2]; // AND
                else if (funct7 == 0x01) { // REMU
                    if (regs[rs2] != 0) {
                        regs[rd] = regs[rs1] % regs[rs2]; 
                    } else {
                        regs[rd] = regs[rs1];
                    }
                }
                break;
            }
        }
        break;


        case 0x03: {
            uint32_t addr = regs[rs1] + imm_i; 
            switch (funct3) {
                case 0x0:{ // LB
                    if (is_lru)
                        regs[rd] = read_lru(addr, memory, false) & 0xFF;
                    else 
                        regs[rd] = read_bp(addr, memory, false) & 0xFF;
                    if (regs[rd] & (1 << 7)) {
                        regs[rd] = regs[rd] | 0xFFFFFF00;
                    }
                    break;
                }
                case 0x1: { // LH
                    if (is_lru)
                        regs[rd] = read_lru(addr, memory, false) & 0xFFFF;
                    else 
                        regs[rd] = read_bp(addr, memory, false) & 0xFFFF;
                    if (regs[rd] & (1 << 15)) {
                        regs[rd] = regs[rd] | 0xFFFF0000;
                    }
                    break;
                }
                case 0x2: { // LW
                    if (is_lru)
                        regs[rd] = read_lru(addr, memory, false);
                    else 
                        regs[rd] = read_bp(addr, memory, false);
                    break;
                }
                case 0x4: { //LBU
                    if (is_lru)
                        regs[rd] = read_lru(addr, memory, false) & 0xFF;
                    else 
                        regs[rd] = read_bp(addr, memory, false) & 0xFF;
                    break;
                }
                case 0x5: { // LHU
                    if (is_lru)
                        regs[rd] = read_lru(addr, memory, false) & 0xFFFF;
                    else 
                        regs[rd] = read_bp(addr, memory, false) & 0xFFFF;
                    break;
                }
            }
        }
        break;

        case 0x23: {
            uint32_t addr = regs[rs1] + imm_s;
            switch (funct3) {
                case 0x0: { // SB
                    if (is_lru)
                        write_lru(addr, regs[rs2] & 0xFF, memory);
                    else 
                        write_bp(addr, regs[rs2] & 0xFF, memory);
                    break;
                }
                case 0x1: { // SH
                    if (is_lru)
                        write_lru(addr, regs[rs2] & 0xFFFF, memory);
                    else 
                        write_bp(addr, regs[rs2] & 0xFFFF, memory);
                    break;
                }
                case 0x2: { // SW
                    if (is_lru)
                        write_lru(addr, regs[rs2], memory);
                    else 
                        write_bp(addr, regs[rs2], memory);
                    break;
                }
            }
        }
        break;


        case 0x63:
        switch (funct3) {
            case 0x0: if (regs[rs1] == regs[rs2]) pc += imm_b - 4; break; // BEQ
            case 0x1: if (regs[rs1] != regs[rs2]) pc += imm_b - 4; break; // BNE
            case 0x4: if ((int32_t)regs[rs1] < (int32_t)regs[rs2]) pc += imm_b - 4; break; // BLT 
            case 0x5: if ((int32_t)regs[rs1] >= (int32_t)regs[rs2]) pc += imm_b - 4; break; // BGE
            case 0x6: if (regs[rs1] < regs[rs2]) pc += imm_b - 4; break; // BLTU
            case 0x7: if (regs[rs1] >= regs[rs2]) pc += imm_b - 4; break; // BGEU
        }
        break;

        case 0x37: regs[rd] = imm_u; break; // LUI
        case 0x17: regs[rd] = pc + imm_u; break; // AUIPC
        case 0x6F: regs[rd] = pc + 4; pc += imm_j - 4; break; // JAL
        case 0x67: regs[rd] = pc + 4; pc = (regs[rs1] + imm_i) & 0xFFFFFFFE; break; // JALR
    }

    regs[0] = 0;
}




int main(int argc, char* argv[]) {
    InputData input = Parser(argc, argv);
    
    std::vector<uint8_t> memory(MEMORY_SIZE, 0);
    
    for (int j = 0; j < 2; ++j) {
        for (auto fragment : input.memory) {
            std::copy(fragment.data.begin(), fragment.data.end(), memory.begin() + fragment.address);
        }

        pc = input.reg.pc;
        for (int i = 0; i < 31; ++i) {
            regs[i + 1] = input.reg.x[i];
        }
        regs[0] = 0;

        while (true) {
            uint32_t instr;
            if (j) 
                instr = read_lru(pc, memory, true);
            else
                instr = read_bp(pc, memory, true);
            
            if (instr == 0x00000073 || instr == 0x00100073) {
                break;
            }
            if (pc == regs[1]) { 
                break;
            }

            execute(instr, memory, j);
            pc += 4;
        }
    }
    for (int i = 0; i < CACHE_SET_COUNT; ++i) {
        for (int j = 0; j < CACHE_WAY; ++j) {
            CacheLine& line = cache_lru[i][j];
            if (line.valid && line.dirty) {
                uint32_t addr = (line.tag << (CACHE_INDEX_LEN + CACHE_OFFSET_LEN)) | (i << CACHE_OFFSET_LEN);
                std::memcpy(memory.data() + addr, line.data, CACHE_LINE_SIZE);
            }
        }
    }
        

    printf("replacement\thit rate\thit rate (inst)\thit rate (data)\n");
    printf("        LRU\t");
    if (total_accesses_lru != 0)
        printf("%3.5f%%\t", 100.0 * hits_lru / total_accesses_lru);
    else
        printf("nan%%\t");
    
    if (total_instr_lru != 0)
        printf("%3.5f%%\t", 100.0 * hits_instr_lru / total_instr_lru);
    else
        printf("nan%%\t");
    
    if (total_data_lru != 0)
        printf("%3.5f%%\n", 100.0 * hits_data_lru / total_data_lru);
    else
        printf("nan%%\n");
    
    
    printf("      bpLRU\t");
    if (total_accesses_bp != 0)
        printf("%3.5f%%\t", 100.0 * hits_bp / total_accesses_bp);
    else
        printf("nan%%\t");
    
    if (total_instr_bp != 0)
        printf("%3.5f%%\t", 100.0 * hits_instr_bp / total_instr_bp);
    else
        printf("nan%%\t");
    
    if (total_data_bp != 0)
        printf("%3.5f%%\n", 100.0 * hits_data_bp / total_data_bp);
    else
        printf("nan%%\n");
    

    if (input.has_output) {
        std::ofstream out(input.output_file, std::ios::binary);
        if (!out) {
            std::cerr << "cannot open output file\n";
        } else {
            out.write(reinterpret_cast<const char*>(&pc), sizeof(uint32_t));
            for (int i = 1; i < 32; ++i) {
                out.write(reinterpret_cast<const char*>(&regs[i]), sizeof(uint32_t));
            }
            out.write(reinterpret_cast<const char*>(&input.output_addr), sizeof(uint32_t));
            out.write(reinterpret_cast<const char*>(&input.output_size), sizeof(uint32_t));
            out.write(reinterpret_cast<const char*>(&memory[input.output_addr]), input.output_size);
        }
    }
        
    return 0;
}