/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CopyPasteEventHub_h__
#define CopyPasteEventHub_h__

#include "nsCOMPtr.h"
#include "nsDocShell.h"
#include "nsIReflowObserver.h"
#include "nsIScrollObserver.h"
#include "nsISelectionListener.h"
#include "nsPoint.h"
#include "nsRefPtr.h"
#include "nsWeakReference.h"
#include "mozilla/EventForwards.h"
#include "mozilla/WeakPtr.h"

class nsDocShell;
class nsIPresShell;
class nsITimer;

namespace mozilla {
class CopyPasteManager;

class CopyPasteEventHub : public nsIReflowObserver,
                          public nsIScrollObserver,
                          public nsSupportsWeakReference
{
public:
  CopyPasteEventHub(nsIPresShell* aPresShell, CopyPasteManager* aHandler);
  nsEventStatus HandleEvent(WidgetEvent* aEvent);
  virtual void Init();
  virtual void Terminate();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREFLOWOBSERVER

  // nsIScrollObserver
  virtual void ScrollPositionChanged() MOZ_OVERRIDE;
  virtual void AsyncPanZoomStarted(const CSSIntPoint aScrollPos) MOZ_OVERRIDE;
  virtual void AsyncPanZoomStopped(const CSSIntPoint aScrollPos) MOZ_OVERRIDE;

protected:
  virtual ~CopyPasteEventHub() {}
  MOZ_BEGIN_NESTED_ENUM_CLASS(InputState, uint8_t)
    PRESS,
    DRAG,
    RELEASE
  MOZ_END_NESTED_ENUM_CLASS(InputState)

  MOZ_BEGIN_NESTED_ENUM_CLASS(InputType, uint8_t)
    NONE,
    MOUSE,
    TOUCH
  MOZ_END_NESTED_ENUM_CLASS(InputType)

  static const char* ToStr(InputState aInputState);
  static const char* ToStr(InputType aInputType);

  nsEventStatus HandleMouseMoveEvent(WidgetMouseEvent* aEvent);
  nsEventStatus HandleMouseUpEvent(WidgetMouseEvent* aEvent);
  nsEventStatus HandleMouseDownEvent(WidgetMouseEvent* aEvent);
  nsEventStatus HandleLongTapEvent(WidgetMouseEvent* aEvent);
  nsEventStatus HandleTouchMoveEvent(WidgetTouchEvent* aEvent);
  nsEventStatus HandleTouchUpEvent(WidgetTouchEvent* aEvent);
  nsEventStatus HandleTouchDownEvent(WidgetTouchEvent* aEvent);
  void HandleScrollStart();
  void HandleScrollEnd();
  nsPoint GetTouchEventPosition(WidgetTouchEvent* aEvent, int32_t aIdentifier);
  nsPoint GetMouseEventPosition(WidgetMouseEvent* aEvent);
  void SetState(InputState aState);

  /**
   * Detecting long tap using timer
   */
  void LaunchLongTapDetector();
  void CancelLongTapDetector();
  static void FireLongTap(nsITimer* aTimer, void* aCopyPasteEventHub);

  void LaunchScrollEndDetector();
  static void FireScrollEnd(nsITimer* aTimer, void* aCopyPasteEventHub);

  // True if AsyncPanZoom is enabled
  bool mAsyncPanZoomEnabled;

  InputState mState;
  InputType mType;

  // For filter multitouch event
  int32_t mActiveTouchId;
  nsIPresShell* mPresShell;
  CopyPasteManager* mHandler;
  WeakPtr<nsDocShell> mDocShell;

  // This timer is used for detecting long tap fire. If content process
  // has APZC, we'll use APZC for long tap detecting. Otherwise, we use this
  // timer to detect long tap.
  nsCOMPtr<nsITimer> mLongTapDetectorTimer;

  // This timer is used for detecting scroll end. We don't have
  // scroll end event now, so we will fire this event with a
  // const time when we scroll. So when timer triggers, we treat it
  // as scroll end event.
  nsCOMPtr<nsITimer> mScrollEndDetectorTimer;

  // Last mouse button down event or touch start event point.
  nsPoint mLastPressEventPoint;

  static const int32_t kScrollEndTimerDelay = 300;
  static const int32_t kMoveStartTolerancePx = 5;
  static const int32_t kInvalidTouchId = -1;
};

MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteEventHub::InputState)
MOZ_FINISH_NESTED_ENUM_CLASS(CopyPasteEventHub::InputType)

} // namespace mozilla

#endif //CopyPasteEventHub_h__
