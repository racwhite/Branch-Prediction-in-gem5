/*
Rachel White
CSE420
Project 2
gselect.hh
 */

#ifndef __CPU_PRED_GSELECT_PRED_HH__
#define __CPU_PRED_GSELECT_PRED_HH__

#include <vector>

#include "base/sat_counter.hh"
#include "base/types.hh"
#include "cpu/pred/bpred_unit.hh"
#include "params/GSelectBP.hh"

/**
 * Implements a global select branch predictor.   It has a global 
 * predictor, which uses a global history to index into a table of 
 * counters.  The global history register is speculatively updated.
 */
class GSelectBP : public BPredUnit
{
  public:
    /**
     * Default branch predictor constructor.
     */
    GSelectBP(const GSelectBPParams *params);

    /**
     * Looks up the given address in the branch predictor and returns
     * a true/false value as to whether it is taken.  Also creates a
     * BPHistory object to store any state it will need on squash/update.
     * @param branch_addr The address of the branch to look up.
     * @param bp_history Pointer that will be set to the BPHistory object.
     * @return Whether or not the branch is taken.
     */
    bool lookup(ThreadID tid, Addr branch_addr, void * &bp_history);

    /**
     * Records that there was an unconditional branch, and modifies
     * the bp history to point to an object that has the previous
     * global history stored in it.
     * @param bp_history Pointer that will be set to the BPHistory object.
     */
    void uncondBranch(ThreadID tid, Addr pc, void * &bp_history);

    /**
     * Updates the branch predictor to Not Taken if a Branch Target
     * Buffer (BTB) entry is invalid or not found.
     * @param branch_addr The address of the branch to look up.
     * @param bp_history Pointer to any bp history state.
     */
    void btbUpdate(ThreadID tid, Addr branch_addr, void * &bp_history);

    /**
     * Updates the branch predictor with the actual result of a branch.
     * @param branch_addr The address of the branch to update.
     * @param taken Whether or not the branch was taken.
     * @param bp_history Pointer to the BPHistory object that was created
     * when the branch was predicted.
     * @param squashed is set when this function is called during a squash
     * operation.
     * @param inst Static instruction information
     * @param corrTarget Resolved target of the branch (only needed if
     * squashed)
     */
    void update(ThreadID tid, Addr branch_addr, bool taken, void *bp_history,
                bool squashed, const StaticInstPtr & inst, Addr corrTarget);

    /**
     * Restores the global branch history on a squash.
     * @param bp_history Pointer to the BPHistory object that has the
     * previous global branch history in it.
     */
    void squash(ThreadID tid, void *bp_history);

  private:
    /**
     * Returns if the branch should be taken or not, given a counter
     * value.
     * @param count The counter value.
     */
    inline bool getPrediction(unsigned &count);
    
    inline unsigned getPredictorIndex(unsigned &history, Addr &branch_addr);

    /** Updates global history as taken. */
    inline void updateGlobalHistTaken(ThreadID tid);

    /** Updates global history as not taken. */
    inline void updateGlobalHistNotTaken(ThreadID tid);

    /**
     * The branch history information that is created upon predicting
     * a branch.  It will be passed back upon updating and squashing,
     * when the BP can use this information to update/restore its
     * state properly.
     */
    struct BPHistory {
#ifdef DEBUG
        BPHistory()
        { newCount++; }
        ~BPHistory()
        { newCount--; }

        static int newCount;
#endif
        /** This holds a copy of the global history 
            at start of the branch prediction process
            in lookup(). */ 
        unsigned globalHistory;
        /** The branch prediction made by lookup(). */
        bool globalPredTaken;
    };

    /** Number of entries in the global predictor. */
    unsigned globalPredictorSize;
    
    /** Number of bits in the global predictor index. */
    unsigned globalPredictorBits;
    unsigned globalPredictorMask;

    /** Number of bits of the global predictor's counters. */
    unsigned globalCtrBits;

    /** Array of counters that make up the global predictor. 
        The index for this array is a concatenation of PC 
        and global history bits. */
    std::vector<SatCounter> globalCtrs;

    /** Global history register. Contains as much history as specified by
     *  globalHistoryBits. Actual number of bits used is determined by
     *  globalHistoryMask. Indexed by Thread ID (TID). */
    std::vector<unsigned> globalHistory;

    /** Number of bits in the globalCtrs index from the program counter */
    unsigned globalPCBits;
    
    unsigned PCMask;
    
    /** Number of bits for the global history. Determines maximum number of
        entries in global. This is also the number of bits in the 
        globalsCtrs index from globalHistory. */
    unsigned globalHistoryBits;

    /** Mask to apply to globalHistory to access global history table.
     *  Based on globalPredictorSize.*/
    unsigned globalHistoryMask;

    /** Mask to control how much history is stored. All of it might not be
     *  used. */
    unsigned historyRegisterMask;
};

#endif // __CPU_PRED_GSELECT_PRED_HH__
