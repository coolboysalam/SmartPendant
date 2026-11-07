#pragma once
#include <cstdint>
class Result {
public:
 enum ResultCode { RESULT_OK=0, ERR_INVALID_ITEM=1, ERR_NOT_IMPLEMENTED=2, ERR_I2C_TIMEOUT=3 };
 Result(ResultCode code=RESULT_OK): v(code) {}
 bool IsGood() const { return v==RESULT_OK; }
 bool IsBad() const { return v!=RESULT_OK; }
 bool operator!=(ResultCode code) const { return v!=code; }
 bool operator==(ResultCode code) const { return v==code; }
private: ResultCode v;
};
