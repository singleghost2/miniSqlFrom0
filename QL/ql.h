//
// Created by 谢俊东 on 16/7/25.
//

#ifndef MINISQLFROM0_QL_MANAGER_H
#define MINISQLFROM0_QL_MANAGER_H

#include <iostream>
#include "../minisql.h"
#include "../RM/rm.h"
#include "../IX/ix.h"
#include "../SM/sm_manager.h"
#include "../Parse/parser.h"
#include "CondFilter.h"

//RC code
#define QL_DUP_ATTR_NAME (START_QL_ERR-1)
#define QL_DUP_TABLE_NAME (START_QL_ERR-2)
#define QL_ATTR_NOT_EXIST (START_QL_ERR-3)
#define QL_TABLE_NOT_EXIST (START_QL_ERR-4)
#define QL_INCOMPATIBLE_COMP_OP (START_QL_ERR-5)
#define QL_PRIMARY_KEY_DUP (START_QL_ERR - 6)
#define QL_EOF (START_QL_ERR-7)

using std::ostream;

class QL_Manager;

class CondFilter;

class QL_Node { //抽象node类
private:
    QL_Node &operator=(const QL_Node &ql_node);

public:
    QL_Node(const QL_Node &ql_node) : qlm(ql_node.qlm), tupleLength(ql_node.tupleLength),
                                      nAttrInfos(ql_node.nAttrInfos) {
        attrInfos = new AttrInfoInRecord[nAttrInfos];
        memcpy(attrInfos, ql_node.attrInfos, sizeof(AttrInfoInRecord) * nAttrInfos);
    }

    QL_Node(QL_Manager &qlm) : qlm(qlm) {}

    virtual ~QL_Node() { delete[] attrInfos; }

    //作为迭代器的三个方法, 纯虚函数,继承类必须实现!!!
    virtual void Open() = 0;

    virtual RC GetNext(RM_Record &rec) = 0;

    virtual void Close() = 0;

    virtual void Reset() = 0;

    //getters
    const AttrInfoInRecord * GetAttrList() const { return attrInfos; } //获得指向当前节点属性信息的指针
    int GetTupleLength() const { return tupleLength; }    //获得当前节点的元祖长度
    int GetAttrNum() const { return nAttrInfos; }         //获得当前节点的属性数量
protected:
    QL_Manager &qlm;
    int tupleLength;                //当前节点的记录长度
    AttrInfoInRecord *attrInfos; //记录当前节点输入的属性的所有信息
    int nAttrInfos;                //当前节点的输入属性数量
};

//投影node
class QL_ProjNode : public QL_Node {
private:
    QL_ProjNode(const QL_ProjNode &ql_node);

    QL_ProjNode &operator=(QL_ProjNode &ql_node);
    //禁用两个函数
public:
    QL_ProjNode(QL_Manager &qlm, QL_Node &prevNode, int nAttrs, const RelAttr projAttrs[]);

    virtual ~QL_ProjNode();

    virtual void Open();

    virtual RC GetNext(RM_Record &rec);

    virtual void Close();

    virtual void Reset();

private:
    char *buffer;
    vector<int> offsetInPrev;      //投影的属性在前一个节点的元组中的偏移
    QL_Node &prevNode;      //前一个节点
};

//选择节点, Condition中的条件
class QL_SelNode : public QL_Node {
private:
    QL_SelNode &operator=(QL_SelNode &ql_node);

public:
    QL_SelNode(const QL_SelNode &ql_node) : QL_Node(ql_node), prevNode(ql_node.prevNode), cond(ql_node.cond) {}

    QL_SelNode(QL_Manager &qlm, QL_Node &prevNode, Condition cond);

    virtual ~QL_SelNode();

    virtual void Open();

    virtual RC GetNext(RM_Record &rec);

    virtual void Close();

    virtual void Reset();

private:
    QL_Node &prevNode;  //前一个节点
    Condition cond;   //选择条件
};

//Join节点,有左右两个子节点,右节点是relNode, 左节点可以是relNode或joinNode
class QL_JoinNode : public QL_Node {
private:
    //禁用
    QL_JoinNode &operator=(QL_JoinNode &ql_node);

public:
    QL_JoinNode(QL_Manager &qlm, QL_Node &leftNode, QL_Node &rightNode);

    QL_JoinNode(const QL_JoinNode &ql_node) : QL_Node(ql_node), lSubNode(ql_node.lSubNode), rSubNode(ql_node.rSubNode) {
        buffer = new char[tupleLength];
        bRightNodeEOF = ql_node.bRightNodeEOF;
    }

    virtual ~QL_JoinNode() { delete[] buffer; }

    virtual void Open();

    virtual RC GetNext(RM_Record &rec);

    virtual void Close();

    virtual void Reset();

private:
    QL_Node &lSubNode;          //左节点
    QL_Node &rSubNode;       //右节点

    bool leftIsJoinNode;
    bool bRightNodeEOF;     //指示右节点是否scan到了末尾
    char *buffer;
};

//关系(表)的节点
class QL_RelNode : public QL_Node {
private:
    //禁用
    QL_RelNode &operator=(QL_RelNode &ql_node);

public:
    QL_RelNode(QL_Manager &qlm, const char *const relation, int relOffset, const Condition &indexCond);    //关系名来初始化
    QL_RelNode(QL_Manager &qlm, const char *const relation, int relOffset);

    virtual ~QL_RelNode();

    virtual void Open();

    virtual RC GetNext(RM_Record &rec);

    virtual void Close();

    virtual void Reset();

    void AddCondition(const Condition &cond);

    void AddIndexCondition(const Condition &cond);

private:
    void InitRelNodeInfos();

    const string relName;              //关系名
    shared_ptr<RelcatTuple> relCatTuple;   //关系的catalog信息
    RM_FileHandler rm_fileHandler;
    RM_FileScan rm_fileScan;
    IX_IndexHandler ix_indexHandler;
    IX_IndexScan ix_indexScan;

    bool useIndexScan;      //迭代时是否使用indexScan
    Condition indexCond;   //选择条件
    AttrInfoInRecord leftAttr;  //如果有conditon的情况,左属性

    vector<Condition> otherConds;   //除了有index的选择条件的其他条件
    vector<CondFilter> condFilters; //条件过滤器
    int relOffset;          //当前relNode在查询的整个复合record中的偏移
};

class QL_Manager {
    friend class QL_Node;

    friend class QL_RelNode;

    friend class QL_SelNode;

    friend class QL_ProjNode;

    friend class QL_JoinNode;

    friend class CondFilter;

private:
    SM_Manager &smm;
    IX_Manager &ixm;
    RM_Manager &rmm;

    RelcatTuple *relcatTuples;  //每次操作所有的relation catalog
    int nRelations;             //每次操作用到的表的数量
    AttrInfoInRecord *attrInfosArr;    //每次操作要用到的所有的attrInfo
    int ntotAttrInfo;                  //每次操作用的的所有attrInfo的数量

    bool HasDupAttrName(int nAttrs, const RelAttr Attrs[]);                 //检查是否有重复的属性名
    bool HasDupTableName(int nRelations, const char *const relations[]);   //检查是否有重复的表名
    RC CheckAttrValid(int nAttrs, const RelAttr *Attrs); //检查属性是否存在
    RC CheckCondsValid(int nConditions, const Condition *conditions);   //检查conditions中的属性是否存在,比较类型是否一致
    //检查conditions中的属性是否存在
    RC InitRelationAndAttrInfos(const char *const *relName, int nRelations);

    void GetAttrInfoByRelAttr(AttrInfoInRecord &attrInfo, const RelAttr &relAttr);  //通过RelAttr填充AttrInfo
    AttrType GetAttrType(const RelAttr &relAttr);   //获取属性类型
    void CleanUp();     //调用增删改查操作之后的清理工作
public:
    // Constructor
    QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm);

    ~QL_Manager() {}                         // Destructor
    RC Select(int nSelAttrs,        // # attrs in Select clause
              const RelAttr *selAttrs,       // attrs in Select clause
              int nRelations,       // # relations in From clause
              const char *const *relations, // relations in From clause
              int nConditions,      // # conditions in Where clause
              const Condition *conditions);  // conditions in Where clause
    RC Insert(const char *relName,           // relation to insert into
              int nValues,            // # values to insert
              const Value values[]);          // values to insert
    RC Delete(const char *relName,            // relation to delete from
              int nConditions,         // # conditions in Where clause
              const Condition conditions[]);  // conditions in Where clause
    RC Update(const char *relName,            // relation to update
              const RelAttr &updAttr,         // attribute to update
              const int bIsValue,             // 0/1 if RHS of = is attribute/value
              const RelAttr &rhsRelAttr,      // attr on RHS of =
              const Value &rhsValue,          // value on RHS of =
              int nConditions,              // # conditions in Where clause
              const Condition conditions[]);  // conditions in Where clause
};

void QL_PrintError(RC rc);

#endif //MINISQLFROM0_QL_MANAGER_H
