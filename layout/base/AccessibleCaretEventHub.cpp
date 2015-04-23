/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleCaretEventHub.h"

#include "AccessibleCaretLogger.h"
#include "AccessibleCaretManager.h"
#include "gfxPrefs.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "nsDocShell.h"
#include "nsFocusManager.h"
#include "nsFrameSelection.h"
#include "nsITimer.h"
#include "nsPresContext.h"

namespace mozilla {

#ifdef PR_LOGGING

#undef CP_LOG
#define CP_LOG(message, ...)                                                   \
  CP_LOG_BASE("AccessibleCaretEventHub (%p): " message, this, ##__VA_ARGS__);

#undef CP_LOGV
#define CP_LOGV(message, ...)                                                  \
  CP_LOGV_BASE("AccessibleCaretEventHub (%p): " message, this, ##__VA_ARGS__);

#endif // #ifdef PR_LOGGING

NS_IMPL_ISUPPORTS(AccessibleCaretEventHub, nsIReflowObserver, nsIScrollObserver,
                  nsISelectionListener, nsISupportsWeakReference);

// -----------------------------------------------------------------------------
// NoActionState
//
class AccessibleCaretEventHub::NoActionState : public AccessibleCaretEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(NoActionState)

  virtual nsEventStatus OnPress(AccessibleCaretEventHub* aContext,
                                const nsPoint& aPoint,
                                int32_t aTouchId) override;
  virtual void OnScrollStart(AccessibleCaretEventHub* aContext) override;
  virtual void OnScrolling(AccessibleCaretEventHub* aContext) override;
  virtual void OnScrollPositionChanged(AccessibleCaretEventHub* aContext) override;
  virtual void OnSelectionChanged(AccessibleCaretEventHub* aContext,
                                  nsIDOMDocument* aDoc, nsISelection* aSel,
                                  int16_t aReason) override;
  virtual void OnBlur(AccessibleCaretEventHub* aContext,
                      bool aIsLeavingDocument) override;
  virtual void OnReflow(AccessibleCaretEventHub* aContext) override;
  virtual void Enter(AccessibleCaretEventHub* aContext) override;
};

// -----------------------------------------------------------------------------
// PressCaretState
//
class AccessibleCaretEventHub::PressCaretState : public AccessibleCaretEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(PressCaretState)

  virtual nsEventStatus OnMove(AccessibleCaretEventHub* aContext,
                               const nsPoint& aPoint) override;
  virtual nsEventStatus OnRelease(AccessibleCaretEventHub* aContext) override;
  virtual nsEventStatus OnLongTap(AccessibleCaretEventHub* aContext,
                                  const nsPoint& aPoint) override;
};

// -----------------------------------------------------------------------------
// DragCaretState
//
class AccessibleCaretEventHub::DragCaretState : public AccessibleCaretEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(DragCaretState)

  virtual nsEventStatus OnMove(AccessibleCaretEventHub* aContext,
                               const nsPoint& aPoint) override;
  virtual nsEventStatus OnRelease(AccessibleCaretEventHub* aContext) override;
};

// -----------------------------------------------------------------------------
// PressNoCaretState
//
class AccessibleCaretEventHub::PressNoCaretState : public AccessibleCaretEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(PressNoCaretState)

  virtual nsEventStatus OnMove(AccessibleCaretEventHub* aContext,
                               const nsPoint& aPoint) override;
  virtual nsEventStatus OnRelease(AccessibleCaretEventHub* aContext) override;
  virtual nsEventStatus OnLongTap(AccessibleCaretEventHub* aContext,
                                  const nsPoint& aPoint) override;
  virtual void OnScrollStart(AccessibleCaretEventHub* aContext) override;
  virtual void OnBlur(AccessibleCaretEventHub* aContext,
                      bool aIsLeavingDocument) override;
  virtual void OnSelectionChanged(AccessibleCaretEventHub* aContext,
                                  nsIDOMDocument* aDoc, nsISelection* aSel,
                                  int16_t aReason) override;
  virtual void OnReflow(AccessibleCaretEventHub* aContext) override;
  virtual void Enter(AccessibleCaretEventHub* aContext) override;
  virtual void Leave(AccessibleCaretEventHub* aContext) override;
};

// -----------------------------------------------------------------------------
// ScrollState
//
class AccessibleCaretEventHub::ScrollState : public AccessibleCaretEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(ScrollState)

  virtual void OnScrollEnd(AccessibleCaretEventHub* aContext) override;
  virtual void OnBlur(AccessibleCaretEventHub* aContext,
                      bool aIsLeavingDocument) override;
};


// -----------------------------------------------------------------------------
// PostScrollState: In this state, we are waiting for another APZ start, press
// event, or momentum wheel scroll.
//
class AccessibleCaretEventHub::PostScrollState : public AccessibleCaretEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(PostScrollState)

  virtual nsEventStatus OnPress(AccessibleCaretEventHub* aContext,
                                const nsPoint& aPoint,
                                int32_t aTouchId) override;
  virtual void OnScrollStart(AccessibleCaretEventHub* aContext) override;
  virtual void OnScrollEnd(AccessibleCaretEventHub* aContext) override;
  virtual void OnScrolling(AccessibleCaretEventHub* aContext) override;
  virtual void OnBlur(AccessibleCaretEventHub* aContext,
                      bool aIsLeavingDocument) override;
  virtual void Enter(AccessibleCaretEventHub* aContext) override;
  virtual void Leave(AccessibleCaretEventHub* aContext) override;
};

// -----------------------------------------------------------------------------
// LongTapState
//
class AccessibleCaretEventHub::LongTapState : public AccessibleCaretEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(LongTapState)

  virtual nsEventStatus OnLongTap(AccessibleCaretEventHub* aContext,
                                  const nsPoint& aPoint) override;
  virtual void OnReflow(AccessibleCaretEventHub* aContext) override;
};

// -----------------------------------------------------------------------------
// Implementation of all concrete state functions
//
nsEventStatus
AccessibleCaretEventHub::State::OnPress(AccessibleCaretEventHub* aContext,
                                  const nsPoint& aPoint,
                                  int32_t aTouchId)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
AccessibleCaretEventHub::State::OnMove(AccessibleCaretEventHub* aContext, const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
AccessibleCaretEventHub::State::OnRelease(AccessibleCaretEventHub* aContext)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
AccessibleCaretEventHub::State::OnLongTap(AccessibleCaretEventHub* aContext,
                                    const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
}

void
AccessibleCaretEventHub::State::OnScrollStart(AccessibleCaretEventHub* aContext)
{
}

void
AccessibleCaretEventHub::State::OnScrollEnd(AccessibleCaretEventHub* aContext)
{
}

void
AccessibleCaretEventHub::State::OnScrolling(AccessibleCaretEventHub* aContext)
{
}

void
AccessibleCaretEventHub::State::OnScrollPositionChanged(AccessibleCaretEventHub* aContext)
{
}

void
AccessibleCaretEventHub::State::OnBlur(AccessibleCaretEventHub* aContext,
                                 bool aIsLeavingDocument)
{
}

void
AccessibleCaretEventHub::State::OnSelectionChanged(AccessibleCaretEventHub* aContext,
                                             nsIDOMDocument* aDoc,
                                             nsISelection* aSel,
                                             int16_t aReason)
{
}

void
AccessibleCaretEventHub::State::OnReflow(AccessibleCaretEventHub* aContext)
{
}

void
AccessibleCaretEventHub::State::Enter(AccessibleCaretEventHub* aContext)
{
}

void
AccessibleCaretEventHub::State::Leave(AccessibleCaretEventHub* aContext)
{
}

nsEventStatus
AccessibleCaretEventHub::NoActionState::OnPress(AccessibleCaretEventHub* aContext,
                                          const nsPoint& aPoint,
                                          int32_t aTouchId)
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  if (NS_SUCCEEDED(aContext->mHandler->PressCaret(aPoint))) {
    aContext->SetState(aContext->PressCaretState());
    rv = nsEventStatus_eConsumeNoDefault;
  } else {
    aContext->SetState(aContext->PressNoCaretState());
  }

  aContext->mPressPoint = aPoint;
  aContext->mActiveTouchId = aTouchId;

  return rv;
}

void
AccessibleCaretEventHub::NoActionState::OnScrollStart(AccessibleCaretEventHub* aContext)
{
  aContext->mHandler->OnScrollStart();
  aContext->SetState(aContext->ScrollState());
}

void
AccessibleCaretEventHub::NoActionState::OnScrolling(AccessibleCaretEventHub* aContext)
{
  aContext->mHandler->OnScrolling();
}

void
AccessibleCaretEventHub::NoActionState::OnScrollPositionChanged(
  AccessibleCaretEventHub* aContext)
{
  aContext->mHandler->OnScrollPositionChanged();
}

void
AccessibleCaretEventHub::NoActionState::OnBlur(AccessibleCaretEventHub* aContext,
                                         bool aIsLeavingDocument)
{
  aContext->mHandler->OnBlur();
}

void
AccessibleCaretEventHub::NoActionState::OnSelectionChanged(
  AccessibleCaretEventHub* aContext, nsIDOMDocument* aDoc, nsISelection* aSel,
  int16_t aReason)
{
  aContext->mHandler->OnSelectionChanged(aDoc, aSel, aReason);
}

void
AccessibleCaretEventHub::NoActionState::OnReflow(AccessibleCaretEventHub* aContext)
{
  aContext->mHandler->OnReflow();
}

void
AccessibleCaretEventHub::NoActionState::Enter(AccessibleCaretEventHub* aContext)
{
  aContext->mPressPoint = nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  aContext->mActiveTouchId = kInvalidTouchId;
}

nsEventStatus
AccessibleCaretEventHub::PressCaretState::OnMove(AccessibleCaretEventHub* aContext,
                                           const nsPoint& aPoint)
{
  if (aContext->MoveDistanceIsLarge(aPoint)) {
    if (NS_SUCCEEDED(aContext->mHandler->DragCaret(aPoint))) {
      aContext->SetState(aContext->DragCaretState());
    }
  }

  // We should always consume the event since we've pressed on the caret.
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
AccessibleCaretEventHub::PressCaretState::OnRelease(AccessibleCaretEventHub* aContext)
{
  aContext->mHandler->ReleaseCaret();
  aContext->mHandler->TapCaret(aContext->mPressPoint);
  aContext->SetState(aContext->NoActionState());

  // We should always consume the event since we've pressed on the caret.
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
AccessibleCaretEventHub::PressCaretState::OnLongTap(AccessibleCaretEventHub* aContext,
                                              const nsPoint& aPoint)
{
  // We should always consume the event since we've pressed on the caret.
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
AccessibleCaretEventHub::DragCaretState::OnMove(AccessibleCaretEventHub* aContext,
                                          const nsPoint& aPoint)
{
  aContext->mHandler->DragCaret(aPoint);

  // We should always consume the event since we've pressed on the caret.
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
AccessibleCaretEventHub::DragCaretState::OnRelease(AccessibleCaretEventHub* aContext)
{
  aContext->mHandler->ReleaseCaret();
  aContext->SetState(aContext->NoActionState());

  // We should always consume the event since we've pressed on the caret.
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
AccessibleCaretEventHub::PressNoCaretState::OnMove(AccessibleCaretEventHub* aContext,
                                             const nsPoint& aPoint)
{
  if (aContext->MoveDistanceIsLarge(aPoint)) {
    aContext->SetState(aContext->NoActionState());
  }

  return nsEventStatus_eIgnore;
}

nsEventStatus
AccessibleCaretEventHub::PressNoCaretState::OnRelease(AccessibleCaretEventHub* aContext)
{
  aContext->SetState(aContext->NoActionState());

  return nsEventStatus_eIgnore;
}

nsEventStatus
AccessibleCaretEventHub::PressNoCaretState::OnLongTap(AccessibleCaretEventHub* aContext,
                                                const nsPoint& aPoint)
{
  aContext->SetState(aContext->LongTapState());
  return aContext->GetState()->OnLongTap(aContext, aPoint);
}

void
AccessibleCaretEventHub::PressNoCaretState::OnScrollStart(AccessibleCaretEventHub* aContext)
{
  aContext->mHandler->OnScrollStart();
  aContext->SetState(aContext->ScrollState());
}

void
AccessibleCaretEventHub::PressNoCaretState::OnReflow(AccessibleCaretEventHub* aContext)
{
  aContext->mHandler->OnReflow();
}

void
AccessibleCaretEventHub::PressNoCaretState::OnBlur(AccessibleCaretEventHub* aContext,
                                             bool aIsLeavingDocument)
{
  aContext->mHandler->OnBlur();
  if (aIsLeavingDocument) {
    aContext->SetState(aContext->NoActionState());
  }
}

void
AccessibleCaretEventHub::PressNoCaretState::OnSelectionChanged(
  AccessibleCaretEventHub* aContext, nsIDOMDocument* aDoc, nsISelection* aSel,
  int16_t aReason)
{
  aContext->mHandler->OnSelectionChanged(aDoc, aSel, aReason);
}

void
AccessibleCaretEventHub::PressNoCaretState::Enter(AccessibleCaretEventHub* aContext)
{
  aContext->LaunchLongTapInjector();
}

void
AccessibleCaretEventHub::PressNoCaretState::Leave(AccessibleCaretEventHub* aContext)
{
  aContext->CancelLongTapInjector();
}

void
AccessibleCaretEventHub::ScrollState::OnScrollEnd(AccessibleCaretEventHub* aContext)
{
  aContext->SetState(aContext->PostScrollState());
}

void
AccessibleCaretEventHub::ScrollState::OnBlur(AccessibleCaretEventHub* aContext,
                                       bool aIsLeavingDocument)
{
  aContext->mHandler->OnBlur();
  if (aIsLeavingDocument) {
    aContext->SetState(aContext->NoActionState());
  }
}

nsEventStatus
AccessibleCaretEventHub::PostScrollState::OnPress(AccessibleCaretEventHub* aContext,
                                            const nsPoint& aPoint,
                                            int32_t aTouchId)
{
  aContext->mHandler->OnScrollEnd();
  aContext->SetState(aContext->NoActionState());
  return aContext->GetState()->OnPress(aContext, aPoint, aTouchId);
}

void
AccessibleCaretEventHub::PostScrollState::OnScrollStart(AccessibleCaretEventHub* aContext)
{
  aContext->SetState(aContext->ScrollState());
}

void
AccessibleCaretEventHub::PostScrollState::OnScrollEnd(AccessibleCaretEventHub* aContext)
{
  aContext->mHandler->OnScrollEnd();
  aContext->SetState(aContext->NoActionState());
}

void
AccessibleCaretEventHub::PostScrollState::OnScrolling(AccessibleCaretEventHub* aContext)
{
  // Momentum scroll by wheel event.
  aContext->LaunchScrollEndInjector();
}

void
AccessibleCaretEventHub::PostScrollState::OnBlur(AccessibleCaretEventHub* aContext,
                                           bool aIsLeavingDocument)
{
  aContext->mHandler->OnBlur();
  if (aIsLeavingDocument) {
    aContext->SetState(aContext->NoActionState());
  }
}

void
AccessibleCaretEventHub::PostScrollState::Enter(AccessibleCaretEventHub* aContext)
{
  // Launch the injector to leave PostScrollState.
  aContext->LaunchScrollEndInjector();
}

void
AccessibleCaretEventHub::PostScrollState::Leave(AccessibleCaretEventHub* aContext)
{
  aContext->CancelScrollEndInjector();
}

nsEventStatus
AccessibleCaretEventHub::LongTapState::OnLongTap(AccessibleCaretEventHub* aContext,
                                           const nsPoint& aPoint)
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  if (NS_SUCCEEDED(aContext->mHandler->SelectWordOrShortcut(aPoint))) {
    rv = nsEventStatus_eConsumeNoDefault;
  }

  aContext->SetState(aContext->NoActionState());

  return rv;
}

void
AccessibleCaretEventHub::LongTapState::OnReflow(AccessibleCaretEventHub* aContext)
{
  aContext->mHandler->OnReflow();
}

// -----------------------------------------------------------------------------
// Implementation of AccessibleCaretEventHub methods
//
AccessibleCaretEventHub::State*
AccessibleCaretEventHub::GetState() const
{
  return mState;
}

void
AccessibleCaretEventHub::SetState(State* aState)
{
  MOZ_ASSERT(aState);

  CP_LOG("%s -> %s", mState->Name(), aState->Name());

  mState->Leave(this);
  mState = aState;
  mState->Enter(this);
}

NS_IMPL_STATE_CLASS_GETTER(NoActionState)
NS_IMPL_STATE_CLASS_GETTER(PressCaretState)
NS_IMPL_STATE_CLASS_GETTER(DragCaretState)
NS_IMPL_STATE_CLASS_GETTER(PressNoCaretState)
NS_IMPL_STATE_CLASS_GETTER(ScrollState)
NS_IMPL_STATE_CLASS_GETTER(PostScrollState)
NS_IMPL_STATE_CLASS_GETTER(LongTapState)

AccessibleCaretEventHub::AccessibleCaretEventHub()
{
}

AccessibleCaretEventHub::~AccessibleCaretEventHub()
{
}

void
AccessibleCaretEventHub::Init(nsIPresShell* aPresShell)
{
  if (mInitialized || !aPresShell || !aPresShell->GetCanvasFrame() ||
      !aPresShell->GetCanvasFrame()->GetCustomContentContainer()) {
    return;
  }

  nsAutoScriptBlocker scriptBlocker;

  mPresShell = aPresShell;

  nsPresContext* presContext = mPresShell->GetPresContext();
  MOZ_ASSERT(presContext, "PresContext should be given in PresShell::Init()");

  nsIDocShell* docShell = presContext->GetDocShell();
  if (!docShell) {
    return;
  }

#if defined(MOZ_WIDGET_GONK)
  mUseAsyncPanZoom = gfxPrefs::AsyncPanZoomEnabled();
#endif

  docShell->AddWeakReflowObserver(this);
  docShell->AddWeakScrollObserver(this);

  mDocShell = static_cast<nsDocShell*>(docShell);

  mLongTapInjectorTimer = do_CreateInstance("@mozilla.org/timer;1");
  mScrollEndInjectorTimer = do_CreateInstance("@mozilla.org/timer;1");

  mHandler = MakeUnique<AccessibleCaretManager>(mPresShell);

  mInitialized = true;
}

void
AccessibleCaretEventHub::Terminate()
{
  if (!mInitialized) {
    return;
  }

  nsRefPtr<nsDocShell> docShell(mDocShell.get());
  if (docShell) {
    docShell->RemoveWeakReflowObserver(this);
    docShell->RemoveWeakScrollObserver(this);
  }

  if (mLongTapInjectorTimer) {
    mLongTapInjectorTimer->Cancel();
  }

  if (mScrollEndInjectorTimer) {
    mScrollEndInjectorTimer->Cancel();
  }

  mHandler = nullptr;
  mInitialized = false;
}

nsEventStatus
AccessibleCaretEventHub::HandleEvent(WidgetEvent* aEvent)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  if (!mInitialized) {
    return status;
  }

  switch (aEvent->mClass) {
  case eMouseEventClass:
    status = HandleMouseEvent(aEvent->AsMouseEvent());
    break;

  case eWheelEventClass:
    status = HandleWheelEvent(aEvent->AsWheelEvent());
    break;

  case eTouchEventClass:
    status = HandleTouchEvent(aEvent->AsTouchEvent());
    break;

  case eKeyboardEventClass:
    status = HandleKeyboardEvent(aEvent->AsKeyboardEvent());
    break;

  default:
    break;
  }

  return status;
}

nsEventStatus
AccessibleCaretEventHub::HandleMouseEvent(WidgetMouseEvent* aEvent)
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  if (aEvent->button != WidgetMouseEvent::eLeftButton) {
    return rv;
  }

  int32_t id = (mActiveTouchId == kInvalidTouchId ?
                kDefaultTouchId : mActiveTouchId);
  nsPoint point = GetMouseEventPosition(aEvent);

  switch (aEvent->message) {
  case NS_MOUSE_BUTTON_DOWN:
    CP_LOGV("Before NS_MOUSE_BUTTON_DOWN, state: %s", mState->Name());
    rv = mState->OnPress(this, point, id);
    CP_LOGV("After NS_MOUSE_BUTTON_DOWN, state: %s, consume: %d",
            mState->Name(), rv);
    break;

  case NS_MOUSE_MOVE:
    CP_LOGV("Before NS_MOUSE_MOVE, state: %s", mState->Name());
    rv = mState->OnMove(this, point);
    CP_LOGV("After NS_MOUSE_MOVE, state: %s, consume: %d", mState->Name(), rv);
    break;

  case NS_MOUSE_BUTTON_UP:
    CP_LOGV("Before NS_MOUSE_BUTTON_UP, state: %s", mState->Name());
    rv = mState->OnRelease(this);
    CP_LOGV("After NS_MOUSE_BUTTON_UP, state: %s, consume: %d", mState->Name(),
            rv);
    break;

  case NS_MOUSE_MOZLONGTAP:
    CP_LOGV("Before NS_MOUSE_MOZLONGTAP, state: %s", mState->Name());
    rv = mState->OnLongTap(this, point);
    CP_LOGV("After NS_MOUSE_MOZLONGTAP, state: %s, consume: %d", mState->Name(),
            rv);
    break;

  default:
    break;
  }

  return rv;
}

nsEventStatus
AccessibleCaretEventHub::HandleWheelEvent(WidgetWheelEvent* aEvent)
{
  switch (aEvent->message) {
  case NS_WHEEL_WHEEL:
    CP_LOGV("NS_WHEEL_WHEEL, isMomentum %d, state: %s", aEvent->isMomentum,
            mState->Name());
    mState->OnScrolling(this);
    break;

  case NS_WHEEL_START:
    CP_LOGV("NS_WHEEL_START, state: %s", mState->Name());
    mState->OnScrollStart(this);
    break;

  case NS_WHEEL_STOP:
    CP_LOGV("NS_WHEEL_STOP, state: %s", mState->Name());
    mState->OnScrollEnd(this);
    break;

  default:
    break;
  }

  // Always ignore this event since we only want to know scroll start and scroll
  // end, not to consume it.
  return nsEventStatus_eIgnore;
}

nsEventStatus
AccessibleCaretEventHub::HandleTouchEvent(WidgetTouchEvent* aEvent)
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  int32_t id = (mActiveTouchId == kInvalidTouchId ?
                aEvent->touches[0]->Identifier() : mActiveTouchId);
  nsPoint point = GetTouchEventPosition(aEvent, id);

  switch (aEvent->message) {
  case NS_TOUCH_START:
    CP_LOGV("Before NS_TOUCH_START, state: %s", mState->Name());
    rv = mState->OnPress(this, point, id);
    CP_LOGV("After NS_TOUCH_START, state: %s, consume: %d", mState->Name(), rv);
    break;

  case NS_TOUCH_MOVE:
    CP_LOGV("Before NS_TOUCH_MOVE, state: %s", mState->Name());
    rv = mState->OnMove(this, point);
    CP_LOGV("After NS_TOUCH_MOVE, state: %s, consume: %d", mState->Name(), rv);
    break;

  case NS_TOUCH_END:
    CP_LOGV("Before NS_TOUCH_END, state: %s", mState->Name());
    rv = mState->OnRelease(this);
    CP_LOGV("After NS_TOUCH_END, state: %s, consume: %d", mState->Name(), rv);
    break;

  case NS_TOUCH_CANCEL:
    CP_LOGV("Before NS_TOUCH_CANCEL, state: %s", mState->Name());
    rv = mState->OnRelease(this);
    CP_LOGV("After NS_TOUCH_CANCEL, state: %s, consume: %d", mState->Name(),
            rv);
    break;

  default:
    break;
  }

  return rv;
}

nsEventStatus
AccessibleCaretEventHub::HandleKeyboardEvent(WidgetKeyboardEvent* aEvent)
{
  switch (aEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_DOWN:
  case NS_KEY_PRESS:
    mHandler->OnKeyboardEvent();
    break;

  default:
    break;
  }

  return nsEventStatus_eIgnore;
}

bool
AccessibleCaretEventHub::MoveDistanceIsLarge(const nsPoint& aPoint) const
{
  nsPoint delta = aPoint - mPressPoint;
  return NS_hypot(delta.x, delta.y) >
         nsPresContext::AppUnitsPerCSSPixel() * kMoveStartToleranceInPixel;
}

void
AccessibleCaretEventHub::LaunchLongTapInjector()
{
  if (mUseAsyncPanZoom) {
    return;
  }

  if (!mLongTapInjectorTimer) {
    return;
  }

  int32_t longTapDelay = gfxPrefs::UiClickHoldContextMenusDelay();
  mLongTapInjectorTimer->InitWithFuncCallback(FireLongTap, this, longTapDelay,
                                              nsITimer::TYPE_ONE_SHOT);
}

void
AccessibleCaretEventHub::CancelLongTapInjector()
{
  if (mUseAsyncPanZoom) {
    return;
  }

  if (!mLongTapInjectorTimer) {
    return;
  }

  mLongTapInjectorTimer->Cancel();
}

/* static */ void
AccessibleCaretEventHub::FireLongTap(nsITimer* aTimer, void* aAccessibleCaretEventHub)
{
  AccessibleCaretEventHub* self = static_cast<AccessibleCaretEventHub*>(aAccessibleCaretEventHub);
  self->mState->OnLongTap(self, self->mPressPoint);
}

NS_IMETHODIMP
AccessibleCaretEventHub::Reflow(DOMHighResTimeStamp aStart,
                          DOMHighResTimeStamp aEnd)
{
  if (!mInitialized) {
    return NS_OK;
  }

  CP_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnReflow(this);
  return NS_OK;
}

NS_IMETHODIMP
AccessibleCaretEventHub::ReflowInterruptible(DOMHighResTimeStamp aStart,
                                       DOMHighResTimeStamp aEnd)
{
  if (!mInitialized) {
    return NS_OK;
  }

  return Reflow(aStart, aEnd);
}

void
AccessibleCaretEventHub::AsyncPanZoomStarted()
{
  if (!mInitialized) {
    return;
  }

  CP_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnScrollStart(this);
}

void
AccessibleCaretEventHub::AsyncPanZoomStopped()
{
  if (!mInitialized) {
    return;
  }

  CP_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnScrollEnd(this);
}

void
AccessibleCaretEventHub::ScrollPositionChanged()
{
  if (!mInitialized) {
    return;
  }

  CP_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnScrollPositionChanged(this);
}

void
AccessibleCaretEventHub::LaunchScrollEndInjector()
{
  if (!mScrollEndInjectorTimer) {
    return;
  }

  mScrollEndInjectorTimer->InitWithFuncCallback(
    FireScrollEnd, this, kScrollEndTimerDelay, nsITimer::TYPE_ONE_SHOT);
}

void
AccessibleCaretEventHub::CancelScrollEndInjector()
{
  if (!mScrollEndInjectorTimer) {
    return;
  }

  mScrollEndInjectorTimer->Cancel();
}

/* static */ void
AccessibleCaretEventHub::FireScrollEnd(nsITimer* aTimer, void* aAccessibleCaretEventHub)
{
  AccessibleCaretEventHub* self = static_cast<AccessibleCaretEventHub*>(aAccessibleCaretEventHub);
  self->mState->OnScrollEnd(self);
}

nsresult
AccessibleCaretEventHub::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                          nsISelection* aSel,
                                          int16_t aReason)
{
  if (!mInitialized) {
    return NS_OK;
  }

  CP_LOG("%s, state: %s, reason: %d", __FUNCTION__, mState->Name(), aReason);
  mState->OnSelectionChanged(this, aDoc, aSel, aReason);
  return NS_OK;
}

void
AccessibleCaretEventHub::NotifyBlur(bool aIsLeavingDocument)
{
  if (!mInitialized) {
    return;
  }

  CP_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnBlur(this, aIsLeavingDocument);
}

nsPoint
AccessibleCaretEventHub::GetTouchEventPosition(WidgetTouchEvent* aEvent,
                                         int32_t aIdentifier) const
{
  for (dom::Touch* touch : aEvent->touches) {
    if (touch->Identifier() == aIdentifier) {
      LayoutDeviceIntPoint touchIntPoint = touch->mRefPoint;

      // Get event coordinate relative to root frame.
      nsIFrame* rootFrame = mPresShell->GetRootFrame();
      return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, touchIntPoint,
                                                          rootFrame);
    }
  }
  return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

nsPoint
AccessibleCaretEventHub::GetMouseEventPosition(WidgetMouseEvent* aEvent) const
{
  LayoutDeviceIntPoint mouseIntPoint = aEvent->AsGUIEvent()->refPoint;

  // Get event coordinate relative to root frame.
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, mouseIntPoint,
                                                      rootFrame);
}

} // namespace mozilla
