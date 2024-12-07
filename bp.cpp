/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <math.h>
#include <vector>

// usefull macros
#define GET_BTB_ROW(pc) branchPredictor.BTB[(pc >> BP::ALIGNED_BITS) % branchPredictor.btbSize]
#define GET_TAG(pc) (pc % (BP::ALIGNED_BITS + branchPredictor.btbSize))
// #define GET_STATE(row) (row.local_FSM[row.local_history])

using namespace std;

class BTB_ROW;
class BP;

// enums default values start from 0
enum State
{
    SNT,
    WNT,
    WT,
    ST
};
// enums default values start from 0
enum ShareType
{
    NOT_USING_SHARE,
    USING_SHARE_LSB,
    USING_SHARE_MID
};

const static int USING_SHARE_LSB_BITS = 2;
const static int USING_SHARE_MID_BITS = 16;

BP branchPredictor;

class BTB_ROW
{
  public:
    uint32_t tag, dest;
    uint8_t local_history;
    vector<State> local_FSM;
    BTB_ROW(uint32_t tag = -1, uint32_t dest = -1, uint8_t history = 0)
        : tag(tag), dest(dest), local_history(local_history), local_FSM()
    {
        local_FSM.reserve(branchPredictor.historySize);
        for (auto &state : local_FSM)
        {
            state = static_cast<State>(branchPredictor.fsmState);
        }
    }
};

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

    BP(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState, bool isGlobalHist,
       bool isGlobalTable, int Shared)
        : btbSize(btbSize), historySize(historySize), tagSize(tagSize), fsmState(fsmState), isGlobalHist(isGlobalHist),
          isGlobalTable(isGlobalTable), Shared(Shared), stats(), BTB(), global_FSM(), global_history(0)
    {
        BTB.reserve(btbSize);
        global_FSM.reserve(historySize);
        // calc predictor size
        //   Predictor size: #BHRs × (tag_size + valid + history_size) + 2 × 2 history_size
        stats.size = btbSize * (tagSize + VALID_BITS + historySize) + 2 * pow(2, historySize);
    }
};

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState, bool isGlobalHist,
            bool isGlobalTable, int Shared)
{
    try
    {
        BP branchPredictor(btbSize, historySize, tagSize, fsmState, isGlobalHist, isGlobalTable, Shared);
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
    BTB_ROW currRow = GET_BTB_ROW(pc);
    uint32_t currTag = currRow.tag, pcTag = GET_TAG(pc);
    uint8_t currStateIndex = branchPredictor.isGlobalHist ? branchPredictor.global_history : currRow.local_history;
    State currState =
        branchPredictor.isGlobalTable ? branchPredictor.global_FSM[currStateIndex] : currRow.local_FSM[currStateIndex];

    // update currHistory according to the need of l/g-share
    if (branchPredictor.isGlobalTable == true && branchPredictor.Shared > 0)
    {
        // false: L-share, true: G-share
        bool shareType = branchPredictor.isGlobalHist ? true : false;
        // calculating history and pc regs for xor operation
        uint8_t doXorWithHistory = shareType ? branchPredictor.global_history : currStateIndex;
        uint8_t doXorWithPc = branchPredictor.Shared == USING_SHARE_LSB
                                  ? (pc > USING_SHARE_LSB_BITS) % branchPredictor.historySize
                                  : (pc > USING_SHARE_MID_BITS) % branchPredictor.historySize;
        currStateIndex = doXorWithHistory ^ doXorWithPc;
        // updating correct state
        currState = branchPredictor.global_FSM[currStateIndex];
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
    // update handled brs , and flushs
    ++branchPredictor.stats.br_num;
    if (taken == false)
    {
		++branchPredictor.stats.flush_num;
    }

    return;
}

void BP_GetStats(SIM_stats *curStats)
{
	*curStats=branchPredictor.stats;
    return;
}
