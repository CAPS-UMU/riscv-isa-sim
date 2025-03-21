#ifndef __G4TRACE_H_
#define __G4TRACE_H_

#include "config.h"
#include "memif.h"

struct G4TraceRegId {
  int id;
  bool operator==(const G4TraceRegId& X) const = default;
};

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

const reg_t g4trace_invalid_target_address = -1;
const G4TraceRegId g4trace_regid_invalid = { -1 };

struct G4InstInfo {
  G4InstType type = G4InstType::INVALID;
  G4TraceRegId S_base_reg = g4trace_regid_invalid;  // for types S, SA, RMW (not neccesary for loads) 
  G4TraceRegId S_data_reg = g4trace_regid_invalid;  // for types S, SA, RMW (not neccesary for loads) TODO: remove this
  G4VectorMemAccessType memory_access_type = G4VectorMemAccessType::INVALID; // for memory accesses (L, LA, LE, S, SA, RMW)
  reg_t target_address = g4trace_invalid_target_address; // for B, C, c, J, j, r
};

class processor_t;
typedef G4InstInfo (*G4TraceDecoder)(processor_t *p, reg_t pc, insn_t insn);

G4TraceDecoder g4trace_get_decoder(const std::string& instr_name);
void g4trace_trace_inst(processor_t *p, reg_t pc, insn_t insn, G4TraceDecoder decoder);

#endif
