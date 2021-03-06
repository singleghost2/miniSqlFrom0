//
// Created by 谢俊东 on 16/7/15.
//

#include "rm.h"


bool myComp::less_than(const void *value1, const void *value2, AttrType attrType, int attrLength) {
    switch (attrType) {
        case INT:
            return *(int *) value1 < *(int *) value2;
        case FLOAT:
            return *(float *) value1 < *(float *) value2;
        case STRING:
            return strncmp((const char *) value1, (const char *) value2, attrLength) < 0;
    }
}

bool myComp::less_than_or_equal(const void *value1, const void *value2, AttrType attrType, int attrLength) {
    switch (attrType) {
        case INT:
            return *(int *) value1 <= *(int *) value2;
        case FLOAT:
            return *(float *) value1 <= *(float *) value2;
        case STRING:
            return (strncmp((const char *) value1, (const char *) value2, attrLength) <= 0);
    }
}

bool myComp::greater_than(const void *value1, const void *value2, AttrType attrType, int attrLength) {
    switch (attrType) {
        case INT:
            return *(int *) value1 > *(int *) value2;
        case FLOAT:
            return *(float *) value1 > *(float *) value2;
        case STRING:
            return strncmp((const char *) value1, (const char *) value2, attrLength) > 0;
    }
}

bool myComp::greater_than_or_equal(const void *value1, const void *value2, AttrType attrType, int attrLength) {
    switch (attrType) {
        case INT:
            return *(int *) value1 >= *(int *) value2;
        case FLOAT:
            return *(float *) value1 >= *(float *) value2;
        case STRING:
            return strncmp((const char *) value1, (const char *) value2, attrLength) >= 0;
    }
}

bool myComp::equal_To(const void *value1, const void *value2, AttrType attrType, int attrLength) {
    switch (attrType) {
        case INT:
            return *(int *) value1 == *(int *) value2;
        case FLOAT:
            return *(float *) value1 == *(float *) value2;
        case STRING:
            return strncmp((const char *) value1, (const char *) value2, attrLength) == 0;
    }
}

bool myComp::not_equal_to(const void *value1, const void *value2, AttrType attrType, int attrLength) {
    switch (attrType) {
        case INT:
            return *(int *) value1 != *(int *) value2;
        case FLOAT:
            return *(float *) value1 != *(float *) value2;
        case STRING:
            return strncmp((const char *) value1, (const char *) value2, attrLength) != 0;
    }
}

RC RM_FileScan::OpenScan(const RM_FileHandler &rm_fileHandler, AttrType attrType, int attrLength, int attrOffset,
                         CompOp compOp, void *value, ClientHint pinHint) {
    this->rm_fileHandler = rm_fileHandler;
    this->attrType = attrType;
    this->attrLength = attrLength;
    this->attrOffset = attrOffset;
    this->compOp = compOp;          //比较操作的类型
    this->value = value;            //比较的值
    this->pinHint = pinHint;        //pinHint暂时不用,默认NO_HINT
    this->cur_rid = RID(0, -1);    //初始化当前扫描到的rid为一个不合法的值
    this->bScanIsOpen = true;
    return 0;
}

RC RM_FileScan::CloseScan() {
    bScanIsOpen = false;
    return 0;
}

RC RM_FileScan::GetNextRec(RM_Record &rec) {
    RC rc;
    if (!bScanIsOpen)
        return -1;
    int page = cur_rid.getPageNum();
    int slot = cur_rid.getSlot();
    RM_PageHandler rm_pageHandler;
    if (rc = rm_fileHandler.GetThisPage(page, rm_pageHandler)) return RM_EOF;   //如果不存在第0页
    while (true) {

        while (rm_pageHandler.GetNextRecord(slot, rec) == RM_PAGE_RECORD_EOF) {
            try {
                page = rm_pageHandler.GetPageNum();
                rm_fileHandler.UnpinPage(page);
                rm_fileHandler.GetNextPage(page, rm_pageHandler);
                slot = -1;
            } catch (page_not_found_exception) {
                return RM_EOF;
            }
        }
        if (compOp == NO_OP) {
            cur_rid = rec.GetRid();
            return 0;
        }
        switch (compOp) {
            case LE_OP:
                comparator = &myComp::less_than_or_equal;
                break;
            case LT_OP:
                comparator = &myComp::less_than;
                break;
            case GT_OP:
                comparator = &myComp::greater_than;
                break;
            case GE_OP:
                comparator = &myComp::greater_than_or_equal;
                break;
            case EQ_OP:
                comparator = &myComp::equal_To;
                break;
            case NE_OP:
                comparator = &myComp::not_equal_to;
                break;
            default:
                break;
        }
        char *pRecord = rec.GetContent();

        if ((*comparator)(pRecord + attrOffset, this->value, attrType, attrLength)) {
            cur_rid = rec.GetRid();
            return 0;
        } else {
            cur_rid = rec.GetRid();
            slot = cur_rid.getSlot();
            continue;
        }
    }

    return 0;
}

