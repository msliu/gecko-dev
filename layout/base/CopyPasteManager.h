/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CopyPasteManager_h__
#define CopyPasteManager_h__

#include "nsCOMPtr.h"
#include "nsISelectionListener.h"
#include "nsRefPtr.h"
#include "nsWeakReference.h"
#include "mozilla/EventForwards.h"
#include "mozilla/WeakPtr.h"

class nsIPresShell;

namespace mozilla {
class AccessibleCaret;
class CopyPasteEventHub;
class CopyPasteManagerGlue;

class CopyPasteManager : public nsISelectionListener
{
public:
  CopyPasteManager();
  virtual void Init(nsIPresShell* aPresShell);
  virtual void Terminate();
  virtual nsEventStatus HandleEvent(WidgetEvent* aEvent);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONLISTENER

protected:
  /**
   * Indicate which part of caret we are dragging at.
   */
  MOZ_BEGIN_NESTED_ENUM_CLASS(DragMode, uint8_t)
    NONE,
    FIRST_CARET,
    SECOND_CARET
  MOZ_END_ENUM_CLASS(DragMode)

  MOZ_BEGIN_NESTED_ENUM_CLASS(CaretMode, uint8_t)
    NONE,
    CURSOR,
    SELECTION
  MOZ_END_ENUM_CLASS(CaretMode);

  virtual ~CopyPasteManager();

  virtual nsEventStatus OnPress(const nsPoint& aPoint);
  virtual nsEventStatus OnDrag(const nsPoint& aPoint);
  virtual nsEventStatus OnRelease();
  virtual nsEventStatus OnLongTap(const nsPoint& aPoint);
  virtual nsEventStatus OnTap(const nsPoint& aPoint);
  virtual void OnScrollStart();
  virtual void OnScrollEnd();
  virtual void OnReflow();

  void UpdateCarets();
  void HideCarets();

  bool mHasInited;
  DragMode mDragMode;
  CaretMode mCaretMode;
  nscoord mCaretCenterToDownPointOffsetY;
  nsRefPtr<AccessibleCaret> mFirstCaret;
  nsRefPtr<AccessibleCaret> mSecondCaret;
  nsRefPtr<CopyPasteManagerGlue> mGlue;
  nsRefPtr<CopyPasteEventHub> mCopyPasteEventHub;

  friend class CopyPasteEventHub;
};

MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteManager::DragMode)
MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteManager::CaretMode)

} // namespace mozilla

#endif //CopyPasteManager_h__
