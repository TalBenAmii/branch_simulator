/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <math.h>
#include <vector>

using namespace std;

class BTB_ROW;
class BP;

enum State
{
    SNT = 0,
    WNT = 1,
    WT = 2,
    ST = 3
};
enum ShareType
{
    NOT_USING_SHARE = 0,
    USING_SHARE_LSB = 1,
    USING_SHARE_MID = 2
};

const static int USING_SHARE_LSB_BITS = 2;
const static int USING_SHARE_MID_BITS = 16;
const static int DEFAULT_HISTORY = 0;

class BP
{
  public:
    const static int ALIGNED_BITS = 2;
    const static int VALID_BITS = 1;

    unsigned btbSize, historySize, tagSize, fsmState;
    bool isGlobalHist, isGlobalTable;
    int Shared;
    SIM_stats stats;
    vector<BTB_ROW> BTB;
    vector<State> global_FSM;
    uint8_t global_history;
    friend class BTB_ROW;

    BP() = default;

    BP(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState, bool isGlobalHist,
       bool isGlobalTable, int Shared)
        : btbSize(btbSize), historySize(historySize), tagSize(tagSize), fsmState(fsmState), isGlobalHist(isGlobalHist),
          isGlobalTable(isGlobalTable), Shared(Shared), stats(), BTB(), global_FSM(), global_history(DEFAULT_HISTORY)
    {
        BTB.reserve(btbSize);

        global_FSM.reserve(historySize);
        for (unsigned i = 0; i < historySize; ++i)
        {
            global_FSM[i] = static_cast<State>(this->fsmState);
        }
        // calc predictor size
        //   Predictor size: #BHRs × (tag_size + valid + history_size) + 2 × 2 history_size
        stats.size = btbSize * (tagSize + VALID_BITS + historySize) + 2 * pow(2, historySize);
    }
};

BP branchPredictor;

class BTB_ROW
{
  public:
    int tag;
    uint32_t dest;
    uint8_t local_history;
    vector<State> local_FSM;
    BTB_ROW(int tag = -1, uint32_t dest = 0, uint8_t history = DEFAULT_HISTORY)
        : tag(tag), dest(dest), local_history(history), local_FSM()
    {
        local_FSM.reserve(branchPredictor.historySize);
        for (unsigned i = 0; i < branchPredictor.historySize; ++i)
        {
            local_FSM[i] = static_cast<State>(branchPredictor.fsmState);
        }
    }
};

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState, bool isGlobalHist,
            bool isGlobalTable, int Shared)
{
    try
    {
        branchPredictor = BP(btbSize, historySize, tagSize, fsmState, isGlobalHist, isGlobalTable, Shared);
        // initialize btb rows
        for (unsigned i = 0; i < btbSize; ++i)
        {
            branchPredictor.BTB[i] = BTB_ROW();
        }
    }
    catch (...)
    {
        return -1;
    }
    return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst)
{
    // condition checking for local FSMs
    BTB_ROW &currRow = branchPredictor.BTB[(pc >> BP::ALIGNED_BITS) % branchPredictor.btbSize];
    int &currTag = currRow.tag;
    int pcTag = ((pc >> (BP::ALIGNED_BITS + branchPredictor.btbSize)) % branchPredictor.tagSize);
    uint8_t &currHistory = branchPredictor.isGlobalHist ? branchPredictor.global_history : currRow.local_history;
    State &currState =
        branchPredictor.isGlobalTable ? branchPredictor.global_FSM[currHistory] : currRow.local_FSM[currHistory];

    // update currHistory according to the need of l/g-share
    if (branchPredictor.isGlobalTable == true && branchPredictor.Shared > 0)
    {
        // false: L-share, true: G-share
        bool shareType = branchPredictor.isGlobalHist ? true : false;
        // calculating history and pc regs for xor operation
        uint8_t doXorWithHistory = shareType ? branchPredictor.global_history : currHistory;
        uint8_t doXorWithPc = branchPredictor.Shared == USING_SHARE_LSB
                                  ? (pc > USING_SHARE_LSB_BITS) % branchPredictor.historySize
                                  : (pc > USING_SHARE_MID_BITS) % branchPredictor.historySize;
        currHistory = doXorWithHistory ^ doXorWithPc;
        // updating correct state
        currState = branchPredictor.global_FSM[currHistory];
    }
    // getting curr state from global/local FSM
    // if row not initialized or initialized but tag isn't matching or matching but not taken: return false
    if (currTag == -1 || currTag != pcTag || currState == SNT || currState == WNT)
    {
        *dst = pc + 4;
        return false;
    }
    // else it's taken

    *dst = currRow.dest;
    return true;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst)
{

    BTB_ROW &currRow = branchPredictor.BTB[(pc >> BP::ALIGNED_BITS) % branchPredictor.btbSize];
    int &currTag = currRow.tag;
    int pcTag = ((pc >> (BP::ALIGNED_BITS + branchPredictor.btbSize)) % branchPredictor.tagSize);
    uint8_t &currHistory = branchPredictor.isGlobalHist ? branchPredictor.global_history : currRow.local_history;
    State &currState =
        branchPredictor.isGlobalTable ? branchPredictor.global_FSM[currHistory] : currRow.local_FSM[currHistory];
    vector<State> &currFsm =
        branchPredictor.isGlobalHist ? branchPredictor.global_FSM : branchPredictor.BTB[currHistory].local_FSM;

    // update handled brs and flushs
    ++branchPredictor.stats.br_num;
    if (targetPc != pred_dst)
    {
        ++branchPredictor.stats.flush_num; // TODO: fix flush count
    }
    // check if need to update btb row
    if (currTag != pcTag)
    {
        currTag = pcTag; // TODO: update dest
        currFsm = vector<State>();
        currFsm.reserve(branchPredictor.historySize);
        for (auto &state : currFsm)
        {
            state = static_cast<State>(branchPredictor.fsmState);
        }
        currHistory = DEFAULT_HISTORY;
    }
    else
    {
        if (taken && currState + 1 < State::ST)
        {
            currState = static_cast<State>(static_cast<int>(currState) + 1);
        }
        else if (taken && currState - 1 > State::SNT)
        {
            currState = static_cast<State>(static_cast<int>(currState) - 1);
        }
    }
    return;
}

void BP_GetStats(SIM_stats *curStats)
{
    *curStats = branchPredictor.stats;
    return;
}
