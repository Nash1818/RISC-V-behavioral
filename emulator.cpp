#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <array>

// 64 MB space...
static constexpr uint64_t DRAM_SIZE = 64ULL * 1024 * 1024;   
// linker start
static constexpr uint64_t DRAM_BASE = 0x80000000ULL;         
static constexpr uint64_t UART_ADDR = 0x10000000ULL;         


class Memory {
public:
    explicit Memory(uint64_t size = DRAM_SIZE) : data(size, 0) {}

    template<typename T> T load(uint64_t addr) const {
        if (addr - DRAM_BASE >= data.size()) trap("load OOB");
        T val; std::memcpy(&val, &data[addr - DRAM_BASE], sizeof(T));
        return val;
    }
    template<typename T> void store(uint64_t addr, T val) {
        if (addr == UART_ADDR && sizeof(T) == 1) { std::putchar(char(val)); return; }
        if (addr - DRAM_BASE >= data.size()) trap("store OOB");
        std::memcpy(&data[addr - DRAM_BASE], &val, sizeof(T));
    }
    std::vector<uint8_t>& raw() { return data; }

private:
    [[noreturn]] static void trap(const char* msg){ std::fprintf(stderr,"MEM: %s\n",msg); std::exit(1); }
    std::vector<uint8_t> data;
};

// ─── tiny ELF loader (load PT_LOAD segments) ─────────
static uint64_t load_elf(const char* path, Memory& mem){
    std::ifstream f(path,std::ios::binary); if(!f){ std::perror("elf"); std::exit(1);}
    std::vector<char> buf((std::istreambuf_iterator<char>(f)),{});
    auto rd32=[&](size_t o){return *reinterpret_cast<uint32_t*>(&buf[o]);};
    auto rd64=[&](size_t o){return *reinterpret_cast<uint64_t*>(&buf[o]);};
    if(buf[4]!=2) { std::fprintf(stderr,"not 64‑bit ELF\n"); std::exit(1);}
    uint16_t phsz=*reinterpret_cast<uint16_t*>(&buf[54]), phnum=*reinterpret_cast<uint16_t*>(&buf[56]);
    uint64_t phoff=rd64(32), entry=rd64(24);
    for(uint16_t i=0;i<phnum;++i){
        size_t o=phoff+i*phsz;
        if(rd32(o)!=1) continue;                    // PT_LOAD
        uint64_t ofs=rd64(o+8), vaddr=rd64(o+16), sz=rd64(o+32);
        std::memcpy(&mem.raw()[vaddr-DRAM_BASE], &buf[ofs], sz);
    }
    return entry;
}

// ─── CPU core: RV64I + M (+ OP‑32/IMM‑32) ───────────
class CPU{
public:
    CPU(Memory& m,uint64_t pc0):mem(m),pc(pc0){ regs.fill(0);}
    bool step(){
        uint32_t ins = mem.load<uint32_t>(pc); pc+=4;
        decode_execute(ins); regs[0]=0; return running;
    }

private:
    // helpers
    static uint32_t fn3(uint32_t i){return (i>>12)&7;}
    static uint32_t fn7(uint32_t i){return i>>25;}
    static uint32_t rd (uint32_t i){return (i>>7)&31;}
    static uint32_t rs1(uint32_t i){return (i>>15)&31;}
    static uint32_t rs2(uint32_t i){return (i>>20)&31;}
    static int64_t  sx(uint64_t v,int b){return int64_t(v<< (64-b)) >> (64-b);}
    static uint64_t sx32(int32_t v){return uint64_t(int64_t(v));}

    void decode_execute(uint32_t ins){
        switch(ins & 0b1111111){
        case 0b0000011: op_load(ins);   break;
        case 0b0100011: op_store(ins);  break;
        case 0b0010011: op_opimm(ins);  break;
        case 0b0110011: op_op(ins);     break;
        case 0b0011011: op_opimm32(ins);break;
        case 0b0111011: op_op32(ins);   break;
        case 0b1100011: op_branch(ins); break;
        case 0b1101111: op_jal(ins);    break;
        case 0b1100111: op_jalr(ins);   break;
        case 0b0110111: regs[rd(ins)] = sx(ins & 0xFFFFF000ULL,32); break; // LUI
        case 0b0010111: regs[rd(ins)] = pc-4 + sx(ins & 0xFFFFF000ULL,32);break;// AUIPC
        case 0b1110011: op_system(ins); break;
        default: die("unknown opcode");
        }
    }

    // ── groups ───────────────────────────────────────
    void op_load(uint32_t i){
        uint64_t a = regs[rs1(i)] + sx(i>>20,12);

        //

        switch (fn3(i)) {
            case 0x0: regs[rd(i)] = sx(mem.load<uint8_t >(a), 8 );  break; // LB
            case 0x1: regs[rd(i)] = sx(mem.load<uint16_t>(a),16 );  break; // LH
            case 0x2: regs[rd(i)] = sx(mem.load<uint32_t>(a),32 );  break; // LW
            case 0x3: regs[rd(i)] = mem.load<uint64_t>(a);      break; // LD
            case 0x4: regs[rd(i)] = mem.load<uint8_t >(a);  break; // LBU
            case 0x5: regs[rd(i)] = mem.load<uint16_t>(a);  break; // LHU
            case 0x6: regs[rd(i)] = mem.load<uint32_t>(a);  break; // LWU
            default : die("bad LOAD");
            }
            
    }
    void op_store(uint32_t i){
        uint64_t imm=((i>>7)&0x1F)|((i>>25)<<5);
        uint64_t a = regs[rs1(i)] + sx(imm,12);
        switch(fn3(i)){
        case 0: mem.store<uint8_t >(a, regs[rs2(i)]); break;
        case 1: mem.store<uint16_t>(a, regs[rs2(i)]); break;
        case 2: mem.store<uint32_t>(a, regs[rs2(i)]); break;
        case 3: mem.store<uint64_t>(a, regs[rs2(i)]); break;
        default: die("bad STORE");
        }
    }
    /* ───── OP‑IMM (full RV64I set) ───── */
    void op_opimm(uint32_t i){
        uint64_t imm = sx(i>>20,12);
        switch (fn3(i)){
        case 0x0: regs[rd(i)] = regs[rs1(i)] + imm;                    break; // ADDI
        case 0x1: regs[rd(i)] = regs[rs1(i)] << (imm & 0x3F);          break; // SLLI
        case 0x2: regs[rd(i)] = (int64_t)regs[rs1(i)] < (int64_t)imm;  break; // SLTI
        case 0x3: regs[rd(i)] = regs[rs1(i)] < imm;                    break; // SLTIU
        case 0x4: regs[rd(i)] = regs[rs1(i)] ^ imm;                    break; // XORI
        case 0x5: regs[rd(i)] = (imm>>6) ? int64_t(regs[rs1(i)])>>(imm&0x3F)   // SRAI
                                         : regs[rs1(i)]>>(imm&0x3F);           // SRLI
                  break;
        case 0x6: regs[rd(i)] = regs[rs1(i)] | imm;                    break; // ORI
        case 0x7: regs[rd(i)] = regs[rs1(i)] & imm;                    break; // ANDI
        }
    }
    /* OP‑IMM‑32 (ADDIW, SLLIW, SRLIW, SRAIW) */
    void op_opimm32(uint32_t i){
        uint32_t imm = i>>20, rd_=rd(i), rs1_=rs1(i);
        switch(fn3(i)){
        case 0: regs[rd_] = sx32(int32_t(regs[rs1_] + sx(imm,12)));          break; // ADDIW
        case 1: regs[rd_] = sx32(int32_t(regs[rs1_] << (imm & 0x1F)));       break; // SLLIW
        case 5: regs[rd_] = sx32(int32_t(((imm>>5)? int32_t(regs[rs1_])
                                                     : uint32_t(regs[rs1_])) >> (imm&0x1F))); break;
        default: die("OP‑IMM‑32 unimp");
        }
    }


    // Todo: fix: in case of Mem:Load OOp error and M-unimp ascii and not implemented functions... 

    /* OP (reg‑reg) — includes M‑extension ops */
    void op_op(uint32_t i){
        uint32_t f3=fn3(i), f7=fn7(i);

    if (f7 == 1) {                          // RV64M, 64‑bit results
        switch (f3) {
        case 0:   /* MUL   */ regs[rd(i)] = int64_t(regs[rs1(i)]) *
                                            int64_t(regs[rs2(i)]);                     break;
        case 1:   /* MULH  */ regs[rd(i)] = (__int128(int64_t(regs[rs1(i)])) *
                                            __int128(int64_t(regs[rs2(i)]))) >> 64;    break;  // ★ NEW
        case 2:   /* MULHSU*/ regs[rd(i)] = (__int128(int64_t(regs[rs1(i)])) *
                                            __int128(uint64_t(regs[rs2(i)]))) >> 64;   break;  // ★ NEW
        case 3:   /* MULHU */ regs[rd(i)] = (__int128(uint64_t(regs[rs1(i)])) *
                                            __int128(uint64_t(regs[rs2(i)]))) >> 64;   break;  // ★ NEW
        case 4:   /* DIV   */ regs[rd(i)] = regs[rs2(i)] ?
                                            int64_t(regs[rs1(i)]) / int64_t(regs[rs2(i)])
                                        : uint64_t(-1);                              break;
        case 5:   /* DIVU  */ regs[rd(i)] = regs[rs2(i)] ? regs[rs1(i)] / regs[rs2(i)]
                                                        : uint64_t(-1);               break;
        case 6:   /* REM   */ regs[rd(i)] = regs[rs2(i)] ?
                                            int64_t(regs[rs1(i)]) % int64_t(regs[rs2(i)])
                                        : regs[rs1(i)];                              break;
        case 7:   /* REMU  */ regs[rd(i)] = regs[rs2(i)] ? regs[rs1(i)] % regs[rs2(i)]
                                                        : regs[rs1(i)];               break;
        default:    die("M‑ext unimp");
        }
        return;
    }

        switch(f3){                              
        case 0: regs[rd(i)]=(f7==0x20)? regs[rs1(i)]-regs[rs2(i)]
                                     : regs[rs1(i)]+regs[rs2(i)];             break;
        case 1: regs[rd(i)]=regs[rs1(i)]<<(regs[rs2(i)]&0x3F);                 break;
        case 2: regs[rd(i)]=(int64_t)regs[rs1(i)]<(int64_t)regs[rs2(i)];       break;
        case 4: regs[rd(i)]=regs[rs1(i)] ^ regs[rs2(i)];                       break;
        case 5: regs[rd(i)]=(f7==0x20)? int64_t(regs[rs1(i)])>>(regs[rs2(i)]&0x3F)
                                     : regs[rs1(i)]>>(regs[rs2(i)]&0x3F);      break;
        case 6: regs[rd(i)]=regs[rs1(i)] | regs[rs2(i)];                       break;
        case 7: regs[rd(i)]=regs[rs1(i)] & regs[rs2(i)];                       break;
        }
    }

    /* OP‑32 — includes word‑size M‑ops */
    void op_op32(uint32_t i){
        uint32_t rd_=rd(i), rs1_=rs1(i), rs2_=rs2(i), f3=fn3(i), f7=fn7(i);
        if(f7==0x01){                           /* ★ NEW: RV64M word ops */
            switch(f3){
            case 0: regs[rd_]=sx32(int32_t(int32_t(regs[rs1_]) * int32_t(regs[rs2_]))); break; // MULW
            case 4: regs[rd_]=sx32(rs2_ ? int32_t(regs[rs1_]) / int32_t(regs[rs2_]) : -1);   break; // DIVW
            case 5: regs[rd_]=sx32(rs2_ ? uint32_t(regs[rs1_]) / uint32_t(regs[rs2_]) : uint32_t(-1)); break; // DIVUW
            case 6: regs[rd_]=sx32(rs2_ ? int32_t(regs[rs1_]) % int32_t(regs[rs2_]) : int32_t(regs[rs1_])); break; // REMW
            case 7: regs[rd_]=sx32(rs2_ ? uint32_t(regs[rs1_]) % uint32_t(regs[rs2_]) : uint32_t(regs[rs1_])); break; // REMUW
            default: die("OP‑32 M unimp");
            }
            return;                            /* ★ NEW */
        }
        switch(f3){                            /* RV64I word ops */
        case 0: regs[rd_]=sx32(int32_t((f7==0x20)? regs[rs1_] - regs[rs2_]
                                                  : regs[rs1_] + regs[rs2_]));      break;
        case 1: regs[rd_]=sx32(int32_t(regs[rs1_] << (regs[rs2_]&0x1F)));           break;
        case 5: regs[rd_]=sx32(int32_t((f7==0x20)? int32_t(regs[rs1_])>>(regs[rs2_]&0x1F)
                                                  : uint32_t(regs[rs1_])>>(regs[rs2_]&0x1F))); break;
        default: die("OP‑32 unimp");
        }
    }

    void op_branch(uint32_t i){
        int32_t imm = ((i>>7 &1)<<11)|((i>>8 &0xF)<<1)|((i>>25&0x3F)<<5)|((i>>31)<<12);
        imm = sx(imm,13);
        bool t=false; switch(fn3(i)){
        case 0:t=regs[rs1(i)]==regs[rs2(i)];break;
        case 1:t=regs[rs1(i)]!=regs[rs2(i)];break;
        case 4:t=int64_t(regs[rs1(i)])< int64_t(regs[rs2(i)]);break;
        case 5:t=int64_t(regs[rs1(i)])>=int64_t(regs[rs2(i)]);break;
        default: die("branch unimp");
        }
        if(t) pc=pc-4+imm;
    }
    void op_jal(uint32_t i){
        int32_t imm=((i>>21&0x3FF)<<1)|((i>>20&1)<<11)|((i>>12&0xFF)<<12)|((int32_t)i>>31<<20);
        regs[rd(i)]=pc; pc=pc-4+sx(imm,21);
    }
    void op_jalr(uint32_t i){
        uint64_t t=pc; pc=(regs[rs1(i)]+sx(i>>20,12))&~1ULL; regs[rd(i)]=t;
    }
    void op_system(uint32_t i){
        if(fn3(i)==0 && (i>>20)==0) running=false;
        else die("SYSTEM unimp");
    }

    [[noreturn]] static void die(const char* m){ std::fprintf(stderr,"*** fatal: %s\n",m); std::exit(1); }

    Memory& mem; uint64_t pc; std::array<uint64_t,32> regs{}; bool running=true;
};

// ─── main ────────────────────────────────────────────
int main(int argc,char** argv){
    if(argc<2){ std::fprintf(stderr,"usage: %s prog.elf\n",argv[0]); return 1;}
    Memory mem; uint64_t entry=load_elf(argv[1],mem);
    CPU cpu(mem,entry); while(cpu.step()); return 0;
}
