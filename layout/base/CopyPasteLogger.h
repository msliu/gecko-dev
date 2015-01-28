/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CopyPasteLog_h
#define CopyPasteLog_h

#include "prlog.h"

namespace mozilla {
#ifdef PR_LOGGING

PRLogModuleInfo* GetCopyPasteLog();

#ifndef CP_LOG_BASE
#define CP_LOG_BASE(...) PR_LOG(GetCopyPasteLog(), PR_LOG_DEBUG, (__VA_ARGS__));
#endif

#ifndef CP_LOGV_BASE
#define CP_LOGV_BASE(...)                                                      \
  PR_LOG(GetCopyPasteLog(), PR_LOG_DEBUG + 1, (__VA_ARGS__));
#endif

#else

#ifndef CP_LOGV_BASE
#define CP_LOGV_BASE(...)
#endif

#ifndef CP_LOGV_BASE
#define CP_LOGV_BASE(...)
#endif

#endif // PR_LOGGING

} // namespace mozilla

#endif // CopyPasteLog_h
