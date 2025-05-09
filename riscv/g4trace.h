#ifndef __G4TRACE_H_
#define __G4TRACE_H_

#include "config.h"
#include "memif.h"
#include <cstdint>
#include <limits>
#include <ostream>

struct G4TraceConfig {
  bool enable = false;
  bool verbose = false;
  const char *dest = nullptr;
  int num_traces = 0; // number of harts that have started tracing
  uint64_t max_trace_instructions = std::numeric_limits<decltype(max_trace_instructions)>::max();
  std::string compression = "lzma-3";//"zstd-13";// "none";
};

struct G4TracePerProcState {
  G4TraceConfig *global = nullptr;
  bool has_started = false; // The first instruction address ha been printed (to avoid doing it twice if the START_TRACING hint has appeared already)
  std::ostream *out = nullptr;
  reg_t lastpc = 0;
  bool setpc_done = false;
  reg_t last_setpc = 0;
  uint64_t instructions_traced = 0;
};

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
  LR, SC,
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
void g4trace_open_trace_file(G4TracePerProcState& s);
void g4trace_close_trace_file(G4TracePerProcState& s);
void g4trace_write_index(G4TraceConfig *global);
bool g4trace_parse_compression_config(const std::string& opts, std::string& method, int& preset);

#endif
