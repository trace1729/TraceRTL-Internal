#ifndef __TRACERTL_DUT_INFO_H__
#define __TRACERTL_DUT_INFO_H__

#define TRACERTL_FPGA_AXIS_WIDTH (512)
#define TRACERTL_FPGA_PACKET_INST_NUM (16)
#define TRACERTL_FPGA_PACKET_CYCLE_NUM (7)

#define TRACERTL_FPGA_COLLECT_INST_WIDTH (64)
#define TRACERTL_FPGA_COLLECT_INST_NUM (128)
#define TRACERTL_FPGA_COLLECT_CYCLE_NUM (16)

#define TRACERTL_INST_BIT_WIDTH (207)
#define TRACERTL_FPGA_INST_BIT_ALIGN_WIDTH (208)
#define TRACERTL_FPGA_INST_BYTE_WIDTH (26)

#define TRACERTL_INST_inst_BIT_WIDTH (32)
#define TRACERTL_INST_op2_BIT_WIDTH (36)
#define TRACERTL_INST_op1_BIT_WIDTH (50)
#define TRACERTL_INST_pcPAWoOff_BIT_WIDTH (36)
#define TRACERTL_INST_pcVA_BIT_WIDTH (50)
#define TRACERTL_INST_traceType_BIT_WIDTH (3)
struct __attribute__((packed)) TraceFpgaInstruction {
  uint32_t inst:32;
  uint64_t op2:36;
  uint64_t op1:50;
  uint64_t pcPAWoOff:36;
  uint64_t pcVA:50;
  uint8_t traceType:3;

  void genFrom(Instruction &instruction) {
    pcVA = instruction.instr_pc_va;
    pcPAWoOff = instruction.instr_pc_pa >> 12;
    inst = instruction.instr;

    if (instruction.branch_type != 0) traceType = 1;
    else if (instruction.memory_type != 0) {
      if (instruction.memory_type == 1 || instruction.memory_type == 4)
        traceType = 2;
      else
        traceType = 3;
    } else if (instruction.isTrap()) traceType = 4;
    else traceType = 0;

    if (traceType == 1) {
      op1 = instruction.target;
      op2 = instruction.branch_taken & 1;
    } else if (traceType == 2 || traceType == 3) {
      op1 = instruction.exu_data.memory_address.va;
      op2 = instruction.exu_data.memory_address.pa >> 12;
    } else if (traceType == 4) {
      op1 = instruction.target;
      op2 = instruction.exception;
    }
  }

  void dump(uint64_t instID) {
    printf("[%lx] traceType %02x pcVA %016lx pcPAWoOff %016lx op1 %016lx op2 %016lx inst %08x\n", instID, traceType, pcVA, pcPAWoOff, op1, op2, inst);
  }
};

#define TraceFpgaCollect_BIT_WIDTH (64)
#define TraceFpgaCollect_BIT_ALIGN_WIDTH (64)
#define TraceFpgaCollect_BYTE_WIDTH (8)
#define TraceFpgaCollect_PCVA_WIDTH 50
#define TraceFpgaCollect_INSTNUM_WIDTH 8

struct __attribute__((packed)) TraceFpgaCollectStruct {
  uint64_t pcVA:50;
  uint32_t instNum:8;

  void dump() {
    printf("pcVA: %lx, instNum: %d\n", pcVA, instNum);
  }
};

#endif // __TRACERTL_DUT_INFO_H__