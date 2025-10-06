#ifndef __G4TRACE_H_
#define __G4TRACE_H_

#include "config.h"
#include "memif.h"
#include <cstdint>
#include <limits>
#include <ostream>
#include <unordered_map>


struct G4ThreadIdentifier {
  reg_t satp;
  reg_t tp;
  bool operator==(const G4ThreadIdentifier&) const = default;
};
template<>
struct std::hash<G4ThreadIdentifier> {
  inline size_t operator()(const G4ThreadIdentifier& x) const {
    return hash<reg_t>{}(x.satp * 5 + x.tp);
  }
};

struct G4TracePerThreadState;

struct G4TraceGlobalState {
  bool enable = false;
  bool verbose = false;
  const char *dest = nullptr;
  int num_traces = 0; // number of harts that have started tracing
  uint64_t max_trace_instructions = std::numeric_limits<decltype(max_trace_instructions)>::max();
  std::string compression = "lzma-3";//"zstd-13";// "none";
  std::unordered_map<G4ThreadIdentifier,G4TracePerThreadState> threads; 
};

struct G4TracePerThreadState {
  G4TraceGlobalState *global = nullptr;
  int thread_id = -1; // initialized when the trace file is opened.
  bool log_active = false; // START_TRACING hint seen, TODO: add option --log-use-roi-markers to initialize
  bool has_started = false; // The first instruction address has been printed (to avoid doing it twice if the START_TRACING hint has appeared already)
  std::ostream *out = nullptr;
  reg_t lastpc = 0;
  uint64_t instructions_traced = 0;
  int sync_marker_level = 0; // currently expected to be 0 or 1
  ~G4TracePerThreadState() {
    if (out) {
      delete out;
      out = nullptr;
    }
  }
};

struct G4TraceRegId {
  int id;
  bool operator==(const G4TraceRegId& X) const = default;
};

enum class G4InstType {
  INVALID,
  UNKNOWN,
  GENERIC,
  L,
  S,
  RMW,
  LR, SC,
  B, C, c, J, j, r,
  A, M, D, Q,
  START_TRACING,
  CLEAR,
  END_ROI,
  ACQ, REL,
  BAR,
  CV_SIGNAL, CV_BCAST,
  CV_WAIT,
  END_SM // not really a valid gems4proc instruction
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
  G4TraceRegId S_data_reg = g4trace_regid_invalid;  // for types S, SA, RMW (not neccesary for loads)
  int S_data_reg_nf = 1; // number of registers written/read
  G4VectorMemAccessType memory_access_type = G4VectorMemAccessType::INVALID; // for memory accesses (L, LA, LE, S, SA, RMW)
  reg_t target_address = g4trace_invalid_target_address; // for B, C, c, J, j, r
  reg_t cond_address = g4trace_invalid_target_address; // for CV_WAIT, CV_SIGNAL, CV_SIGNALCV_BCAST
  reg_t lock_address = g4trace_invalid_target_address; // for ACQ, REL, BAR (barrier address instead of lock), CV_WAIT
};

class processor_t;
typedef G4InstInfo (*G4TraceDecoder)(processor_t *p, reg_t pc, insn_t insn);

G4TraceDecoder g4trace_get_decoder(const std::string& instr_name);
void g4trace_trace_inst(processor_t *p, reg_t pc, insn_t insn, G4TraceDecoder decoder);
void g4trace_open_trace_file(G4TracePerThreadState& s);
void g4trace_close_trace_file(G4TracePerThreadState& s);
void g4trace_close_and_write_index(G4TraceGlobalState *global);
bool g4trace_parse_compression_config(const std::string& opts, std::string& method, int& preset);
G4TracePerThreadState& g4trace_get_thread_state(processor_t *p);

// From g4tracer-interface.h
enum G4TraceAnnotationId {
    G4_TRACE_ANNOTATION_ID_START_TRACING = 0x101,
    G4_TRACE_ANNOTATION_ID_START_REGION_OF_INTEREST = 0x102,
    G4_TRACE_ANNOTATION_ID_END_REGION_OF_INTEREST = 0x103,

    // Synchronization markers
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_LOCK_ACQUIRE = 0x110,
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_LOCK_RELEASE = 0x111,
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_BARRIER = 0x112,
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_CONDITION_SIGNAL = 0x113,
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_CONDITION_BROADCAST = 0x114,
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_CONDITION_WAIT = 0x115,

    G4_TRACE_ANNOTATION_ID_END_SM = 0x1FF,
};

#endif
