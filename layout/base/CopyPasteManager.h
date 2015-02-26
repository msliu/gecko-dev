/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CopyPasteManager_h__
#define CopyPasteManager_h__

#include "nsCOMPtr.h"
#include "nsCoord.h"
#include "nsIFrame.h"
#include "nsISelectionListener.h"
#include "nsRefPtr.h"
#include "nsWeakReference.h"
#include "mozilla/EventForwards.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"

class nsFrameSelection;
class nsIContent;
class nsIPresShell;
struct nsPoint;

namespace mozilla {

namespace dom {
class Selection;
}

class AccessibleCaret;
class CopyPasteEventHub;

class CopyPasteManager
{
public:
  explicit CopyPasteManager(nsIPresShell* aPresShell);
  virtual ~CopyPasteManager();

  void UpdateCarets();
  void HideCarets();

protected:
  enum class CaretMode : uint8_t {
    None,
    Cursor,
    Selection
  };

  CaretMode GetCaretMode();

  virtual nsresult PressCaret(const nsPoint& aPoint);
  virtual nsresult DragCaret(const nsPoint& aPoint);
  virtual nsresult ReleaseCaret();
  virtual nsresult TapCaret(const nsPoint& aPoint);
  virtual nsresult SelectWordOrShortcut(const nsPoint& aPoint);
  virtual void OnScrollStart();
  virtual void OnScrollEnd();
  virtual void OnScrolling();
  virtual void OnReflow();
  virtual void OnBlur();
  virtual nsresult OnSelectionChanged(nsIDOMDocument* aDoc,
                                      nsISelection* aSel,
                                      int16_t aReason);
  virtual void OnKeyboardEvent();

  void UpdateCaretsForCursorMode();
  void UpdateCaretsForSelectionMode();

  virtual nsresult SelectWord(const nsPoint& aPoint);
  virtual void SetSelectionDragState(bool aState);
  virtual void SetSelectionDirection(nsDirection aDir);
  virtual nsIFrame* FindFirstNodeWithFrame(bool aBackward, int* aOutOffset);
  virtual nsresult DragCaretInternal(const nsPoint& aPoint);
  nsPoint AdjustDragBoundary(const nsPoint& aPoint);

  // Utility functions
  dom::Selection* GetSelection();
  already_AddRefed<nsFrameSelection> GetFrameSelection();
  nsIContent* GetFocusedContent();

  /*
   * If we're dragging start caret, we do not want to drag over previous
   * character of end caret. Same as end caret. So we check if content offset
   * exceed previous/next character of end/start caret base on aDragMode.
   */
  bool CompareRangeWithContentOffset(nsIFrame::ContentOffsets& aOffsets);

  nscoord mOffsetYToCaretLogicalPosition;
  nsIPresShell* mPresShell;
  UniquePtr<AccessibleCaret> mFirstCaret;
  UniquePtr<AccessibleCaret> mSecondCaret;

  // The caret being pressed or dragged.
  AccessibleCaret* mActiveCaret;

  static const int32_t kAutoScrollTimerDelay = 30;
  friend class CopyPasteEventHub;
};

} // namespace mozilla

#endif // CopyPasteManager_h__
