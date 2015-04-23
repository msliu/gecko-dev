/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaretManager_h
#define AccessibleCaretManager_h

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
class AccessibleCaretEventHub;

// -----------------------------------------------------------------------------
// AccessibleCaretManager handles events and callbacks from
// AccessibleCaretEventHub, and perform the real work to manipulate selection
// object and AccessibleCaret.
//
class AccessibleCaretManager
{
public:
  explicit AccessibleCaretManager(nsIPresShell* aPresShell);
  virtual ~AccessibleCaretManager();

  virtual nsresult PressCaret(const nsPoint& aPoint);
  virtual nsresult DragCaret(const nsPoint& aPoint);
  virtual nsresult ReleaseCaret();
  virtual nsresult TapCaret(const nsPoint& aPoint);
  virtual nsresult SelectWordOrShortcut(const nsPoint& aPoint);
  virtual void OnScrollStart();
  virtual void OnScrollEnd();
  virtual void OnScrolling();
  virtual void OnScrollPositionChanged();
  virtual void OnReflow();
  virtual void OnBlur();
  virtual nsresult OnSelectionChanged(nsIDOMDocument* aDoc,
                                      nsISelection* aSel,
                                      int16_t aReason);
  virtual void OnKeyboardEvent();

protected:
  // This enum representing the number of AccessibleCarets on the screen.
  enum class CaretMode : uint8_t {
    // No caret on the screen.
    None,

    // One caret, i.e. the selection is collapsed.
    Cursor,

    // Two carets, i.e. the selection is not collapsed.
    Selection
  };
  CaretMode GetCaretMode() const;

  void UpdateCarets();
  void HideCarets();

  void UpdateCaretsForCursorMode();
  void UpdateCaretsForSelectionMode();
  void UpdateCaretsForTilt();

  bool ChangeFocus(nsIFrame* aFrame) const;
  nsresult SelectWord(nsIFrame* aFrame, const nsPoint& aPoint) const;
  void SetSelectionDragState(bool aState) const;
  void SetSelectionDirection(nsDirection aDir) const;

  // If aBackward is false, find the first node from the first range in current
  // selection, and return the frame and the offset into that frame. If aBackward
  // is true, find the last node from the last range instead.
  nsIFrame* FindFirstNodeWithFrame(bool aBackward, int32_t* aOutOffset) const;

  nsresult DragCaretInternal(const nsPoint& aPoint);
  nsPoint AdjustDragBoundary(const nsPoint& aPoint) const;
  void ClearMaintainedSelection() const;

  dom::Selection* GetSelection() const;
  already_AddRefed<nsFrameSelection> GetFrameSelection() const;
  nsIContent* GetFocusedContent() const;

  // If we're dragging the first caret, we do not want to drag it over the
  // previous character of the second caret. Same as the second caret. So we
  // check if content offset exceeds the previous/next character of second/first
  // caret base the active caret.
  bool CompareRangeWithContentOffset(nsIFrame::ContentOffsets& aOffsets);

  // Timeout in milliseconds to hide the AccessibleCaret under cursor mode while
  // no one touches it.
  uint32_t CaretTimeoutMs() const;
  void LaunchCaretTimeoutTimer();
  void CancelCaretTimeoutTimer();

  // Member variables
  nscoord mOffsetYToCaretLogicalPosition = NS_UNCONSTRAINEDSIZE;
  nsIPresShell* mPresShell = nullptr;

  // First caret is attached to nsCaret in cursor mode, and is attached to
  // selection highlight as the left caret in selection mode.
  UniquePtr<AccessibleCaret> mFirstCaret;

  // Second caret is used solely in selection mode, and is attached to selection
  // highlight as the right caret.
  UniquePtr<AccessibleCaret> mSecondCaret;

  // The caret being pressed or dragged.
  AccessibleCaret* mActiveCaret = nullptr;

  nsCOMPtr<nsITimer> mCaretTimeoutTimer;
  CaretMode mCaretMode = CaretMode::None;

  static const int32_t kAutoScrollTimerDelay = 30;
};

} // namespace mozilla

#endif // AccessibleCaretManager_h
