/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CopyPasteEventHub_h__
#define CopyPasteEventHub_h__

#include "nsCOMPtr.h"
#include "nsIReflowObserver.h"
#include "nsIScrollObserver.h"
#include "nsISelectionListener.h"
#include "nsPoint.h"
#include "nsRefPtr.h"
#include "nsWeakReference.h"
#include "mozilla/EventForwards.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/UniquePtr.h"

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
  virtual void Init();
  virtual void Terminate();

  nsEventStatus HandleEvent(WidgetEvent* aEvent);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREFLOWOBSERVER

  // nsIScrollObserver
  virtual void ScrollPositionChanged() MOZ_OVERRIDE;
  virtual void AsyncPanZoomStarted(const CSSIntPoint aScrollPos) MOZ_OVERRIDE;
  virtual void AsyncPanZoomStopped(const CSSIntPoint aScrollPos) MOZ_OVERRIDE;

protected:
  virtual ~CopyPasteEventHub();

  // Base state and concrete states
  class State;
  class NoActionState;
  class PressState;
  class DragState;
  class WaitLongTapState;
  class ScrollState;

  void SetState(State* aState);

  nsEventStatus HandleMouseEvent(WidgetMouseEvent* aEvent);
  nsEventStatus HandleTouchEvent(WidgetTouchEvent* aEvent);

  nsPoint GetTouchEventPosition(WidgetTouchEvent* aEvent, int32_t aIdentifier);
  nsPoint GetMouseEventPosition(WidgetMouseEvent* aEvent);

  bool MoveDistanceIsLarge(const nsPoint& aPoint);

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

  State* mState;

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
  nsPoint mPressPoint;

  // For filter multitouch event
  int32_t mActiveTouchId;

  static const int32_t kScrollEndTimerDelay = 300;
  static const int32_t kMoveStartToleranceInPixel = 5;
  static const int32_t kInvalidTouchId = -1;
  static const int32_t kDefaultTouchId = 0; // For mouse event
};

} // namespace mozilla

#endif //CopyPasteEventHub_h__
