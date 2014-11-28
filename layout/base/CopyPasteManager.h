/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CopyPasteManager_h__
#define CopyPasteManager_h__

#include "nsISelectionListener.h"

class nsIPresShell;

namespace mozilla {
class AccessibleCaret;
class CopyPasteManagerGlue;

class CopyPasteManager : public nsISelectionListener
{
public:
  CopyPasteManager();
  virtual void Init(nsIPresShell* aPresShell);
  virtual nsEventStatus HandleEvent(WidgetEvent* aEvent);

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

  class GestureManager : public nsISupports {
  public:
    NS_DECL_ISUPPORTS
    GestureManager(nsIPresShell* aPresShell, CopyPasteManager* aHandler);
    nsEventStatus HandleEvent(WidgetEvent* aEvent);

  private:
    virtual ~GestureManager() {}
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
protected:
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

  void UpdateCarets();
  void HideCarets();
  nsEventStatus DragCaret(const nsPoint &aMovePoint);

  nsRefPtr<AccessibleCaret> mFirstCaret;
  nsRefPtr<AccessibleCaret> mSecondCaret;
  DragMode mDragMode;
  CaretMode mCaretMode;
  nscoord mCaretCenterToDownPointOffsetY;
  nsRefPtr<CopyPasteManagerGlue> mGlue;
  bool mHasInited;
  nsRefPtr<GestureManager> mGestureManager;
};

MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteManager::DragMode)
MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteManager::CaretMode)
MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteManager::GestureManager::InputState)
MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteManager::GestureManager::InputType)

} // namespace mozilla

#endif //CopyPasteManager_h__
