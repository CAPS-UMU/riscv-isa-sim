#include "g4trace.h"
#include "processor.h"
#include "disasm.h"

#include <filesystem>

using namespace std;

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

template <typename T, typename... Args>
bool eq_any(const T& first, const Args&... rest) {
  return ((first == rest) || ...);
}

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

static G4TraceDecoder g4trace_get_decoder_internal(const string& instr_name) { // g4trace_get_decoder does some (optional) logging before this.
#define DECODER_ARGS processor_t *p, reg_t pc, insn_t insn
  if (instr_name == "srai") {
    return [](DECODER_ARGS) { 
      if (insn.bits() == 0x40205013 /* srai zero, zero, 2 */) {
        return G4InstInfo { G4InstType::START_TRACING }; ;
      } else if (insn.bits() == 0x40005013 /* srai zero, zero, 0 */) {
        return G4InstInfo { G4InstType::CLEAR };   // ROI start
        } else if (insn.bits() == 0x40105013 /* srai zero, zero, 1 */) {
        return G4InstInfo { G4InstType::END_ROI };   // ROI end
      } else {
        return G4InstInfo { G4InstType::GENERIC }; 
      }
    };
  } else if (eq_any(instr_name,
                    "add", "addi", "addiw", "addw", "and", "andi", "auipc", "lui", "or", "ori", "sll", "slli",
                    "slliw", "sllw", "slt", "slti", "sltiu", "sltu", "sra", "sraiw", "sraw", "srl",
                    "srli", "srliw", "srlw", "sub", "subw", "xor", "xori",
                    "c_add", "c_addi", "c_addi4spn", "c_addw", "c_and", "c_andi",
                    "c_li", "c_lui", "c_mv", "c_or", "c_slli", "c_srai", "c_srli", "c_sub", "c_subw", "c_xor")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::GENERIC }; };
  } else if (eq_any(instr_name, "beq", "bge", "bgeu", "blt", "bltu", "bne")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::B };
      ret.target_address = pc + insn.sb_imm();
      return ret;
    };
  } else if (eq_any(instr_name, "c_beqz", "c_bnez")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::B };
      ret.target_address = pc + insn.rvc_b_imm();
      return ret;
    };
  } else if (eq_any(instr_name, "c_j")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::J };
      ret.target_address = pc + insn.rvc_j_imm();
      return ret;
    };
  } else if (eq_any(instr_name, "c_jr")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret;
      if ((insn.rvc_rs1() == 1 || insn.rvc_rs1() == 5) && insn.rvc_imm() == 0) {
        ret.type = G4InstType::r;
      } else {
        ret.type = G4InstType::j;
      }
      ret.target_address = p->get_state()->XPR[insn.rvc_rs1()] & ~reg_t(1);
      return ret;
    };
/* } else if (eq_any(instr_name, "c_jalr")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret;
      ret.type = G4InstType::c; // (or r?)
      ret.target_address = p->get_state()->XPR[insn.rvc_rs1()] & ~reg_t(1);
      return ret;
    };
    */
  } else if (eq_any(instr_name, "c_jal")) { // this is actually c.addiw (difference between RV32 an RV64 and spike weirdness)
    return [](DECODER_ARGS) {
      assert(insn.rvc_rd() != 0);
      return G4InstInfo { G4InstType::GENERIC }; };
  } else if (eq_any(instr_name, "jal")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret;
      if (insn.rd() == 0) {
        ret.type = G4InstType::J; // J pseudoinstruction
      } else {
        assert((insn.rd() == 1 || insn.rd() == 5) || "JAL with unexpected destination register. Probably should be treated as J.");
        ret.type = G4InstType::C;
      }
      ret.target_address = pc + insn.uj_imm();
      return ret;
    };
  } else if (eq_any(instr_name, "jalr")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret;
      bool rdislink = insn.rd() == 1 || insn.rd() == 5;
      bool rs1islink = insn.rs1() == 1 || insn.rs1() == 5;
      if (!rdislink && rs1islink) {
        ret.type = G4InstType::r; // ret
      } else {
        ret.type = G4InstType::c;
      }
      ret.target_address = (p->get_state()->XPR[insn.rs1()] + insn.i_imm()) & ~reg_t(1);
      return ret;
    };
  } else if (eq_any(instr_name, "c_jalr")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret;
      ret.type = G4InstType::c;
      ret.target_address = p->get_state()->XPR[insn.rvc_rs1()] & ~reg_t(1);
      return ret;
    };
  } else if (eq_any(instr_name,
                    "lb", "lbu", "ld", "lh", "lhu", "lw", "lwu",
                    "fld", "flw", "flq", "flw",
                    "vle8_v", "vle16_v", "vle32_v", "vle64_v", "vle8ff_v", "vle16ff_v", "vle32ff_v", "vle64ff_v"
                    "vluxei8_v", "vluxei16_v", "vluxei32_v", "vluxei64_v")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::L };
      ret.memory_access_type = g4trace_decode_mem_access_type(insn);
      return ret;
    };
  } else if (eq_any(instr_name,
                    "c_fld", "c_ld", "c_lw", "c_lbu", "c_lb", "c_lhu", "c_lh", "c_fld", "c_ldsp", "c_lwsp", "c_fldsp")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::L };
      ret.memory_access_type = G4VectorMemAccessType::SCALAR;
      return ret;
    };
  } else if (eq_any(instr_name, "c_sd", "c_sw", "c_fsd")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::S };
      auto first3bits = (insn.bits() & 0xe000) >> 13;
      ret.S_base_reg = g4trace_regid_x(insn.rvc_rs1s());
      ret.S_data_reg =
        first3bits == 0x5 ? g4trace_regid_f(insn.rvc_rs2s()) // FSD
        : first3bits == 0x6 ? g4trace_regid_x(insn.rvc_rs2s()) // SW
        : first3bits == 0x7 ? g4trace_regid_x(insn.rvc_rs2s()) // SD
        : g4trace_regid_invalid;
      ret.memory_access_type = G4VectorMemAccessType::SCALAR;
      return ret;
    };
  } else if (eq_any(instr_name, "c_sdsp", "c_swsp", "c_fsdsp")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::S };
      auto first3bits = (insn.bits() & 0xe000) >> 13;
      ret.S_base_reg = g4trace_regid_x(2); // sp, x2
      ret.S_data_reg =
        first3bits == 0x5 ? g4trace_regid_f(insn.rvc_rs2()) // s_fsdsp
        : first3bits == 0x6 ? g4trace_regid_x(insn.rvc_rs2()) // c_swsp
        : first3bits == 0x7 ? g4trace_regid_x(insn.rvc_rs2()) // c_sdsp
        : g4trace_regid_invalid;
      ret.memory_access_type = G4VectorMemAccessType::SCALAR;
      return ret;
    };
  } else if (eq_any(instr_name,
                    "sb", "sd", "sh", "sw",
                    "vse8_v", "vse16_v", "vse32_v", "vse64_v",
                    "vsuxei8_v", "vsuxei16_v", "vsuxei32_v", "vsuxei64_v")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::S };
      ret.S_base_reg = g4trace_regid_x(insn.rs1());
      ret.memory_access_type = g4trace_decode_mem_access_type(insn);
      if (ret.memory_access_type == G4VectorMemAccessType::SCALAR) {
        ret.S_data_reg = g4trace_regid_x(insn.rs2());
      } else {
        ret.S_data_reg = g4trace_regid_v(insn.rd());
      }
      return ret;
    };
  } else if (eq_any(instr_name,
                    "fsd", "fsh", "fsq", "fsw")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::S };
      ret.S_base_reg = g4trace_regid_x(insn.rs1());
      ret.memory_access_type = g4trace_decode_mem_access_type(insn);
      if (ret.memory_access_type == G4VectorMemAccessType::SCALAR) {
        ret.S_data_reg = g4trace_regid_f(insn.rs2());
      } else {
        ret.S_data_reg = g4trace_regid_v(insn.rd());  // Actually covered in the integer store case
      }
      return ret;
    };
  } else if (eq_any(instr_name, "lr_d", "lr_w")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::LA };
      ret.memory_access_type = G4VectorMemAccessType::SCALAR;
      return ret;
    };
  } else if (eq_any(instr_name, "sc_d", "sc_w")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::SA };
      ret.memory_access_type = G4VectorMemAccessType::SCALAR;
      ret.S_base_reg = g4trace_regid_x(insn.rs1());
      ret.S_data_reg= g4trace_regid_x(insn.rs2());
      return ret;
    };
  } else if (eq_any(instr_name,
                    "amoadd_d", "amoadd_w", "amoand_d", "amoand_w", "amomax_d", "amomaxu_d", "amomaxu_w", "amomax_w", "amomin_d",
                    "amominu_d", "amominu_w", "amomin_w", "amoor_d", "amoor_w", "amoswap_d", "amoswap_w", "amoxor_d", "amoxor_w")) {
    return [](DECODER_ARGS) {
      G4InstInfo ret { G4InstType::RMW };
      ret.memory_access_type = G4VectorMemAccessType::SCALAR;
      ret.S_base_reg = g4trace_regid_x(insn.rs1());
      ret.S_data_reg= g4trace_regid_x(insn.rs2());
      return ret;
    };

  } else if (eq_any(instr_name,
                    "fence", "fence_i")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::GENERIC }; };
  } else if (eq_any(instr_name,
                    "fmadd_d", "fmadd_h", "fmadd_q", "fmadd_s", "fmsub_d", "fmsub_h", "fmsub_q", "fmsub_s",
                    "fnmadd_d", "fnmadd_h", "fnmadd_q", "fnmadd_s", "fnmsub_d", "fnmsub_h", "fnmsub_q", "fnmsub_s")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::M }; };
  } else if (eq_any(instr_name,
                    "fmul_d", "fmul_h", "fmul_q", "fmul_s",
                    "vfmul_vf", "vfmul_vv", "vfwmul_vf", "vfwmul_vv")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::M }; };
  } else if (eq_any(instr_name,
                    "clmulh", "clmul", "clmulr", "c_mul", "mulh", "mulhsu", "mulhu", "mul", "mulw",
                    "vclmulh_vv", "vclmulh_vx", "vclmul_vv", "vclmul_vx", "vmulhsu_vv", "vmulhsu_vx",
                    "vmulhu_vv", "vmulhu_vx", "vmulh_vv", "vmulh_vx", "vmul_vv", "vmul_vx", "vsmul_vv",
                    "vsmul_vx", "vwmulsu_vv", "vwmulsu_vx", "vwmulu_vv", "vwmulu_vx", "vwmul_vv",
                    "vwmul_vx")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::GENERIC }; };
  } else if (eq_any(instr_name,
                    "fdiv_s", "fdiv_d", "fdiv_q", "fdiv_h",
                    "vfdiv_vf", "vfdiv_vv", "vfrdiv_vf")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::D }; };
  } else if (eq_any(instr_name,
                    "div", "divu", "divuw", "divw", 
                    "rem", "remu", "remuw", "remw", 
                    "vdiv_vv", "vdiv_vx", "vdivu_vv", "vdivu_vx"
                    "vrem_vv", "vrem_vx", "vremu_vv", "vremu_vx")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::GENERIC }; };
  } else if (eq_any(instr_name,
                    "fadd_d", "fadd_h", "fadd_q", "fadd_s",
                    "vfadd_vf", "vfadd_vv",
                    "vfredosum_vs", "vfredusum_vs")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::A }; };
  } else if (eq_any(instr_name,
                    "fsqrt_s", "fsqrt_d", "vfrsqrt7_v", "vfsqrt_v", "fsqrt_q", "fsqrt_h")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::Q }; };
  } else if (eq_any(instr_name,
                    "fmv_w_x", "fmv_x_w", 
                    "fcvt_l_h", "fcvt_lu_h", "fcvt_d_h", "fcvt_h_d", "fcvt_h_l", "fcvt_h_lu", "fcvt_h_q",
                    "fcvt_h_s", "fcvt_h_w", "fcvt_h_wu", "fcvt_q_h", "fcvt_s_h", "fcvt_w_h", "fcvt_wu_h",
                    "fcvt_l_s", "fcvt_lu_s", "fcvt_s_l", "fcvt_s_lu", "fcvt_s_w", "fcvt_s_wu", "fcvt_w_s",
                    "fcvt_wu_s", "fcvt_d_l", "fcvt_d_lu", "fcvt_d_q", "fcvt_d_s", "fcvt_d_w", "fcvt_d_wu",
                    "fcvt_l_d", "fcvt_lu_d", "fcvt_s_d", "fcvt_w_d", "fcvt_wu_d")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::GENERIC }; };    
  } else if (eq_any(instr_name, "vsetivli", "vsetvli", "vsetvl")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::GENERIC }; };
  } else if (eq_any(instr_name, 
                    "vfmv_f_s", "vfmv_s_f", "vfmv_v_f", "vfncvt_f_f_w", "vfncvt_f_x_w", "vfncvt_f_xu_w", "vfncvt_rod_f_f_w",
                    "vfncvt_rtz_x_f_w", "vfncvt_rtz_xu_f_w", "vfncvt_x_f_w", "vfncvt_xu_f_w",
                    "vfcvt_f_x_v", "vfcvt_f_xu_v", "vfcvt_rtz_x_f_v", "vfcvt_rtz_xu_f_v", "vfcvt_x_f_v", "vfcvt_xu_f_v", 
                    "vfwcvt_f_f_v", "vfwcvt_f_x_v", "vfwcvt_f_xu_v", "vfwcvt_rtz_x_f_v", "vfwcvt_rtz_xu_f_v", "vfwcvt_x_f_v", "vfwcvt_xu_f_v", 
                    "vmv1r_v", "vmv2r_v", "vmv4r_v", "vmv8r_v", "vmv_s_x", "vmv_v_i", "vmv_v_v", "vmv_v_x", "vmv_x_s",
                    "vid_v", "viota_m",
                    "vor_vi", "vor_vv", "vor_vx", "vandn_vv", "vandn_vx", "vand_vi", "vand_vv", "vand_vx",                    
                    "vredand_vs", "vredmax_vs", "vredmaxu_vs", "vredmin_vs", "vredminu_vs", "vredor_vs", "vredsum_vs", "vredxor_vs", 
                    "vadd_vi", "vadd_vv", "vadd_vx", "vsub_vv", "vsub_vx", 
                    "vwadd_vv", "vwadd_vx", "vwadd_wv", "vwadd_wx", "vwaddu_vv", "vwaddu_vx", "vwaddu_wv", "vwaddu_wx", "vwmacc_vv",
                    "vwmacc_vx", "vwmaccsu_vv", "vwmaccsu_vx", "vwmaccu_vv", "vwmaccu_vx", "vwmaccus_vx", "vasub_vv", "vasubu_vv",
                    "vasub_vx", "vasubu_vx",
                    "vsll_vi", "vsll_vv", "vsll_vx", "vsra_vi", "vsra_vv", "vsra_vx", "vsrl_vi", "vsrl_vv", "vsrl_vx", "vssra_vi",
                    "vssra_vv", "vssra_vx", "vssrl_vi", "vssrl_vv", "vssrl_vx", "vssub_vv", "vssub_vx", "vssubu_vv", "vssubu_vx", 
                    "vsext_vf2", "vsext_vf4", "vsext_vf8",
                    "vslide1down_vx", "vslide1up_vx", "vslidedown_vi", "vslidedown_vx", "vslideup_vi", "vslideup_vx", 
                    "vsadd_vi", "vsadd_vv", "vsadd_vx", "vsaddu_vi", "vsaddu_vv", "vsaddu_vx", "vsbc_vvm", "vsbc_vxm", 
                    "vmacc_vv", "vmacc_vx", "vmadc_vv", "vmadc_vx", "vmadc_vi", "vmadc_vim", "vmadc_vvm",
                    "vmadc_vxm", "vmadd_vv", "vmadd_vx",
                    "vmand_mm", "vmandn_mm", "vmax_vv", "vmax_vx", "vmaxu_vv", "vmaxu_vx",
                    "vmin_vv", "vmin_vx", "vminu_vv", "vminu_vx", "vmnand_mm", "vmnor_mm", "vmor_mm", "vmorn_mm",
                    "vmsbc_vv", "vmsbc_vx", "vmsbc_vvm", "vmsbc_vxm", "vmsbf_m", "vmseq_vi", "vmseq_vv", "vmseq_vx",
                    "vmsgt_vi", "vmsgt_vx", "vmsgtu_vi", "vmsgtu_vx", "vmsif_m", "vmsle_vi", "vmsle_vv", "vmsle_vx",
                    "vmsleu_vi", "vmsleu_vv", "vmsleu_vx", "vmslt_vv", "vmslt_vx", "vmsltu_vv", "vmsltu_vx", "vmsne_vi",
                    "vmsne_vv", "vmsne_vx", "vmsof_m",
                    "vmerge_vim", "vmerge_vvm", "vmerge_vxm", "vfirst_m")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::GENERIC }; };
  } else if (eq_any(instr_name,
                    "vfmacc_vf", "vfmacc_vv", "vfmadd_vf", "vfmadd_vv", "vfnmacc_vf", "vfnmacc_vv",
                    "vfnmadd_vf", "vfnmadd_vv", "vfnmsac_vf", "vfnmsac_vv", "vfnmsub_vf", "vfnmsub_vv")) {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::M }; };
  } else {
    return [](DECODER_ARGS) { return G4InstInfo { G4InstType::UNKNOWN }; };        
  }
#undef DECODER_ARGS
}

G4TraceDecoder g4trace_get_decoder(const string& instr_name) {
  auto ret = g4trace_get_decoder_internal(instr_name);
  printf("G4TraceDecoder:\t%s\t%p\n", instr_name.c_str(), ret);
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
  } else if (g4i.memory_access_type ==  G4VectorMemAccessType::INDEXED) {
    fprintf(log_file, "s%de%lu ", size, num_items);
    for (auto i = accesses.cbegin(); i != accesses.cend(); i++) {
      if (i != accesses.cbegin()) {
        fprintf(log_file, ",");
      }
      fprintf(log_file, "%lx", get<0>(*i));
    }
  } else {
    fprintf(log_file, " TODO access_tcype=%d ", (int) g4i.memory_access_type);
    for (auto item : accesses) {
      auto addr = get<0>(item);
      auto size = get<2>(item);
      fprintf(log_file, " %lx %d", addr, size);
    }
  }
}

void g4trace_trace_inst(processor_t *p, reg_t pc, insn_t insn, G4TraceDecoder decoder) {
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
    fprintf(log_file, "{ %-32s } ", p->get_disassembler()->disassemble(insn).c_str());
    fflush(log_file); // TODO remove this, now here to ensure output is complete in case of assert.
  }

  G4InstInfo g4i = decoder(p, pc, insn);

  bool ignore_csrs = true;
  bool ignore_vstatus = true;

  auto diffpc = pc - p->get_state()->g4trace_lastpc;

  if (g4i.type == G4InstType::GENERIC) {
    fprintf(log_file, "%ld", diffpc);
  } else if (g4i.type == G4InstType::A) {
    fprintf(log_file, "A%ld", diffpc);
  } else if (g4i.type == G4InstType::M) {
    fprintf(log_file, "M%ld", diffpc);
  } else if (g4i.type == G4InstType::D) {
    fprintf(log_file, "D%ld", diffpc);
  } else if (g4i.type == G4InstType::Q) {
    fprintf(log_file, "Q%ld", diffpc);
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
  } else if (g4i.type == G4InstType::RMW) {
    fprintf(log_file, "RMW%ld", diffpc);
    assert(g4i.S_base_reg != g4trace_regid_invalid);
    assert(g4i.S_data_reg != g4trace_regid_invalid);
    assert(g4i.memory_access_type != G4VectorMemAccessType::INVALID);
    assert(loads.size() == stores.size()); // TODO: check that this is necessarily true (maybe the stores don't always happen?);
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
    if (!p->get_log_g4trace_has_started()) {
      p->get_state()->g4trace_lastpc = pc + 4; // Address of next instruction, which will be the first in the trace
      fprintf(log_file, "%lx\n", p->get_state()->g4trace_lastpc);
      p->set_log_g4trace_has_started();
      return; // don't print operands
    } else {
      // trace has already started, we have found the marker twice (maybe two threads are runningin the same hart)
      // don't print anything, don't update lastpc
      return;
    }
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
  assert(loads.empty() || (g4i.type == G4InstType::L || g4i.type == G4InstType::LA || g4i.type == G4InstType::RMW));
  assert(stores.empty() || (g4i.type == G4InstType::S || g4i.type == G4InstType::SA || g4i.type == G4InstType::RMW));

  p->get_state()->g4trace_lastpc = pc;

  // print x and y register operands for S (and not RMW), and x operands for anything else
  if (g4i.type == G4InstType::S/* || g4i.type == G4InstType::RMW*/) {
    assert(g4i.S_base_reg != g4trace_regid_invalid);
    assert(g4i.S_data_reg != g4trace_regid_invalid);
    assert(count_if(read_regs.begin(), read_regs.end(), [&](auto x){ return g4trace_regid_from_commit_log_reg_id(x.first) == g4i.S_base_reg; }) == 1);
    assert(count_if(read_regs.begin(), read_regs.end(), [&](auto x){ return g4trace_regid_from_commit_log_reg_id(x.first) == g4i.S_data_reg; }) == 1);

    if (count_if(read_regs.begin(), read_regs.end(), [&](auto x){ return g4trace_regid_from_commit_log_reg_id(x.first) != g4i.S_base_reg; }) == 0) {
      // only the base_reg has been read, so the data register must be the same
      assert(g4i.S_base_reg == g4i.S_data_reg); // is this true in all cases?
      assert(read_regs.size() == 1);
    } else {
      //print the base register as x, the rest as y (must be data)
      fprintf(log_file, "x%d", g4i.S_base_reg.id);
      for (auto item : read_regs) {
        auto g4rid = g4trace_regid_from_commit_log_reg_id(item.first);
        if (g4rid != g4i.S_base_reg
            && (!commit_log_reg_id_is_csr(item.first) || !ignore_csrs)
            && (!commit_log_reg_id_is_vstatus(item.first) || !ignore_vstatus)) {
          fprintf(log_file, "y%d", g4rid.id);
        }
      }
    }
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

  if (!stores.empty() && g4i.type != G4InstType::RMW) { // don't print stores for RMWs, they sould be the same as loads
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


void g4trace_write_index(G4TraceConfig *global) {
  if (global && global->enable) {
    if (global->num_traces > 0) {
      auto index_filename = filesystem::path(global->dest) / "trace.index";
      ofstream index_file(index_filename, ios::out);
      index_file << global->num_traces << endl;
      index_file << "TRACE_HAS_SEQUENCE_NUMBERS: 0" << endl;
      index_file << "TRACE_HAS_SC_vs_RELAXED_LOCK_TYPE: 0" << endl;
      index_file.close();
    } else {
      cerr << "No gems4proc trace created. It seems no processor used the START_TRACING hint." << endl;
    }
  }
}

FILE *g4trace_open_trace_file(G4TraceConfig *global) {
  assert(global->enable);
  if (!filesystem::exists(global->dest)) {
    filesystem::create_directory(global->dest);
  }
  stringstream name;
  name << "trace-" << setw(4) << setfill('0') << global->num_traces << ".trc";
  filesystem::path p = filesystem::path(global->dest) / name.str();
  FILE* f = fopen(p.c_str(), "w"); // TODO: gzip / lzma
  ++global->num_traces;
  return f;
}

void g4trace_close_trace_file(G4TraceConfig *global, FILE *f)
{
  if (f) {
    fclose(f);
  }
}
