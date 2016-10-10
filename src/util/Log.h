#pragma once
#include <glog/logging.h>

#define COMMONLOG(_log_, _logtype_, _logctx_) _log_(_logtype_) << " [" << _logctx_ << ":" << __func__ << "] "
/* Context based logging macros */
#define CLog(_logtype_) COMMONLOG(LOG, _logtype_, getLogContext())
#define CVLog(_logtype_) COMMONLOG(VLOG, _logtype_, getLogContext())

#define ALog(_logtype_)   LOG(_logtype_) << myId()
#define AVLog(level)    VLOG(level) << myId()

/* Verbose logs */
#define LMSG                1
#define LCONFIG             2
