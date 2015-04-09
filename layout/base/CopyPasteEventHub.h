/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CopyPasteEventHub_h__
#define CopyPasteEventHub_h__

#include "mozilla/EventForwards.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsIReflowObserver.h"
#include "nsIScrollObserver.h"
#include "nsISelectionListener.h"
#include "nsPoint.h"
#include "nsRefPtr.h"
#include "nsWeakReference.h"

class nsDocShell;
class nsIPresShell;
class nsITimer;

namespace mozilla {
class CopyPasteManager;
class WidgetKeyboardEvent;
class WidgetMouseEvent;
class WidgetTouchEvent;
class WidgetWheelEvent;

class CopyPasteEventHub : public nsIReflowObserver,
                          public nsIScrollObserver,
                          public nsISelectionListener,
                          public nsSupportsWeakReference
{
public:
  explicit CopyPasteEventHub();
  virtual void Init(nsIPresShell* aPresShell);
  virtual void Terminate();

  nsEventStatus HandleEvent(WidgetEvent* aEvent);

  // Call this function to notify the blur event happened.
  void NotifyBlur(bool aIsLeavingDocument);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREFLOWOBSERVER
  NS_DECL_NSISELECTIONLISTENER

  // nsIScrollObserver
  virtual void ScrollPositionChanged() override;
  virtual void AsyncPanZoomStarted() override;
  virtual void AsyncPanZoomStopped() override;

  // Base state
  class State;
  State* GetState() const;

protected:
  virtual ~CopyPasteEventHub();

#define NS_DECL_STATE_CLASS_GETTER(aClassName)                                 \
  class aClassName;                                                            \
  static State* aClassName();

#define NS_IMPL_STATE_CLASS_GETTER(aClassName)                                 \
  CopyPasteEventHub::State* CopyPasteEventHub::aClassName()                    \
  {                                                                            \
    return CopyPasteEventHub::aClassName::Singleton();                         \
  }

  // Concrete state getters
  NS_DECL_STATE_CLASS_GETTER(NoActionState)
  NS_DECL_STATE_CLASS_GETTER(PressCaretState)
  NS_DECL_STATE_CLASS_GETTER(DragCaretState)
  NS_DECL_STATE_CLASS_GETTER(PressNoCaretState)
  NS_DECL_STATE_CLASS_GETTER(ScrollState)
  NS_DECL_STATE_CLASS_GETTER(PostScrollState)
  NS_DECL_STATE_CLASS_GETTER(LongTapState)

  void SetState(State* aState);

  nsEventStatus HandleMouseEvent(WidgetMouseEvent* aEvent);
  nsEventStatus HandleWheelEvent(WidgetWheelEvent* aEvent);
  nsEventStatus HandleTouchEvent(WidgetTouchEvent* aEvent);
  nsEventStatus HandleKeyboardEvent(WidgetKeyboardEvent* aEvent);

  virtual nsPoint GetTouchEventPosition(WidgetTouchEvent* aEvent,
                                        int32_t aIdentifier) const;
  virtual nsPoint GetMouseEventPosition(WidgetMouseEvent* aEvent) const;

  bool MoveDistanceIsLarge(const nsPoint& aPoint) const;

  void LaunchLongTapInjector();
  void CancelLongTapInjector();
  static void FireLongTap(nsITimer* aTimer, void* aCopyPasteEventHub);

  void LaunchScrollEndInjector();
  void CancelScrollEndInjector();
  static void FireScrollEnd(nsITimer* aTimer, void* aCopyPasteEventHub);

  // Member variables
  bool mInitialized = false;

  // True if async-pan-zoom should be used.
  bool mUseAsyncPanZoom = false;

  State* mState = NoActionState();

  nsIPresShell* mPresShell = nullptr;

  UniquePtr<CopyPasteManager> mHandler;

  WeakPtr<nsDocShell> mDocShell;

  // Use this timer for injecting a long tap event when APZ is disabled. If APZ
  // is enabled, it will send long tap event to us.
  nsCOMPtr<nsITimer> mLongTapInjectorTimer;

  // Use this timer for injecting a simulated scroll end.
  nsCOMPtr<nsITimer> mScrollEndInjectorTimer;

  // Last mouse button down event or touch start event point.
  nsPoint mPressPoint{NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE};

  // For filter multitouch event
  int32_t mActiveTouchId = kInvalidTouchId;

  static const int32_t kScrollEndTimerDelay = 300;
  static const int32_t kMoveStartToleranceInPixel = 5;
  static const int32_t kInvalidTouchId = -1;
  static const int32_t kDefaultTouchId = 0; // For mouse event
};

//
// Base class for all states
//
class CopyPasteEventHub::State
{
public:
#define NS_IMPL_STATE_UTILITIES(aClassName)                                    \
  virtual const char* Name() const { return #aClassName; }                     \
  static aClassName* Singleton()                                               \
  {                                                                            \
    static aClassName singleton;                                               \
    return &singleton;                                                         \
  }

  virtual const char* Name() const { return ""; }

  virtual nsEventStatus OnPress(CopyPasteEventHub* aContext,
                                const nsPoint& aPoint, int32_t aTouchId);
  virtual nsEventStatus OnMove(CopyPasteEventHub* aContext,
                               const nsPoint& aPoint);
  virtual nsEventStatus OnRelease(CopyPasteEventHub* aContext);
  virtual nsEventStatus OnLongTap(CopyPasteEventHub* aContext,
                                  const nsPoint& aPoint);
  virtual void OnScrollStart(CopyPasteEventHub* aContext);
  virtual void OnScrollEnd(CopyPasteEventHub* aContext);
  virtual void OnScrolling(CopyPasteEventHub* aContext);
  virtual void OnScrollPositionChanged(CopyPasteEventHub* aContext);
  virtual void OnBlur(CopyPasteEventHub* aContext, bool aIsLeavingDocument);
  virtual void OnSelectionChanged(CopyPasteEventHub* aContext,
                                  nsIDOMDocument* aDoc, nsISelection* aSel,
                                  int16_t aReason);
  virtual void OnReflow(CopyPasteEventHub* aContext);
  virtual void Enter(CopyPasteEventHub* aContext);
  virtual void Leave(CopyPasteEventHub* aContext);

protected:
  explicit State() = default;
  State(const State&) = delete;
  void operator=(const State&) = delete;
};

} // namespace mozilla

#endif //CopyPasteEventHub_h__
