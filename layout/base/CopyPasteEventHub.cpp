/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteEventHub.h"

#include "CopyPasteLogger.h"
#include "CopyPasteManager.h"
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
  CP_LOG_BASE("CopyPasteEventHub (%p): " message, this, ##__VA_ARGS__);

#undef CP_LOGV
#define CP_LOGV(message, ...)                                                  \
  CP_LOGV_BASE("CopyPasteEventHub (%p): " message, this, ##__VA_ARGS__);

#endif // #ifdef PR_LOGGING

NS_IMPL_ISUPPORTS(CopyPasteEventHub, nsIReflowObserver, nsIScrollObserver,
                  nsISelectionListener, nsISupportsWeakReference);

//
// NoActionState
//
class CopyPasteEventHub::NoActionState : public CopyPasteEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(NoActionState)

  virtual nsEventStatus OnPress(CopyPasteEventHub* aContext,
                                const nsPoint& aPoint,
                                int32_t aTouchId) MOZ_OVERRIDE;
  virtual void OnScrollStart(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void OnScrolling(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void OnScrollPositionChanged(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void OnSelectionChanged(CopyPasteEventHub* aContext,
                                  nsIDOMDocument* aDoc, nsISelection* aSel,
                                  int16_t aReason) MOZ_OVERRIDE;
  virtual void OnBlur(CopyPasteEventHub* aContext,
                      bool aIsLeavingDocument) MOZ_OVERRIDE;
  virtual void OnReflow(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void Enter(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
};

//
// PressCaretState
//
class CopyPasteEventHub::PressCaretState : public CopyPasteEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(PressCaretState)

  virtual nsEventStatus OnMove(CopyPasteEventHub* aContext,
                               const nsPoint& aPoint) MOZ_OVERRIDE;
  virtual nsEventStatus OnRelease(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual nsEventStatus OnLongTap(CopyPasteEventHub* aContext,
                                  const nsPoint& aPoint) MOZ_OVERRIDE;
};

//
// DragCaretState
//
class CopyPasteEventHub::DragCaretState : public CopyPasteEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(DragCaretState)

  virtual nsEventStatus OnMove(CopyPasteEventHub* aContext,
                               const nsPoint& aPoint) MOZ_OVERRIDE;
  virtual nsEventStatus OnRelease(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
};

//
// PressNoCaretState
//
class CopyPasteEventHub::PressNoCaretState : public CopyPasteEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(PressNoCaretState)

  virtual nsEventStatus OnMove(CopyPasteEventHub* aContext,
                               const nsPoint& aPoint) MOZ_OVERRIDE;
  virtual nsEventStatus OnRelease(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual nsEventStatus OnLongTap(CopyPasteEventHub* aContext,
                                  const nsPoint& aPoint) MOZ_OVERRIDE;
  virtual void OnScrollStart(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void OnBlur(CopyPasteEventHub* aContext,
                      bool aIsLeavingDocument) MOZ_OVERRIDE;
  virtual void OnSelectionChanged(CopyPasteEventHub* aContext,
                                  nsIDOMDocument* aDoc, nsISelection* aSel,
                                  int16_t aReason) MOZ_OVERRIDE;
  virtual void OnReflow(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void Enter(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void Leave(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
};

//
// ScrollState
//
class CopyPasteEventHub::ScrollState : public CopyPasteEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(ScrollState)

  virtual void OnScrollEnd(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
};

//
// PostScrollState: In this state, we are waiting for another APZ start, press
// event, or momentum wheel scroll.
//
class CopyPasteEventHub::PostScrollState : public CopyPasteEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(PostScrollState)

  virtual nsEventStatus OnPress(CopyPasteEventHub* aContext,
                                const nsPoint& aPoint,
                                int32_t aTouchId) MOZ_OVERRIDE;
  virtual void OnScrollStart(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void OnScrollEnd(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void OnScrolling(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void Enter(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual void Leave(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
};

class CopyPasteEventHub::LongTapState : public CopyPasteEventHub::State
{
public:
  NS_IMPL_STATE_UTILITIES(LongTapState)

  virtual nsEventStatus OnLongTap(CopyPasteEventHub* aContext,
                                  const nsPoint& aPoint) MOZ_OVERRIDE;
  virtual void OnReflow(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
};

//
// Implementation of all state functions
//
nsEventStatus
CopyPasteEventHub::State::OnPress(CopyPasteEventHub* aContext,
                                  const nsPoint& aPoint,
                                  int32_t aTouchId)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::State::OnMove(CopyPasteEventHub* aContext, const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::State::OnRelease(CopyPasteEventHub* aContext)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::State::OnLongTap(CopyPasteEventHub* aContext,
                                    const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
}

void
CopyPasteEventHub::State::OnScrollStart(CopyPasteEventHub* aContext)
{
}

void
CopyPasteEventHub::State::OnScrollEnd(CopyPasteEventHub* aContext)
{
}

void
CopyPasteEventHub::State::OnScrolling(CopyPasteEventHub* aContext)
{
}

void
CopyPasteEventHub::State::OnScrollPositionChanged(CopyPasteEventHub* aContext)
{
}

void
CopyPasteEventHub::State::OnBlur(CopyPasteEventHub* aContext,
                                 bool aIsLeavingDocument)
{
}

void
CopyPasteEventHub::State::OnSelectionChanged(CopyPasteEventHub* aContext,
                                             nsIDOMDocument* aDoc,
                                             nsISelection* aSel,
                                             int16_t aReason)
{
}

void
CopyPasteEventHub::State::OnReflow(CopyPasteEventHub* aContext)
{
}

void
CopyPasteEventHub::State::Enter(CopyPasteEventHub* aContext)
{
}

void
CopyPasteEventHub::State::Leave(CopyPasteEventHub* aContext)
{
}

nsEventStatus
CopyPasteEventHub::NoActionState::OnPress(CopyPasteEventHub* aContext,
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
CopyPasteEventHub::NoActionState::OnScrollStart(CopyPasteEventHub* aContext)
{
  aContext->mHandler->OnScrollStart();
  aContext->SetState(aContext->ScrollState());
}

void
CopyPasteEventHub::NoActionState::OnScrolling(CopyPasteEventHub* aContext)
{
  aContext->mHandler->OnScrolling();
}

void
CopyPasteEventHub::NoActionState::OnScrollPositionChanged(
  CopyPasteEventHub* aContext)
{
  aContext->mHandler->OnScrollPositionChanged();
}

void
CopyPasteEventHub::NoActionState::OnBlur(CopyPasteEventHub* aContext,
                                         bool aIsLeavingDocument)
{
  aContext->mHandler->OnBlur();
}

void
CopyPasteEventHub::NoActionState::OnSelectionChanged(
  CopyPasteEventHub* aContext, nsIDOMDocument* aDoc, nsISelection* aSel,
  int16_t aReason)
{
  aContext->mHandler->OnSelectionChanged(aDoc, aSel, aReason);
}

void
CopyPasteEventHub::NoActionState::OnReflow(CopyPasteEventHub* aContext)
{
  aContext->mHandler->OnReflow();
}

void
CopyPasteEventHub::NoActionState::Enter(CopyPasteEventHub* aContext)
{
  aContext->mPressPoint = nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  aContext->mActiveTouchId = kInvalidTouchId;
}

nsEventStatus
CopyPasteEventHub::PressCaretState::OnMove(CopyPasteEventHub* aContext,
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
CopyPasteEventHub::PressCaretState::OnRelease(CopyPasteEventHub* aContext)
{
  aContext->mHandler->ReleaseCaret();
  aContext->mHandler->TapCaret(aContext->mPressPoint);
  aContext->SetState(aContext->NoActionState());

  // We should always consume the event since we've pressed on the caret.
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
CopyPasteEventHub::PressCaretState::OnLongTap(CopyPasteEventHub* aContext,
                                              const nsPoint& aPoint)
{
  // We should always consume the event since we've pressed on the caret.
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
CopyPasteEventHub::DragCaretState::OnMove(CopyPasteEventHub* aContext,
                                          const nsPoint& aPoint)
{
  aContext->mHandler->DragCaret(aPoint);

  // We should always consume the event since we've pressed on the caret.
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
CopyPasteEventHub::DragCaretState::OnRelease(CopyPasteEventHub* aContext)
{
  aContext->mHandler->ReleaseCaret();
  aContext->SetState(aContext->NoActionState());

  // We should always consume the event since we've pressed on the caret.
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
CopyPasteEventHub::PressNoCaretState::OnMove(CopyPasteEventHub* aContext,
                                             const nsPoint& aPoint)
{
  if (aContext->MoveDistanceIsLarge(aPoint)) {
    aContext->SetState(aContext->NoActionState());
  }

  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::PressNoCaretState::OnRelease(CopyPasteEventHub* aContext)
{
  aContext->SetState(aContext->NoActionState());

  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::PressNoCaretState::OnLongTap(CopyPasteEventHub* aContext,
                                                const nsPoint& aPoint)
{
  aContext->SetState(aContext->LongTapState());
  return aContext->GetState()->OnLongTap(aContext, aPoint);
}

void
CopyPasteEventHub::PressNoCaretState::OnScrollStart(CopyPasteEventHub* aContext)
{
  aContext->mHandler->OnScrollStart();
  aContext->SetState(aContext->ScrollState());
}

void
CopyPasteEventHub::PressNoCaretState::OnReflow(CopyPasteEventHub* aContext)
{
  aContext->mHandler->OnReflow();
}

void
CopyPasteEventHub::PressNoCaretState::OnBlur(CopyPasteEventHub* aContext,
                                             bool aIsLeavingDocument)
{
  aContext->mHandler->OnBlur();
  if (aIsLeavingDocument) {
    aContext->SetState(aContext->NoActionState());
  }
}

void
CopyPasteEventHub::PressNoCaretState::OnSelectionChanged(
  CopyPasteEventHub* aContext, nsIDOMDocument* aDoc, nsISelection* aSel,
  int16_t aReason)
{
  aContext->mHandler->OnSelectionChanged(aDoc, aSel, aReason);
}

void
CopyPasteEventHub::PressNoCaretState::Enter(CopyPasteEventHub* aContext)
{
  aContext->LaunchLongTapInjector();
}

void
CopyPasteEventHub::PressNoCaretState::Leave(CopyPasteEventHub* aContext)
{
  aContext->CancelLongTapInjector();
}

void
CopyPasteEventHub::ScrollState::OnScrollEnd(CopyPasteEventHub* aContext)
{
  aContext->SetState(aContext->PostScrollState());
}

nsEventStatus
CopyPasteEventHub::PostScrollState::OnPress(CopyPasteEventHub* aContext,
                                            const nsPoint& aPoint,
                                            int32_t aTouchId)
{
  aContext->mHandler->OnScrollEnd();
  aContext->SetState(aContext->NoActionState());
  return aContext->GetState()->OnPress(aContext, aPoint, aTouchId);
}

void
CopyPasteEventHub::PostScrollState::OnScrollStart(CopyPasteEventHub* aContext)
{
  aContext->SetState(aContext->ScrollState());
}

void
CopyPasteEventHub::PostScrollState::OnScrollEnd(CopyPasteEventHub* aContext)
{
  aContext->mHandler->OnScrollEnd();
  aContext->SetState(aContext->NoActionState());
}

void
CopyPasteEventHub::PostScrollState::OnScrolling(CopyPasteEventHub* aContext)
{
  // Momentum scroll by wheel event.
  aContext->LaunchScrollEndInjector();
}

void
CopyPasteEventHub::PostScrollState::Enter(CopyPasteEventHub* aContext)
{
  // Launch the injector to leave PostScrollState.
  aContext->LaunchScrollEndInjector();
}

void
CopyPasteEventHub::PostScrollState::Leave(CopyPasteEventHub* aContext)
{
  aContext->CancelScrollEndInjector();
}

nsEventStatus
CopyPasteEventHub::LongTapState::OnLongTap(CopyPasteEventHub* aContext,
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
CopyPasteEventHub::LongTapState::OnReflow(CopyPasteEventHub* aContext)
{
  aContext->mHandler->OnReflow();
}

//
// Implementation of CopyPasteEventHub
//
CopyPasteEventHub::State*
CopyPasteEventHub::GetState()
{
  return mState;
}

void
CopyPasteEventHub::SetState(State* aState)
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

CopyPasteEventHub::CopyPasteEventHub()
  : mInitialized(false)
  , mAsyncPanZoomEnabled(false)
  , mState(NoActionState())
  , mPresShell(nullptr)
  , mPressPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE)
  , mActiveTouchId(kInvalidTouchId)
{
}

CopyPasteEventHub::~CopyPasteEventHub()
{
}

void
CopyPasteEventHub::Init(nsIPresShell* aPresShell)
{
  if (mInitialized || !aPresShell || !aPresShell->GetCanvasFrame()) {
    return;
  }

  mPresShell = aPresShell;

  nsPresContext* presContext = mPresShell->GetPresContext();
  MOZ_ASSERT(presContext, "PresContext should be given in PresShell::Init()");

  nsIDocShell* docShell = presContext->GetDocShell();
  if (!docShell) {
    return;
  }

  docShell->GetAsyncPanZoomEnabled(&mAsyncPanZoomEnabled);
  mAsyncPanZoomEnabled = mAsyncPanZoomEnabled && gfxPrefs::AsyncPanZoomEnabled();

  docShell->AddWeakReflowObserver(this);
  docShell->AddWeakScrollObserver(this);

  mDocShell = static_cast<nsDocShell*>(docShell);

  mLongTapInjectorTimer = do_CreateInstance("@mozilla.org/timer;1");
  mScrollEndInjectorTimer = do_CreateInstance("@mozilla.org/timer;1");

  mHandler = MakeUnique<CopyPasteManager>(mPresShell);

  mInitialized = true;
}

void
CopyPasteEventHub::Terminate()
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
CopyPasteEventHub::HandleEvent(WidgetEvent* aEvent)
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
CopyPasteEventHub::HandleMouseEvent(WidgetMouseEvent* aEvent)
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
CopyPasteEventHub::HandleWheelEvent(WidgetWheelEvent* aEvent)
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
CopyPasteEventHub::HandleTouchEvent(WidgetTouchEvent* aEvent)
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
CopyPasteEventHub::HandleKeyboardEvent(WidgetKeyboardEvent* aEvent)
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
CopyPasteEventHub::MoveDistanceIsLarge(const nsPoint& aPoint)
{
  nsPoint delta = aPoint - mPressPoint;
  return NS_hypot(delta.x, delta.y) >
         nsPresContext::AppUnitsPerCSSPixel() * kMoveStartToleranceInPixel;
}

void
CopyPasteEventHub::LaunchLongTapInjector()
{
  if (mAsyncPanZoomEnabled) {
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
CopyPasteEventHub::CancelLongTapInjector()
{
  if (mAsyncPanZoomEnabled) {
    return;
  }

  if (!mLongTapInjectorTimer) {
    return;
  }

  mLongTapInjectorTimer->Cancel();
}

/* static */ void
CopyPasteEventHub::FireLongTap(nsITimer* aTimer, void* aCopyPasteEventHub)
{
  CopyPasteEventHub* self = static_cast<CopyPasteEventHub*>(aCopyPasteEventHub);
  self->mState->OnLongTap(self, self->mPressPoint);
}

NS_IMETHODIMP
CopyPasteEventHub::Reflow(DOMHighResTimeStamp aStart,
                          DOMHighResTimeStamp aEnd)
{
  CP_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnReflow(this);
  return NS_OK;
}

NS_IMETHODIMP
CopyPasteEventHub::ReflowInterruptible(DOMHighResTimeStamp aStart,
                                       DOMHighResTimeStamp aEnd)
{
  return Reflow(aStart, aEnd);
}

void
CopyPasteEventHub::AsyncPanZoomStarted(const CSSIntPoint aScrollPos)
{
  CP_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnScrollStart(this);
}

void
CopyPasteEventHub::AsyncPanZoomStopped(const CSSIntPoint aScrollPos)
{
  CP_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnScrollEnd(this);
}

void
CopyPasteEventHub::ScrollPositionChanged()
{
  CP_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnScrollPositionChanged(this);
}

void
CopyPasteEventHub::LaunchScrollEndInjector()
{
  if (!mScrollEndInjectorTimer) {
    return;
  }

  mScrollEndInjectorTimer->InitWithFuncCallback(
    FireScrollEnd, this, kScrollEndTimerDelay, nsITimer::TYPE_ONE_SHOT);
}

void
CopyPasteEventHub::CancelScrollEndInjector()
{
  if (!mScrollEndInjectorTimer) {
    return;
  }

  mScrollEndInjectorTimer->Cancel();
}

/* static */ void
CopyPasteEventHub::FireScrollEnd(nsITimer* aTimer, void* aCopyPasteEventHub)
{
  CopyPasteEventHub* self = static_cast<CopyPasteEventHub*>(aCopyPasteEventHub);
  self->mState->OnScrollEnd(self);
}

nsresult
CopyPasteEventHub::NotifySelectionChanged(nsIDOMDocument* aDoc,
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
CopyPasteEventHub::NotifyBlur(bool aIsLeavingDocument)
{
  if (!mInitialized) {
    return;
  }

  CP_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnBlur(this, aIsLeavingDocument);
}

nsPoint
CopyPasteEventHub::GetTouchEventPosition(WidgetTouchEvent* aEvent,
                                         int32_t aIdentifier)
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
CopyPasteEventHub::GetMouseEventPosition(WidgetMouseEvent* aEvent)
{
  LayoutDeviceIntPoint mouseIntPoint = aEvent->AsGUIEvent()->refPoint;

  // Get event coordinate relative to root frame.
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, mouseIntPoint,
                                                      rootFrame);
}

} // namespace mozilla
