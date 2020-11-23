/*
 * Portions Copyright (c) 2020 Huawei Technologies Co.,Ltd.
 * Portions Copyright (c) 1998-2016, PostgreSQL Global Development Group
 *
 * openGauss is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * ---------------------------------------------------------------------------------------
 * 
 * opfusion.h
 *     Operator Fusion's definition for bypass.
 * 
 * IDENTIFICATION
 *        src/include/opfusion/opfusion.h
 *
 * ---------------------------------------------------------------------------------------
 */

#ifndef SRC_INCLUDE_OPFUSION_OPFUSION_H_
#define SRC_INCLUDE_OPFUSION_OPFUSION_H_
#include "auditfuncs.h"
#include "commands/prepare.h"
#include "opfusion/opfusion_util.h"
#include "opfusion/opfusion_scan.h"
#include "lib/stringinfo.h"
#include "nodes/parsenodes.h"
#include "pgxc/pgxcnode.h"
#include "utils/numeric.h"
#include "utils/numeric_gs.h"
#include "utils/plancache.h"
#include "utils/syscache.h"

extern void report_qps_type(CmdType commandType);
extern const char* getBypassReason(FusionType result);
extern void BypassUnsupportedReason(FusionType result);
extern void ExecCheckXactReadOnly(PlannedStmt* plannedstmt);
extern FusionType getSelectFusionType(List* stmt_list, ParamListInfo params);
extern FusionType getInsertFusionType(List* stmt_list, ParamListInfo params);
extern FusionType getUpdateFusionType(List* stmt_list, ParamListInfo params);
extern FusionType getDeleteFusionType(List* stmt_list, ParamListInfo params);
class OpFusion : public BaseObject {
public:
    OpFusion(MemoryContext context, CachedPlanSource* psrc, List* plantree_list);

    virtual ~OpFusion(){};

    static void* FusionFactory(
        FusionType type, MemoryContext context, CachedPlanSource* psrc, List* plantree_list, ParamListInfo params);

    static FusionType getFusionType(CachedPlan* cplan, ParamListInfo params, List* plantree_list);

    static void setCurrentOpFusionObj(OpFusion* obj);

    static bool process(int op, StringInfo msg, char* completionTag, bool isTopLevel);

    void CopyFormats(int16* formats, int numRFormats);

    void updatePreAllocParamter(StringInfo msg);

    void useOuterParameter(ParamListInfo params);

    void decribe(StringInfo msg);

    virtual bool execute(long max_rows, char* completionTag)
    {
        Assert(false);
        return false;
    }

    virtual void close()
    {
        Assert(false);
        return;
    }

    void setPreparedDestReceiver(DestReceiver* preparedDest);

    Datum CalFuncNodeVal(Oid functionId, List* args, bool* is_null);

    Datum EvalSimpleArg(Node* arg, bool* is_null);

    static void tearDown(OpFusion* opfusion);

    static void clearForCplan(OpFusion* opfusion, CachedPlanSource* psrc);

    void checkPermission();

    void setReceiver();

    void initParams(ParamListInfo params);

    void executeInit();

    void executeEnd();

    void auditRecord();

    static bool isQueryCompleted();

    void bindClearPosition();

public:
    struct ParamLoc {
        int paramId;
        int scanKeyIndx;
    };

    CachedPlanSource* m_psrc; /* to get m_cacheplan in PBE */

    CachedPlan* m_cacheplan;

    PlannedStmt* m_planstmt; /* m_cacheplan->stmt_list in PBE, plantree in non-PBE */

    bool m_isFirst; /* be true if is the fisrt execute in PBE */

    MemoryContext m_context;

    MemoryContext m_tmpContext; /* use for tmp memory allocation. */

    ParamListInfo m_params;

    ParamListInfo m_outParams; /* use outer side parameter. */

    int m_paramNum;

    ParamLoc* m_paramLoc; /* location of m_params, include paramId and the location in indexqual */

    Oid m_reloid; /* relation oid of range table */

    TupleDesc m_tupDesc; /* tuple descriptor */

    TupleTableSlot* m_reslot; /* result slot */

    int16* m_attrno; /* target attribute number, length is m_tupDesc->natts */

    Datum* m_values;

    bool* m_isnull;

    Datum* m_tmpvals; /* for mapping m_values */

    bool* m_tmpisnull; /* for mapping m_isnull */

    DestReceiver* m_receiver;

    bool m_isInsideRec;

    bool m_is_pbe_query;

    int16* m_rformats;

    bool m_isCompleted;

    long m_position;
};

class SelectFusion : public OpFusion {
public:
    SelectFusion(MemoryContext context, CachedPlanSource* psrc, List* plantree_list, ParamListInfo params);

    ~SelectFusion(){};

    bool execute(long max_rows, char* completionTag);

    void close();

private:
    class ScanFusion* m_scan;

    int64 m_limitCount;

    int64 m_limitOffset;
};

class InsertFusion : public OpFusion {
public:
    InsertFusion(MemoryContext context, CachedPlanSource* psrc, List* plantree_list, ParamListInfo params);

    ~InsertFusion(){};

    bool execute(long max_rows, char* completionTag);

private:
    void refreshParameterIfNecessary();

    EState* m_estate;

    /* for func/op expr calculation */
    FuncExprInfo* m_targetFuncNodes;

    int m_targetFuncNum;

    int m_targetParamNum;

    bool m_is_bucket_rel;
};

class UpdateFusion : public OpFusion {
public:
    UpdateFusion(MemoryContext context, CachedPlanSource* psrc, List* plantree_list, ParamListInfo params);

    ~UpdateFusion(){};

    bool execute(long max_rows, char* completionTag);

private:
    class ScanFusion* m_scan;

    HeapTuple heapModifyTuple(HeapTuple tuple);

    void refreshTargetParameterIfNecessary();

    EState* m_estate;

    /* targetlist */
    int m_targetNum;

    int m_targetParamNum;

    Datum* m_targetValues;

    bool* m_targetIsnull;

    int* m_targetConstLoc;

    ParamLoc* m_targetParamLoc;

    /* for func/op expr calculation */
    FuncExprInfo* m_targetFuncNodes;

    int m_targetFuncNum;

    bool m_is_bucket_rel;
};

class DeleteFusion : public OpFusion {
public:
    DeleteFusion(MemoryContext context, CachedPlanSource* psrc, List* plantree_list, ParamListInfo params);

    ~DeleteFusion(){};

    bool execute(long max_rows, char* completionTag);

private:
    class ScanFusion* m_scan;

    EState* m_estate;

    bool m_is_bucket_rel;
};

class MotJitSelectFusion: public OpFusion {
public:
    MotJitSelectFusion(MemoryContext context, CachedPlanSource *psrc, List *plantree_list, ParamListInfo params);

    ~MotJitSelectFusion() {};

    bool execute(long max_rows, char *completionTag);
};

class MotJitModifyFusion: public OpFusion {
public:
    MotJitModifyFusion(MemoryContext context, CachedPlanSource *psrc, List *plantree_list, ParamListInfo params);

    ~MotJitModifyFusion() {};

    bool execute(long max_rows, char *completionTag);

private:
    EState* m_estate;
    CmdType m_cmdType;
};

class SelectForUpdateFusion : public OpFusion {
public:
    SelectForUpdateFusion(MemoryContext context, CachedPlanSource* psrc, List* plantree_list, ParamListInfo params);

    ~SelectForUpdateFusion(){};

    bool execute(long max_rows, char* completionTag);

    void close();

private:
    class ScanFusion* m_scan;

    EState* m_estate;

    int64 m_limitCount;

    int64 m_limitOffset;

    bool m_is_bucket_rel;
};

class AggFusion : public OpFusion {

public:
    AggFusion(MemoryContext context, CachedPlanSource* psrc, List* plantree_list, ParamListInfo params);

    ~AggFusion(){};

    bool execute(long max_rows, char* completionTag);

protected:

    typedef void (AggFusion::*aggSumFun)(Datum *transVal, bool transIsNull, Datum *inVal, bool inIsNull);

    /* agg sum function */
    void agg_int2_sum(Datum *transVal, bool transIsNull, Datum *inVal, bool inIsNull);

    void agg_int4_sum(Datum *transVal, bool transIsNull, Datum *inVal, bool inIsNull);

    void agg_int8_sum(Datum *transVal, bool transIsNull, Datum *inVal, bool inIsNull);

    void agg_numeric_sum(Datum *transVal, bool transIsNull, Datum *inVal, bool inIsNull);

    inline void init_var_from_num(Numeric num, NumericVar *dest)
    {
        Assert(!NUMERIC_IS_BI(num));
        dest->ndigits = NUMERIC_NDIGITS(num);
        dest->weight = NUMERIC_WEIGHT(num);
        dest->sign = NUMERIC_SIGN(num);
        dest->dscale = NUMERIC_DSCALE(num);
        dest->digits = NUMERIC_DIGITS(num);
        dest->buf = NULL;       /* digits array is not palloc'd */
    }

    class ScanFusion* m_scan;

    aggSumFun m_aggSumFunc;
};

class SortFusion: public OpFusion {

public:

    SortFusion(MemoryContext context, CachedPlanSource* psrc, List* plantree_list, ParamListInfo params);

    ~SortFusion(){};

    bool execute(long max_rows, char *completionTag);

protected:

    class ScanFusion* m_scan;

    TupleDesc  m_scanDesc;
};

#if 0
class NestLoopFusion: public OpFusion {

public:

    NestLoopFusion(MemoryContext context, CachedPlanSource* psrc, List* plantree_list, ParamListInfo params);

    ~NestLoopFusion(){};

    bool execute(long max_rows, char *completionTag);

protected:

    class ScanFusion* m_lscan;
    class RScanFusion* m_rscan;
};
#endif

#endif /* SRC_INCLUDE_OPFUSION_OPFUSION_H_ */
