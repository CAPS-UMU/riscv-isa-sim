#ifndef __G4TRACE_H_
#define __G4TRACE_H_

#include "config.h"
#include "decode.h"
#include "processor.h"
#include "disasm.h"
#include "decode_macros.h"
#include <algorithm>
#include <cassert>

const reg_t g4trace_invalid_target_address = -1;

struct G4TraceRegId {
  int id;
  bool operator==(const G4TraceRegId& X) const = default;      // OK
};
const G4TraceRegId g4trace_regid_invalid = { -1 };
static G4TraceRegId g4trace_regid_x(reg_t reg_id) { assert((reg_id & 0x1f) == reg_id); return { int(reg_id) }; }
static G4TraceRegId g4trace_regid_f(reg_t reg_id) { assert((reg_id & 0x1f) == reg_id); return { int(reg_id) + 32 }; }
static G4TraceRegId g4trace_regid_v(reg_t reg_id) { assert((reg_id & 0x1f) == reg_id); return { int(reg_id) + 64 }; }
static G4TraceRegId g4trace_regid_vstatus() { return { 9999 }; }
static G4TraceRegId g4trace_regid_csr(reg_t reg_id) { return { int(reg_id) + 10000 }; }

static G4TraceRegId g4trace_regid_from_commit_log_reg_id(reg_t commit_log_reg_id) {
  int r = commit_log_reg_id >> 4;
  switch (commit_log_reg_id & 0xf) {
  case 0:  // IREG
    return g4trace_regid_x(r);
  case 1: // FREG
    return g4trace_regid_f(r);
  case 2: // VREG
    return g4trace_regid_v(r);
  case 3: // VSTATUS
    return g4trace_regid_vstatus();
  case 4: // CSR
    return g4trace_regid_csr(r);
  default:
    assert(false);
  }
}
static bool commit_log_reg_id_is_vstatus(reg_t commit_log_reg_id) {
  return (commit_log_reg_id & 0xf) == 3;
}
static bool commit_log_reg_id_is_csr(reg_t commit_log_reg_id) {
  return (commit_log_reg_id & 0xf) == 4;
}

enum class G4InstType {
  INVALID,
  UNKNOWN,
  GENERIC,
  L, LA, LE,
  S, SA, RMW,
  B, C, c, J, j, r,
  A, M, D, Q,
  START_TRACING,
  CLEAR,
  END_ROI,
  ACQ, REL,
  BAR,
  CV_SIGNAL, CV_SIGNALCV_BCAST,
  CV_WAIT,
};

enum class G4VectorMemAccessType {
  INVALID,
  SCALAR,
  CONTIGUOUS,
  STRIDED,
  INDEXED,
};

# pragma GCC diagnostic ignored "-Wsign-compare"
template <typename T, typename... Args>
bool eq_any(const T& first, const Args&... rest) {
  return ((first == rest) || ...);
}
# pragma GCC diagnostic pop

struct G4InstInfo {
  G4InstType type = G4InstType::INVALID;
  G4TraceRegId S_base_reg = g4trace_regid_invalid;  // for types S, SA, RMW (not neccesary for loads) TODO: remove this or S_data_reg
  G4TraceRegId S_data_reg = g4trace_regid_invalid;  // for types S, SA, RMW (not neccesary for loads)
  G4VectorMemAccessType memory_access_type = G4VectorMemAccessType::INVALID; // for memory accesses (L, LA, LE, S, SA, RMW)
  reg_t target_address = g4trace_invalid_target_address; // for B, C, c, J, j, r
};

static G4VectorMemAccessType g4trace_decode_mem_access_type(insn_t insn) {
  auto last2bits = insn.bits() & 0x3;
  auto next5bits = (insn.bits() & 0x7f) >> 2;
  auto w = insn.v_width();
  assert(last2bits == 0x3);
  assert(next5bits == 0x00 || next5bits == 0x01 || next5bits == 0x08 || next5bits == 0x09);
  bool is_vector = (next5bits == 0x01 || next5bits == 0x09) && (w == 0 || w > 5);
  if (!is_vector) {
    return  G4VectorMemAccessType::SCALAR;
  } else {
    if (insn.v_mop() == 0) {
      return G4VectorMemAccessType::CONTIGUOUS;
    } else if (insn.v_mop() == 1 || insn.v_mop() == 3) {
      return G4VectorMemAccessType::INDEXED;
    } else {
      assert(insn.v_mop() == 2);
      return G4VectorMemAccessType::STRIDED;
    }
  }
}

static G4InstInfo g4trace_decode(processor_t *p, reg_t pc, insn_t insn) {
  G4InstInfo ret;
  auto last2bits = insn.bits() & 0x3;
  auto first3bits = (insn.bits() & 0xe000) >> 13;
  if (last2bits == 0x0) {
    if (eq_any(first3bits, 0x1, 0x2, 0x3)) { // FLD LQ || LW || FLW LD
      ret.type = G4InstType::L;
      ret.memory_access_type = G4VectorMemAccessType::SCALAR;
    } else if (eq_any(first3bits, 0x5, 0x6, 0x7)) { // FSD LQ || SW ||  SD
        ret.type = G4InstType::S;
        ret.S_base_reg = g4trace_regid_x(insn.rvc_rs1s());
        ret.S_data_reg =
            first3bits == 0x5 ? g4trace_regid_f(insn.rvc_rs2s()) // FSD
            : first3bits == 0x6 ? g4trace_regid_x(insn.rvc_rs2s()) // SW
            : first3bits == 0x7 ? g4trace_regid_x(insn.rvc_rs2s()) // SD
            : g4trace_regid_invalid;
        ret.memory_access_type = G4VectorMemAccessType::SCALAR;
      } else {
      ret.type = G4InstType::UNKNOWN;
    }
  } else if (last2bits == 0x1) {
    if (eq_any(first3bits, 0x0, 0x1, 0x2, 0x3, 0x4)) { // C.ADDI || C.ADDIW || C.LI || C.LUI, C.ADDI16SP || MISC-ALU // (RV64, RV32 would have 0x2 as C.JAL)
      ret.type = G4InstType::GENERIC;
    } else if (first3bits == 0x5) { // C.J
      ret.type = G4InstType::J;
      ret.target_address = pc + insn.rvc_j_imm();
    } else if (eq_any(first3bits, 0x6, 0x7)) { // C.BEQZ || C.BNEZ
      ret.type = G4InstType::B;
      ret.target_address = pc + insn.rvc_b_imm();
    } else {
      ret.type = G4InstType::UNKNOWN;
    }
  } else if (last2bits == 0x2) {
    if (first3bits == 0x0) { // C.SLLI
      ret.type = G4InstType::GENERIC;
    } else if (first3bits == 0x4) {
      if (insn.rvc_rs1() != 0 && insn.rvc_rs2() != 0) { // C.MV C.ADD
        ret.type = G4InstType::GENERIC;
      } else if (insn.rvc_rs1() != 0 && insn.rvc_rs2() == 0) { // C.JR || C.JALR
        if (((insn.bits() >> 12) & 0x1) == 0) { // C.JR
          if ((insn.rvc_rs1() == 1 || insn.rvc_rs1() == 5) && insn.rvc_imm() == 0) {
            ret.type = G4InstType::r;
          } else {
            ret.type = G4InstType::j;
          }
        } else { // C.JALR
          ret.type = G4InstType::c; // (or r?)
          assert(false && "not tested yet");
        }
        ret.target_address = p->get_state()->XPR[insn.rvc_rs1()] & ~reg_t(1);
      } else if (insn.rvc_rs1() == 0 && insn.rvc_rs2() == 0) { // C.EBREAK
        ret.type = G4InstType::UNKNOWN; // TODO
        assert(false && "not tested yet");
      } else {
        assert(false);
      }
    } else if (eq_any(first3bits, 0x5, 0x6, 0x7)) { // C.FSDSP, C.SWSP, C.SDSP
      ret.type = G4InstType::S;
      ret.S_base_reg = g4trace_regid_x(2); // sp, x2
      ret.S_data_reg =
          first3bits == 0x5 ? g4trace_regid_f(insn.rvc_rs2()) // C.FSDSP
        : first3bits == 0x6 ? g4trace_regid_x(insn.rvc_rs2())  // C.SWSP
        : first3bits == 0x7 ? g4trace_regid_x(insn.rvc_rs2())  // C.SDSP, assume RV64 (RV32 would be FSWSP)
        : g4trace_regid_invalid;
      ret.memory_access_type = G4VectorMemAccessType::SCALAR;
    } else {
      ret.type = G4InstType::UNKNOWN;
    }
  } else {
    assert(last2bits == 0x3);
    auto next5bits = (insn.bits() & 0x7f) >> 2;
    if (eq_any(next5bits,
                   0x04,  // 00 100 OP-IMM
                   0x05,  // 00 101 AUIPC
                   0x06,  // 00 110 OP-IMM-32
                   0x0c,  // 01 100 OP
                   0x0d)) // 01 101 LUI
    {
      if (insn.bits() == 0x40205013 /* srai zero, zero, 2 */) {
        ret.type = G4InstType::START_TRACING;
      } else if (insn.bits() == 0x40005013 /* srai zero, zero, 0 */) {
        ret.type = G4InstType::CLEAR;  // ROI start
      } else if (insn.bits() == 0x40105013 /* srai zero, zero, 1 */) {
        ret.type = G4InstType::END_ROI;  // ROI end
      } else {
        ret.type = G4InstType::GENERIC;
      }
    } else if (next5bits == 0x15) { // 10 101 OP-V
      auto width = insn.v_width();
      auto first6bits = (insn.bits() & 0xfc000000) >> 26;
      if (width == 0x7) {
        ret.type = G4InstType::GENERIC; // VSETVL, VSETVLI, VSETIVLI
      } else if (eq_any(width, 0x1, 0x5)) { // xxx.vv, xxx.vf
        // TODO: A M D Q
        if (eq_any(first6bits, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f)) {
          ret.type = G4InstType::M; // vfmacc vfmadd vfnmacc vfnmadd vfnmsac vfnmsub vfmsac vfmsub
        } else if (eq_any(first6bits, 0x24, 0x38)) {
          ret.type = G4InstType::M; // vfmul vfwmul
        } else if (eq_any(first6bits, 0x17)) {
          ret.type = G4InstType::GENERIC; // vfmv
        } else {
          ret.type = G4InstType::UNKNOWN;
        }
      } else if (eq_any(width, 0x0, 0x2, 0x3, 0x4)) { // xxx.vi xxx.vx integer_xxx.vv xxx.mm
        ret.type = G4InstType::GENERIC;
      } else {
        ret.type = G4InstType::UNKNOWN;
      }
    } else if (eq_any(next5bits,
                      0x08,  // 00 000 STORE
                      0x09))  // 00 001 STORE-FP
    {
      ret.type = G4InstType::S;
      ret.S_base_reg = g4trace_regid_x(insn.rs1());
      ret.memory_access_type = g4trace_decode_mem_access_type(insn);
      auto w = insn.v_width();
      if (ret.memory_access_type == G4VectorMemAccessType::SCALAR) {
        ret.S_data_reg =
            next5bits == 0x8 ? g4trace_regid_x(insn.rs2()) // STORE
          : next5bits == 0x9 ? g4trace_regid_f(insn.rs2()) // STORE-FP
          : g4trace_regid_invalid ;
      } else {
        ret.S_data_reg = g4trace_regid_v(insn.rd());
      }
    } else if (eq_any(next5bits,
                      0x00,  // 00 000 LOAD
                      0x01))  // 00 001 LOAD-FP
    {
      ret.type = G4InstType::L;
      ret.memory_access_type = g4trace_decode_mem_access_type(insn);
    } else if (next5bits == 0x0b) { // AMO
      auto first5bits = (insn.bits() & 0xf8000000) >> 27;
      if (first5bits == 0x2) { // LR
        ret.type = G4InstType::LA;
        ret.memory_access_type = G4VectorMemAccessType::SCALAR;
      } else if (first5bits == 0x3) { // SC
        ret.type = G4InstType::SA;
        ret.memory_access_type = G4VectorMemAccessType::SCALAR;
        ret.S_base_reg = g4trace_regid_x(insn.rs1());
        ret.S_data_reg= g4trace_regid_x(insn.rs2());
      } else { // AMO...
        // TODO
        ret.type = G4InstType::UNKNOWN;
      }
    } else if (eq_any(next5bits, 0x10, 0x11, 0x12, 0x13)) {  // FMADD || FMSUB || FNMSUB || FNMADD
      ret.type = G4InstType::M;
    } else if (next5bits == 0x18) {  // B
      ret.type = G4InstType::B;
      ret.target_address = pc + insn.sb_imm();
    } else if (next5bits == 0x19) {  // JALR
      bool rdislink = insn.rd() == 1 || insn.rd() == 5;
      bool rs1islink = insn.rs1() == 1 || insn.rs1() == 5;
      if (!rdislink && rs1islink) {
        ret.type = G4InstType::r; // ret
      } else {
        ret.type = G4InstType::c;
      }
      ret.target_address = (p->get_state()->XPR[insn.rs1()] + insn.i_imm()) & ~reg_t(1);
    } else if (next5bits == 0x1b) {  // JAL
      if (insn.rd() == 0) {
        ret.type = G4InstType::J; // J pseudoinstruction
      } else {
        assert((insn.rd() == 1 || insn.rd() == 5) || "JAL with unexpected destination register. Probably should be treated as J.");
        ret.type = G4InstType::C;
      }
      ret.target_address = pc + insn.uj_imm();
    } else {
      ret.type = G4InstType::UNKNOWN;
    }
  }
  assert(ret.type != G4InstType::INVALID);
  return ret;
}

static void g4trace_print_memory_access_addresses(const commit_log_mem_t& accesses, const G4InstInfo& g4i, FILE* log_file) {
  const auto& first = *accesses.begin();
  auto addr_first = get<0>(first);
  auto size = get<2>(first);
  for (const auto& i : accesses) {
    assert(get<2>(i) == size); // all accesses have the same size
  }
  auto num_items = accesses.size();

  if (g4i.memory_access_type ==  G4VectorMemAccessType::SCALAR) {
    assert(num_items == 1);
    fprintf(log_file, " %lx %d", addr_first, size);
  } else if (g4i.memory_access_type ==  G4VectorMemAccessType::CONTIGUOUS) {
    fprintf(log_file, "s%de%lu %lx", size, num_items, addr_first); // assuming that the first address is the lowest
  } else {
    fprintf(log_file, " TODO access_type=%d ", (int) g4i.memory_access_type);
    for (auto item : accesses) {
      auto addr = get<0>(item);
      auto size = get<2>(item);
      fprintf(log_file, " %lx %d", addr, size);
      //auto value = get<1>(item);
      //commit_log_print_value(log_file, size * 8, value);
    }
  }
}
static void g4trace_trace_inst(processor_t *p, reg_t pc, insn_t insn) {
  if (!p->get_log_active()) return;

  FILE *log_file = p->get_g4trace_output_file();

  auto& read_regs = p->get_state()->log_reg_read;
  auto& written_regs = p->get_state()->log_reg_write;
  auto& loads = p->get_state()->log_mem_read;
  auto& stores = p->get_state()->log_mem_write;
  int priv = p->get_state()->last_inst_priv;
  int xlen = p->get_state()->last_inst_xlen;
  int flen = p->get_state()->last_inst_flen;

  if (priv && p->get_log_filter_privileged()) return;

  if (p->get_log_g4trace_verbose()) {
    fprintf(log_file, "{ %1lx-%2lx-%lx-%2lx-%lx ",
            (insn.bits() & 0x3),
            (insn.bits() & 0x7f) >> 2,
            (insn.bits() & 0xe000) >> 13,
            (insn.bits() & 0xfc000000) >> 26,
            insn.v_width());
    fprintf(log_file, " %-32s } ", p->get_disassembler()->disassemble(insn).c_str());
    fflush(log_file); // TODO remove this, now here to ensure output is complete in case of assert.
  }

  G4InstInfo g4i = g4trace_decode(p, pc, insn);

  bool ignore_csrs = true;
  bool ignore_vstatus = true;

  auto diffpc = pc - p->get_state()->g4trace_lastpc;

  if (g4i.type == G4InstType::GENERIC) {
    fprintf(log_file, "%ld", diffpc);
  } else if (g4i.type == G4InstType::M) {
    fprintf(log_file, "M%ld", diffpc);
  } else if (g4i.type == G4InstType::L) {
    fprintf(log_file, "L%ld", diffpc);
    assert(g4i.memory_access_type != G4VectorMemAccessType::INVALID);
  } else if (g4i.type == G4InstType::LA) {
    fprintf(log_file, "LA%ld", diffpc);
    assert(g4i.memory_access_type != G4VectorMemAccessType::INVALID);
  } else if (g4i.type == G4InstType::S) {
    fprintf(log_file, "S%ld", diffpc);
    assert(g4i.S_base_reg != g4trace_regid_invalid);
    assert(g4i.S_data_reg != g4trace_regid_invalid);
    assert(g4i.memory_access_type != G4VectorMemAccessType::INVALID);
  } else if (g4i.type == G4InstType::SA) {
    fprintf(log_file, "SA%ld", diffpc);
    assert(g4i.S_base_reg != g4trace_regid_invalid);
    assert(g4i.S_data_reg != g4trace_regid_invalid);
    assert(g4i.memory_access_type != G4VectorMemAccessType::INVALID);
  } else if (g4i.type == G4InstType::B) {
    fprintf(log_file, "B%ld", diffpc);
    assert(g4i.target_address != g4trace_invalid_target_address);
  } else if (g4i.type == G4InstType::C) {
    fprintf(log_file, "C%ld", diffpc);
    assert(g4i.target_address != g4trace_invalid_target_address);
  } else if (g4i.type == G4InstType::J) {
    fprintf(log_file, "J%ld", diffpc);
    assert(g4i.target_address != g4trace_invalid_target_address);
  } else if (g4i.type == G4InstType::j) {
    fprintf(log_file, "r%ld", diffpc);
    assert(g4i.target_address != g4trace_invalid_target_address);
  } else if (g4i.type == G4InstType::r) {
    fprintf(log_file, "r%ld", diffpc);
    assert(g4i.target_address != g4trace_invalid_target_address);
  } else if (g4i.type == G4InstType::c) {
    fprintf(log_file, "c%ld", diffpc);
    assert(g4i.target_address != g4trace_invalid_target_address);
  } else if (g4i.type == G4InstType::START_TRACING) {
    p->get_state()->g4trace_lastpc = pc + 4; // Address of next instruction, which will be the first in the trace
    fprintf(log_file, "%lx\n", p->get_state()->g4trace_lastpc);
    return; // don't print operands
  } else if (g4i.type == G4InstType::CLEAR) {
    fprintf(log_file, "CLEAR\n");
    return; // don't print operands, don't update lastpc
  } else if (g4i.type == G4InstType::END_ROI) {
    fprintf(log_file, "END %lx\n", p->get_state()->g4trace_lastpc);
    fflush(log_file);
    // TODO maybe fclose(log_file);
    return; // don't print operands, don't update lastpc
  } else {
    fprintf(log_file, "UNKNOWN%ld", diffpc);
    assert(g4i.type == G4InstType::UNKNOWN);
  }

  assert(p->get_log_g4trace_verbose() || g4i.type != G4InstType::UNKNOWN);
  assert(loads.empty() || (g4i.type == G4InstType::L || g4i.type == G4InstType::LA));
  assert(stores.empty() || (g4i.type == G4InstType::S || g4i.type == G4InstType::SA));

  p->get_state()->g4trace_lastpc = pc;

  // print x and y register operands for S, and x operands for anything else
  if (g4i.type == G4InstType::S) {
    assert(g4i.S_base_reg != g4trace_regid_invalid);
    assert(g4i.S_data_reg != g4trace_regid_invalid);
    //fprintf(log_file, "x%d", g4i.S_base_reg);
    assert(count_if(read_regs.begin(), read_regs.end(), [&](auto x){ return g4trace_regid_from_commit_log_reg_id(x.first) == g4i.S_base_reg; }));
    assert(count_if(read_regs.begin(), read_regs.end(), [&](auto x){ return g4trace_regid_from_commit_log_reg_id(x.first) == g4i.S_data_reg; }));
    bool printed_another_register = false;
    for (auto item : read_regs) {
      auto g4rid = g4trace_regid_from_commit_log_reg_id(item.first);
      if (g4rid != g4i.S_data_reg
          && (!commit_log_reg_id_is_csr(item.first) || !ignore_csrs)
          && (!commit_log_reg_id_is_vstatus(item.first) || !ignore_vstatus)) {
        printed_another_register = true;
        fprintf(log_file, "x%d", g4rid.id);
      }
    }
    if (!printed_another_register) { // the data register must be the same as the base register
      assert(g4i.S_data_reg == g4i.S_base_reg);
      fprintf(log_file, "x%d", g4i.S_base_reg.id);
    }
    fprintf(log_file, "y%d", g4i.S_data_reg.id);
  } else {
    for (auto item : read_regs) {
      auto g4rid = g4trace_regid_from_commit_log_reg_id(item.first);
      if ((!commit_log_reg_id_is_csr(item.first) || !ignore_csrs)
          && (!commit_log_reg_id_is_vstatus(item.first) || !ignore_vstatus)) {
        fprintf(log_file, "x%d", g4rid.id);
      }
    }
  }

  for (auto item : written_regs) {
    if (item.first == 0)
      continue;
    auto g4rid = g4trace_regid_from_commit_log_reg_id(item.first);
    if (((item.first & 0xf) != 4 || !ignore_csrs)
        && (item.first & 0xf) != 3) {
      fprintf(log_file, "z%d", g4rid.id);
    }
  }

  if (!loads.empty()) {
    g4trace_print_memory_access_addresses(loads, g4i, log_file);
  }

  if (!stores.empty()) {
    g4trace_print_memory_access_addresses(stores, g4i, log_file);
  }

  if (g4i.target_address != g4trace_invalid_target_address) {
    assert(g4i.type == G4InstType::B || g4i.type == G4InstType::C || g4i.type == G4InstType::c || g4i.type == G4InstType::J || g4i.type == G4InstType::j || g4i.type == G4InstType::r);
    fprintf(log_file, "t%ld", g4i.target_address - pc);
    if (g4i.type == G4InstType::B) {
      if (p->get_state()->g4trace_setpc_done) {
        fprintf(log_file, "*");
        assert(g4i.target_address == p->get_state()->g4trace_last_setpc);
      }
    } else {
      assert(p->get_state()->g4trace_setpc_done);
      assert(g4i.target_address == p->get_state()->g4trace_last_setpc);
    }
  }

  fprintf(log_file, "\n");
}

#endif
