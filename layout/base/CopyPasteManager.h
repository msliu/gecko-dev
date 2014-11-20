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
class AccessibleCaret;

class CopyPasteManager MOZ_FINAL : public nsISelectionListener
{
public:
  explicit CopyPasteManager(nsIPresShell* aPresShell);
  nsEventStatus HandleEvent(WidgetEvent* aEvent);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONLISTENER

  /**
   * Indicate which part of caret we are dragging at.
   */
  MOZ_BEGIN_NESTED_ENUM_CLASS(DragMode, uint8_t)
    NONE,
    FIRST_CARET,
    SECOND_CARET
  MOZ_END_ENUM_CLASS(DragMode)

private:
  MOZ_BEGIN_NESTED_ENUM_CLASS(CaretMode, uint8_t)
    NONE,
    CURSOR,
    SELECTION
  MOZ_END_ENUM_CLASS(CaretMode);

  ~CopyPasteManager();

  // Utility function
  dom::Selection* GetSelection();
  already_AddRefed<nsFrameSelection> GetFrameSelection();
  nsIContent* GetFocusedContent();

  // Input event handler
  nsEventStatus OnPress(const nsPoint& aPoint);
  nsEventStatus OnDrag(const nsPoint& aPoint);
  nsEventStatus OnRelease();
  nsEventStatus OnLongTap(const nsPoint& aPoint);
  nsEventStatus OnTap(const nsPoint& aPoint);

  void UpdateCarets();
  nsresult SelectWord(const nsPoint& aPoint);
  void SetSelectionDragState(bool aState);
  void SetSelectionDirection(bool aForward);
  void HideCarets();
  nsEventStatus DragCaret(const nsPoint &aMovePoint);

  static nsIFrame* FindFirstNodeWithFrame(nsIDocument* aDocument,
                                          nsRange* aRange,
                                          nsFrameSelection* aFrameSelection,
                                          bool aBackward,
                                          int& aOutOffset);

  nsIPresShell* mPresShell;
  nsRefPtr<AccessibleCaret> mFirstCaret;
  nsRefPtr<AccessibleCaret> mSecondCaret;
  DragMode mDragMode;
  CaretMode mCaretMode;
  nscoord mCaretCenterToDownPointOffsetY;

  class GestureManager {
  public:
    GestureManager(nsIPresShell* aPresShell, CopyPasteManager* aManager);
    nsEventStatus HandleEvent(WidgetEvent* aEvent);

  private:
    MOZ_BEGIN_NESTED_ENUM_CLASS(InputState, uint8_t)
      PRESS,
      DRAG,
      RELEASE
    MOZ_END_ENUM_CLASS(InputState)

    MOZ_BEGIN_NESTED_ENUM_CLASS(InputType, uint8_t)
      NONE,
      MOUSE,
      TOUCH
    MOZ_END_ENUM_CLASS(InputType)

    nsEventStatus HandleMouseMoveEvent(WidgetMouseEvent* aEvent);
    nsEventStatus HandleMouseUpEvent(WidgetMouseEvent* aEvent);
    nsEventStatus HandleMouseDownEvent(WidgetMouseEvent* aEvent);
    nsEventStatus HandleLongTapEvent(WidgetMouseEvent* aEvent);
    nsEventStatus HandleTouchMoveEvent(WidgetTouchEvent* aEvent);
    nsEventStatus HandleTouchUpEvent(WidgetTouchEvent* aEvent);
    nsEventStatus HandleTouchDownEvent(WidgetTouchEvent* aEvent);
    nsPoint GetEventPosition(WidgetTouchEvent* aEvent, int32_t aIdentifier);
    nsPoint GetEventPosition(WidgetMouseEvent* aEvent);
    void SetState(InputState aState);

    /**
     * Detecting long tap using timer
     */
    void LaunchLongTapDetector();
    void CancelLongTapDetector();
    static void FireLongTap(nsITimer* aTimer, void* aSelectionCarets);

    InputState mState;
    InputType mType;
    // For filter multitouch event
    int32_t mActiveTouchId;
    nsIPresShell* mPresShell;
    CopyPasteManager* mHandler;
    nsPoint mDownPoint;
    // This timer is used for detecting long tap fire. If content process
    // has APZC, we'll use APZC for long tap detecting. Otherwise, we use this
    // timer to detect long tap.
    nsCOMPtr<nsITimer> mLongTapDetectorTimer;
    // True if AsyncPanZoom is enabled
    bool mAsyncPanZoomEnabled;
  };

  GestureManager mGestureManager;
};

MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteManager::DragMode)
MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteManager::CaretMode)
MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteManager::GestureManager::InputState)
MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteManager::GestureManager::InputType)

} // namespace mozilla

#endif //CopyPasteManager_h__
