/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteEventHub.h"

#include "CopyPasteManager.h"
#include "gfxPrefs.h"
#include "mozilla/TouchEvents.h"
#include "nsDocShell.h"
#include "nsFocusManager.h"
#include "nsFrameSelection.h"
#include "nsITimer.h"
#include "nsPresContext.h"
#include "prlog.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(CopyPasteEventHub, nsIReflowObserver, nsIScrollObserver,
                  nsISelectionListener, nsISupportsWeakReference);

// Avoid redefine macros
#undef LOG

#ifdef PR_LOGGING
static PRLogModuleInfo* gCopyPasteEventHubLogModule;
static const char* kCopyPasteEventHubModuleName = "CopyPasteEventHub";
#define LOG(level, message, ...)                                               \
  PR_LOG(gCopyPasteEventHubLogModule, level,                                   \
         ("%s (%p): %s:%d : " message "\n", kCopyPasteEventHubModuleName,      \
          this, __FUNCTION__, __LINE__, ##__VA_ARGS__));
#define LOG_ALWAYS(...) LOG(PR_LOG_ALWAYS, ##__VA_ARGS__)
#define LOG_ERROR(...) LOG(PR_LOG_ERROR, ##__VA_ARGS__)
#define LOG_WARNING(...) LOG(PR_LOG_WARNING, ##__VA_ARGS__)
#define LOG_DEBUG(...) LOG(PR_LOG_DEBUG, ##__VA_ARGS__)
#define LOG_DEBUG_VERBOSE(...) LOG(PR_LOG_DEBUG + 1, ##__VA_ARGS__)

#else
#define LOG(level, message, ...)
#define LOG_ALWAYS(...)
#define LOG_ERROR(...)
#define LOG_WARNING(...)
#define LOG_DEBUG(...)
#define LOG_DEBUG_VERBOSE(...)
#endif // #ifdef PR_LOGGING

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

  virtual nsEventStatus OnRelease(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
  virtual nsEventStatus OnLongTap(CopyPasteEventHub* aContext,
                                  const nsPoint& aPoint) MOZ_OVERRIDE;
  virtual void OnScrollStart(CopyPasteEventHub* aContext) MOZ_OVERRIDE;
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
CopyPasteEventHub::State::OnBlur(CopyPasteEventHub* aContext)
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
CopyPasteEventHub::PressNoCaretState::OnRelease(CopyPasteEventHub* aContext)
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  aContext->SetState(aContext->NoActionState());

  return rv;
}

nsEventStatus
CopyPasteEventHub::PressNoCaretState::OnLongTap(CopyPasteEventHub* aContext,
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
CopyPasteEventHub::PressNoCaretState::OnScrollStart(CopyPasteEventHub* aContext)
{
  aContext->mHandler->OnScrollStart();
  aContext->SetState(aContext->ScrollState());
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
  LOG_DEBUG("%s -> %s", mState ? mState->Name() : "nullptr",
            aState ? aState->Name() : "nullptr");

  if (mState) {
    mState->Leave(this);
  }

  mState = aState;

  if (mState) {
    mState->Enter(this);
  }
}

NS_IMPL_STATE_CLASS_GETTER(NoActionState)
NS_IMPL_STATE_CLASS_GETTER(PressCaretState)
NS_IMPL_STATE_CLASS_GETTER(DragCaretState)
NS_IMPL_STATE_CLASS_GETTER(PressNoCaretState)
NS_IMPL_STATE_CLASS_GETTER(ScrollState)
NS_IMPL_STATE_CLASS_GETTER(PostScrollState)

CopyPasteEventHub::CopyPasteEventHub()
  : mInitialized(false)
  , mAsyncPanZoomEnabled(false)
  , mState(nullptr)
  , mPresShell(nullptr)
  , mPressPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE)
  , mActiveTouchId(kInvalidTouchId)
{
#ifdef PR_LOGGING
  if (!gCopyPasteEventHubLogModule) {
    gCopyPasteEventHubLogModule = PR_NewLogModule(kCopyPasteEventHubModuleName);
  }
#endif

  SetState(NoActionState());
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

  default:
    LOG_DEBUG_VERBOSE("Unhandled event message: %d", aEvent->message);
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
    LOG_DEBUG_VERBOSE("Before NS_MOUSE_BUTTON_DOWN, state: %s", mState->Name());
    rv = mState->OnPress(this, point, id);
    LOG_DEBUG_VERBOSE("After NS_MOUSE_BUTTON_DOWN, state: %s, consume: %d",
                      mState->Name(), rv);
    break;

  case NS_MOUSE_MOVE:
    LOG_DEBUG_VERBOSE("Before NS_MOUSE_MOVE, state: %s", mState->Name());
    rv = mState->OnMove(this, point);
    LOG_DEBUG_VERBOSE("After NS_MOUSE_MOVE, state: %s, consume: %d",
                      mState->Name(), rv);
    break;

  case NS_MOUSE_BUTTON_UP:
    LOG_DEBUG_VERBOSE("Before NS_MOUSE_BUTTON_UP, state: %s", mState->Name());
    rv = mState->OnRelease(this);
    LOG_DEBUG_VERBOSE("After NS_MOUSE_BUTTON_UP, state: %s, consume: %d",
                      mState->Name(), rv);
    break;

  case NS_MOUSE_MOZLONGTAP:
    LOG_DEBUG_VERBOSE("Before NS_MOUSE_MOZLONGTAP, state: %s", mState->Name());
    rv = mState->OnLongTap(this, point);
    LOG_DEBUG_VERBOSE("After NS_MOUSE_MOZLONGTAP, state: %s, consume: %d",
                      mState->Name(), rv);
    break;

  default:
    LOG_DEBUG_VERBOSE("Unhandled mouse event message: %d", aEvent->message);
    break;
  }

  return rv;
}

nsEventStatus
CopyPasteEventHub::HandleWheelEvent(WidgetWheelEvent* aEvent)
{
  switch (aEvent->message) {
  case NS_WHEEL_WHEEL:
    LOG_DEBUG_VERBOSE("NS_WHEEL_WHEEL, isMomentum %d, state: %s",
                      aEvent->isMomentum, mState->Name());
    mState->OnScrolling(this);
    break;

  case NS_WHEEL_START:
    LOG_DEBUG_VERBOSE("NS_WHEEL_START, state: %s", mState->Name());
    mState->OnScrollStart(this);
    break;

  case NS_WHEEL_STOP:
    LOG_DEBUG_VERBOSE("NS_WHEEL_STOP, state: %s", mState->Name());
    mState->OnScrollEnd(this);
    break;

  default:
    LOG_DEBUG_VERBOSE("Unhandled wheel event message: %d", aEvent->message);
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
    LOG_DEBUG_VERBOSE("Before NS_TOUCH_START, state: %s", mState->Name());
    rv = mState->OnPress(this, point, id);
    LOG_DEBUG_VERBOSE("After NS_TOUCH_START, state: %s, consume: %d",
                      mState->Name(), rv);
    break;

  case NS_TOUCH_MOVE:
    LOG_DEBUG_VERBOSE("Before NS_TOUCH_MOVE, state: %s", mState->Name());
    rv = mState->OnMove(this, point);
    LOG_DEBUG_VERBOSE("After NS_TOUCH_MOVE, state: %s, consume: %d",
                      mState->Name(), rv);
    break;

  case NS_TOUCH_END:
    LOG_DEBUG_VERBOSE("Before NS_TOUCH_END, state: %s", mState->Name());
    rv = mState->OnRelease(this);
    LOG_DEBUG_VERBOSE("After NS_TOUCH_END, state: %s, consume: %d",
                      mState->Name(), rv);
    break;

  case NS_TOUCH_CANCEL:
    LOG_DEBUG_VERBOSE("Before NS_TOUCH_CANCEL, state: %s", mState->Name());
    rv = mState->OnRelease(this);
    LOG_DEBUG_VERBOSE("After NS_TOUCH_CANCEL, state: %s, consume: %d",
                      mState->Name(), rv);
    break;

  default:
    LOG_DEBUG_VERBOSE("Unhandled touch event message: %d", aEvent->message);
    break;
  }

  return rv;
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
  LOG_DEBUG("state: %s", mState->Name());
  mHandler->OnReflow();
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
  LOG_DEBUG("state: %s", mState->Name());
  mState->OnScrollStart(this);
}

void
CopyPasteEventHub::AsyncPanZoomStopped(const CSSIntPoint aScrollPos)
{
  LOG_DEBUG("state: %s", mState->Name());
  mState->OnScrollEnd(this);
}

void
CopyPasteEventHub::ScrollPositionChanged()
{
  LOG_DEBUG("state: %s", mState->Name());

  // XXX: Do we receive a standalone ScrollPositionChanged() without
  // AsyncPanZoomStarted()?
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

  LOG_DEBUG("state: %s, reason: %d", mState->Name(), aReason);
  return mHandler->OnSelectionChanged(aDoc, aSel, aReason);
}

nsPoint
CopyPasteEventHub::GetTouchEventPosition(WidgetTouchEvent* aEvent,
                                         int32_t aIdentifier)
{
  for (size_t i = 0; i < aEvent->touches.Length(); i++) {
    if (aEvent->touches[i]->Identifier() == aIdentifier) {
      nsIntPoint touchIntPoint = aEvent->touches[i]->mRefPoint;

      // Return dev pixel directly, only used for gtest.
      if (!mPresShell) {
        return nsPoint(touchIntPoint.x, touchIntPoint.y);
      }

      // Get event coordinate relative to root frame.
      nsIFrame* rootFrame = mPresShell->GetRootFrame();
      return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                          touchIntPoint,
                                                          rootFrame);
    }
  }
  return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

nsPoint
CopyPasteEventHub::GetMouseEventPosition(WidgetMouseEvent* aEvent)
{
  nsIntPoint mouseIntPoint =
    LayoutDeviceIntPoint::ToUntyped(aEvent->AsGUIEvent()->refPoint);

  // Return dev pixel directly, only used for gtest.
  if (!mPresShell) {
    return nsPoint(mouseIntPoint.x, mouseIntPoint.y);
  }

  // Get event coordinate relative to root frame.
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                      mouseIntPoint,
                                                      rootFrame);
}

} // namespace mozilla
