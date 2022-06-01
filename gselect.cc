/*
 Rachel White
 CSE420
 Project 2
 gselect.cc
 */

#include "cpu/pred/gselect.hh"

#include "base/bitfield.hh"
#include "base/intmath.hh"
#include "debug/GSelect.hh"

GSelectBP::GSelectBP(const GSelectBPParams *params)
    : BPredUnit(params),
      globalPredictorSize(params->PredictorSize),
      globalCtrBits(params->PHTCtrBits),
      globalCtrs(globalPredictorSize, SatCounter(globalCtrBits)),
      globalHistory(params->numThreads, 0),
      globalHistoryBits(params->globalHistoryBits)
{
    if (!isPowerOf2(globalPredictorSize)) {
        fatal("Invalid global predictor size!\n");
    }

    assert((8 * sizeof(unsigned)) > globalCtrBits);
    assert((8 * sizeof(unsigned)) > globalHistoryBits);

    // total bits for index to predictor counters
    // index is n bits from history concatenated with m bits from PC
    // after PC has been right shifted this->instShiftAmt bits
    globalPredictorBits = ceilLog2(globalPredictorSize);
    
    globalPredictorMask = mask(globalPredictorBits);

    // bits from program counter in branch_addr
    globalPCBits = globalPredictorBits - globalHistoryBits;
    if (globalPCBits < 1) {
        fatal("Invalid program counter bits must be > 0!\n");
    }
        
    // mask for branch_addr after instShiftAmt shift
    PCMask = mask(globalPCBits);

    if (globalHistoryBits > globalPredictorBits) {
        fatal("Global history bits too large for global predictor index!\n");
    }
    
    // Set up the global history mask
    // this is equivalent to mask(log2(globalPredictorSize)
    globalHistoryMask = mask(globalHistoryBits);

    // Set up historyRegisterMask
    historyRegisterMask = mask(globalHistoryBits);
    
    DPRINTF(GSelect, "index mask: %#x\n", globalPredictorMask);
    DPRINTF(GSelect, "PC mask: %#x\n", PCMask);
    DPRINTF(GSelect, "history mask: %#x\n", globalHistoryMask);

    DPRINTF(GSelect, "predictor size: %i\n",
            globalPredictorSize);

    DPRINTF(GSelect, "PHT counter bits: %i\n", globalCtrBits);

    DPRINTF(GSelect, "instruction shift amount: %i\n",
            instShiftAmt);
}

inline
void
GSelectBP::updateGlobalHistTaken(ThreadID tid)
{
    globalHistory[tid] = (globalHistory[tid] << 1) | 1;
    globalHistory[tid] = globalHistory[tid] & historyRegisterMask;
}

inline
void
GSelectBP::updateGlobalHistNotTaken(ThreadID tid)
{
    globalHistory[tid] = (globalHistory[tid] << 1);
    globalHistory[tid] = globalHistory[tid] & historyRegisterMask;
}

void
GSelectBP::btbUpdate(ThreadID tid, Addr branch_addr, void * &bp_history)
{
    // Update Global History to Not Taken (clear LSB)
    globalHistory[tid] &= (historyRegisterMask & ~ULL(1));
}

bool
GSelectBP::lookup(ThreadID tid, Addr branch_addr, void * &bp_history)
{
    bool global_prediction;

    unsigned i = getPredictorIndex(globalHistory[tid], branch_addr);
    
    // Lookup in the global predictor to get its branch prediction
    unsigned counter_val = globalCtrs[i];
    global_prediction = getPrediction(counter_val);

    // Create BPHistory and pass it back to be recorded.
    BPHistory *history = new BPHistory;
    // remember original global history for this thread
    // prior to speculative update
    history->globalHistory = globalHistory[tid];
    // remember the branch prediction
    history->globalPredTaken = global_prediction;
    bp_history = (void *)history;

    // Speculative update of the global 
    // history for this thread.  Will be
    // corrected in update() or btbupdate()
    // if needed.
    if (global_prediction) {
        updateGlobalHistTaken(tid);
        return true;
    } else {
        updateGlobalHistNotTaken(tid);
        return false;
    }
}

void
GSelectBP::uncondBranch(ThreadID tid, Addr pc, void * &bp_history)
{
    // Create BPHistory and pass it back to be recorded.
    BPHistory *history = new BPHistory;
    history->globalHistory = globalHistory[tid];
    history->globalPredTaken = true;
    bp_history = static_cast<void *>(history);

    updateGlobalHistTaken(tid);
}

void
GSelectBP::update(ThreadID tid, Addr branch_addr, bool taken,
                  void *bp_history, bool squashed,
                  const StaticInstPtr & inst, Addr corrTarget)
{
    assert(bp_history);

    BPHistory *history = static_cast<BPHistory *>(bp_history);

    // If this is a misprediction, restore the speculatively
    // updated state (global history register)
    // and update again.
    if (squashed) {
        // Global history restore and update
        globalHistory[tid] = (history->globalHistory << 1) | taken;
        globalHistory[tid] &= historyRegisterMask;

        return;
    }

    // Update the predictor counters with the proper
    // resolution of the branch. Histories are updated
    // speculatively, restored upon squash() calls, and
    // recomputed upon update(squash = true) calls,
    // so they do not need to be updated.
    
    // Calculate the index to the correct counter using the
    // global history remembered at the start of branch 
    // prediction in lookup().  This updates the predictor 
    // counter on which the original prediction was made.  
    unsigned i = getPredictorIndex(history->globalHistory, branch_addr);
    
    // update predictor counter with actual branch taken value    
    if (taken) {
        globalCtrs[i]++;
    } else {
        globalCtrs[i]--;
    }

    // We're done with this history, now delete it.
    delete history;
}

void
GSelectBP::squash(ThreadID tid, void *bp_history)
{
    BPHistory *history = static_cast<BPHistory *>(bp_history);

    // Restore global history to state prior to this branch.
    globalHistory[tid] = history->globalHistory;

    // Delete this BPHistory now that we're done with it.
    delete history;
}

GSelectBP*
GSelectBPParams::create()
{
    return new GSelectBP(this);
}

/** Private functions */

inline
unsigned
GSelectBP::getPredictorIndex(unsigned &history, Addr &branch_addr)
{
    // The prediction index is the concatenation of n bits
    // from the global history register and m bits from the
    // program counter: i = n + m or i = m*bits(n) + n
    
    // Extract the global history component.
    unsigned n = history & globalHistoryMask;
    
    // Extract the PC component.
    unsigned m = (branch_addr >> instShiftAmt) & PCMask;
    
    // Concatenate into index and return;
    return (((m << globalHistoryBits) | n) & globalPredictorMask);
}

inline
bool
GSelectBP::getPrediction(unsigned &count)
{
    // Get the MSB of the count
    return (count >> (globalCtrBits - 1));
}


#ifdef DEBUG
int
GSelectBP::BPHistory::newCount = 0;
#endif
