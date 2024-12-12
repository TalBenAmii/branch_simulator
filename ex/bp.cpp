/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */
// minimal as possible
#include "bp_api.h"
#include <math.h>
#include <vector>

using namespace std;

uint8_t getXoredHistory(uint32_t pc, uint8_t history, int Shared, unsigned fsmSize);
//class forwarding
class BTB_ROW;
class BP;
// useful enums
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

// some constants
const static int USING_SHARE_LSB_BITS = 2;
const static int USING_SHARE_MID_BITS = 16;
const static int DEFAULT_HISTORY = 0;

// btb row structure
class BTB_ROW
{
  public:
    uint32_t tag, dest;
    uint8_t local_history;
    vector<State> local_FSM;
    bool valid;
    BTB_ROW(unsigned fsmSize, State defaultState, uint32_t tag = 0, uint32_t dest = 0,
            uint8_t history = DEFAULT_HISTORY)
        : tag(tag), dest(dest), local_history(history), local_FSM(fsmSize, defaultState), valid(false)
    {
    }
};
// branch predictor structure
class BP
{
  public:
    const static int ALIGNED_BITS = 2;
    const static int VALID_BITS_SIZE = 1;
    const static int DEST_SIZE = 30;

    unsigned btbSize, btbBitSize, historyBitSize, tagSize, tagBitSize, fsmSize;
    State defaultState;
    bool isGlobalHist, isGlobalTable;
    int Shared;
    SIM_stats stats;
    vector<State> global_FSM; // global predictor (global FSM)
    uint8_t global_history;   // global history (GHR)
    vector<BTB_ROW> BTB;      // includes local predictors (with tags, history, dests, valid)

    friend class BTB_ROW;

    BP() = default;
    ~BP() = default;
    // initializing BP
    BP(unsigned btbSize, unsigned historyBitSize, unsigned tagBitSize, unsigned fsmState, bool isGlobalHist,
       bool isGlobalTable, int Shared)
        : btbSize(btbSize), btbBitSize(static_cast<unsigned>(log2(btbSize))), historyBitSize(historyBitSize),
          tagSize(static_cast<unsigned>(pow(2, tagBitSize))), tagBitSize(tagBitSize),
          fsmSize(static_cast<unsigned>(pow(2, historyBitSize))), defaultState(static_cast<State>(fsmState)),
          isGlobalHist(isGlobalHist), isGlobalTable(isGlobalTable), Shared(Shared), stats(),
          global_FSM(fsmSize, defaultState), global_history(DEFAULT_HISTORY),
          BTB(btbSize, BTB_ROW(fsmSize, defaultState))
    {
        // calc predictor size
        stats.br_num = 0;
        stats.flush_num = 0;
        stats.size = isGlobalTable ? btbSize * (tagBitSize + DEST_SIZE + VALID_BITS_SIZE) + 2 * fsmSize
                                   : btbSize * (tagBitSize + DEST_SIZE + VALID_BITS_SIZE + 2 * fsmSize);
        stats.size += isGlobalHist ? historyBitSize : btbSize * historyBitSize;
    }
};

BP branchPredictor; // bss allocated, ugly but avoids slow heap allocations (not including vectors) and annoying memory leaks
// also good enough for one c file restriction

int BP_init(unsigned btbSize, unsigned historyBitSize, unsigned tagSize, unsigned fsmState, bool isGlobalHist,
            bool isGlobalTable, int Shared)
{
    try //initializing BP returning -1 incase of error
    {
        branchPredictor = BP(btbSize, historyBitSize, tagSize, fsmState, isGlobalHist, isGlobalTable, Shared);
    }
    catch (...)
    {
        return -1;
    }
    return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst)
{
    // fetching relevent data
    BTB_ROW &currRow = branchPredictor.BTB[(pc >> BP::ALIGNED_BITS) % branchPredictor.btbSize];
    uint32_t &currTag = currRow.tag;
    uint32_t pcTag = ((pc >> (BP::ALIGNED_BITS + branchPredictor.btbBitSize)) % branchPredictor.tagSize);
    uint8_t *currHistory = branchPredictor.isGlobalHist ? &branchPredictor.global_history : &currRow.local_history;
    vector<State> &currFsm = branchPredictor.isGlobalTable ? branchPredictor.global_FSM : currRow.local_FSM;
    State *currState = &currFsm[*currHistory];
    bool &currValid = currRow.valid;
    uint8_t indexedHistory;
    // fetching xored index of fsm when lshare/gshare is needed
    if (branchPredictor.isGlobalTable && branchPredictor.Shared != ShareType::NOT_USING_SHARE)
    {
        indexedHistory = getXoredHistory(pc, *currHistory, branchPredictor.Shared, branchPredictor.fsmSize);
        currState = &branchPredictor.global_FSM[indexedHistory];
    }
    // Getting curr state from global/local FSM
    // If row not initialized or initialized but tag isn't matching or matching but not taken: return false
    if (!currValid || currTag != pcTag || *currState == SNT ||
        *currState == WNT) // TODO: check if valid bit is needed in cond
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
    // fetching relevent data
    BTB_ROW &currRow = branchPredictor.BTB[(pc >> BP::ALIGNED_BITS) % branchPredictor.btbSize];
    uint32_t &currTag = currRow.tag;
    uint32_t pcTag = ((pc >> (BP::ALIGNED_BITS + branchPredictor.btbBitSize)) % branchPredictor.tagSize);
    uint8_t *currHistory = branchPredictor.isGlobalHist ? &branchPredictor.global_history : &currRow.local_history;
    vector<State> &currFsm = branchPredictor.isGlobalTable ? branchPredictor.global_FSM : currRow.local_FSM;
    State *currState = &currFsm[*currHistory];
    bool &currValid = currRow.valid;
    uint8_t indexedHistory;

    // fetching xored index of fsm when lshare/gshare is needed
    if (branchPredictor.isGlobalTable && branchPredictor.Shared != ShareType::NOT_USING_SHARE)
    {
        indexedHistory = getXoredHistory(pc, *currHistory, branchPredictor.Shared, branchPredictor.fsmSize);
        currState = &currFsm[indexedHistory];
    }

    // update handled brs and flushs
    ++branchPredictor.stats.br_num;
    if ((taken && (targetPc != pred_dst)) || (!taken && (pc + 4 != pred_dst)))
    {
        ++branchPredictor.stats.flush_num;
    }
    currRow.dest = targetPc; // updating dest in BTB to correct one
    // check if need to initialize row in cases of valid bit is 0 or tags are coliding
    if (currTag != pcTag || !currValid)
    {
        currValid = true;
        currTag = pcTag;
        currRow.dest = targetPc;
        if (!branchPredictor.isGlobalHist)
        {
            *currHistory = DEFAULT_HISTORY;
            currState = &currFsm[*currHistory];
            // get xored history with the new default history
            if (branchPredictor.isGlobalTable && branchPredictor.Shared != ShareType::NOT_USING_SHARE)
            {
                indexedHistory = getXoredHistory(pc, *currHistory, branchPredictor.Shared, branchPredictor.fsmSize);
                currState = &currFsm[indexedHistory];
            }
        }
        if (!branchPredictor.isGlobalTable)
        {
            currFsm.assign(branchPredictor.fsmSize, branchPredictor.defaultState);
            currState = &currFsm[*currHistory]; // update currState pointer to correct historyValue
        }
    }
    // update states
    if (taken && *currState + 1 <= State::ST)
    {
        *currState = static_cast<State>(static_cast<int>(*currState) + 1);
    }
    else if (!taken && *currState - 1 >= State::SNT)
    {
        *currState = static_cast<State>(static_cast<int>(*currState) - 1);
    }
    // update history
    *currHistory = static_cast<uint8_t>(((*currHistory << 1) | (taken ? 1 : 0)) % (branchPredictor.fsmSize));
}
// returning stats
void BP_GetStats(SIM_stats *curStats)
{
    *curStats = branchPredictor.stats;
}
// getting lshare/gshare xor result
uint8_t getXoredHistory(uint32_t pc, uint8_t history, int Shared, unsigned fsmSize)
{
    uint8_t doXorWithPc = Shared == USING_SHARE_LSB ? static_cast<uint8_t>((pc >> USING_SHARE_LSB_BITS) % fsmSize)
                                                    : static_cast<uint8_t>((pc >> USING_SHARE_MID_BITS) % fsmSize);
    return (history ^ doXorWithPc);
}