/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CopyPasteManager_h__
#define CopyPasteManager_h__

#include "nsISelectionListener.h"

class nsIPresShell;

namespace mozilla {
class InteractiveCaret;

class CopyPasteManager MOZ_FINAL : public nsISelectionListener
{
public:
  explicit CopyPasteManager(nsIPresShell* aPresShell);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONLISTENER

private:
  ~CopyPasteManager();

  // Utility function
  dom::Selection* GetSelection();
  already_AddRefed<nsFrameSelection> GetFrameSelection();
  nsIContent* GetFocusedContent();

  void UpdateCarets();

  static nsIFrame* FindFirstNodeWithFrame(nsIDocument* aDocument,
                                          nsRange* aRange,
                                          nsFrameSelection* aFrameSelection,
                                          bool aBackward,
                                          int& aOutOffset);

  nsIPresShell* mPresShell;
  nsRefPtr<InteractiveCaret> mFirstCaret;
  nsRefPtr<InteractiveCaret> mSecondCaret;
};
} // namespace mozilla

#endif //CopyPasteManager_h__
