/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteEventHub.h"

#include "CopyPasteManager.h"
#include "gfxPrefs.h"
#include "mozilla/TouchEvents.h"
#include "nsFocusManager.h"
#include "nsFrameSelection.h"
#include "nsITimer.h"
#include "prlog.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(CopyPasteEventHub,
                  nsIReflowObserver,
                  nsIScrollObserver,
                  nsISupportsWeakReference)

// Avoid redefine macros
#undef LOG

#ifdef PR_LOGGING
static PRLogModuleInfo* gCopyPasteEventHubLogModule;
static const char* kCopyPasteEventHubModuleName = "CopyPasteEventHub";
#define LOG(level, message, ...)                                               \
  PR_LOG(gCopyPasteEventHubLogModule, level,                                   \
         ("%s (%p): %s:%d : " message "\n", kCopyPasteEventHubModuleName,      \
          this, __FUNCTION__, __LINE__, ##__VA_ARGS__));
#define LOG_DEBUG(...) LOG(PR_LOG_DEBUG, ##__VA_ARGS__)
#define LOG_WARNING(...) LOG(PR_LOG_WARNING, ##__VA_ARGS__)
#define LOG_ERROR(...) LOG(PR_LOG_ERROR, ##__VA_ARGS__)

#else
#define LOG(level, message, ...)
#define LOG_DEBUG(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)
#endif // #ifdef PR_LOGGING

/* static */ const char*
CopyPasteEventHub::ToStr(InputState aInputState) {
  switch(aInputState) {
  case InputState::PRESS: return "PRESS";
  case InputState::DRAG: return "DRAG";
  case InputState::RELEASE: return "RELEASE";
  default: return "";
  }
}

/* static */ const char*
CopyPasteEventHub::ToStr(InputType aInputType) {
  switch(aInputType) {
  case InputType::NONE: return "NONE";
  case InputType::MOUSE: return "MOUSE";
  case InputType::TOUCH: return "TOUCH";
  default: return "";
  }
}

//
// Implementation of methods in base state class
//

CopyPasteEventHub::State::State(CopyPasteEventHub* aCopyPasteEventHub)
  : mCopyPasteEventHub(aCopyPasteEventHub)
{
}

CopyPasteEventHub::State::~State()
{

}

nsEventStatus
CopyPasteEventHub::State::OnPress(const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::State::OnMove(const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::State::OnRelease()
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::State::OnLongTap(const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::State::OnTap(const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::State::OnScrollStart()
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::State::OnScrollEnd()
{
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteEventHub::State::OnBlur()
{
  return nsEventStatus_eIgnore;
}

//
// No Action State
//
class CopyPasteEventHub::NoActionState : public CopyPasteEventHub::State
{
public:
  NoActionState(CopyPasteEventHub* aCopyPasteEventHub)
    : State(aCopyPasteEventHub) {}

  ~NoActionState() {}

  virtual nsEventStatus OnPress(const nsPoint& aPoint) MOZ_OVERRIDE
  {
    return nsEventStatus_eIgnore;
  }
};

//
// Press State
//
class CopyPasteEventHub::PressState : public CopyPasteEventHub::State
{
public:
  PressState(CopyPasteEventHub* aCopyPasteEventHub)
    : State(aCopyPasteEventHub) {}

  ~PressState() {}

  virtual nsEventStatus OnPress(const nsPoint& aPoint) MOZ_OVERRIDE
  {
    return nsEventStatus_eIgnore;
  }
};

CopyPasteEventHub::CopyPasteEventHub(nsIPresShell* aPresShell,
                                     CopyPasteManager* aHandler)
  : mAsyncPanZoomEnabled(false)
  , mState(MakeUnique<NoActionState>(this))
  , mInputState(InputState::RELEASE)
  , mType(InputType::NONE)
  , mActiveTouchId(kInvalidTouchId)
  , mPresShell(aPresShell)
  , mHandler(aHandler)
{
#ifdef PR_LOGGING
  if (!gCopyPasteEventHubLogModule) {
    gCopyPasteEventHubLogModule = PR_NewLogModule(kCopyPasteEventHubModuleName);
  }
#endif
}

void
CopyPasteEventHub::Init()
{
  if (!mPresShell) {
    return;
  }

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

  mLongTapDetectorTimer = do_CreateInstance("@mozilla.org/timer;1");
  mScrollEndDetectorTimer = do_CreateInstance("@mozilla.org/timer;1");
}

void
CopyPasteEventHub::Terminate()
{
  nsRefPtr<nsDocShell> docShell(mDocShell.get());
  if (docShell) {
    docShell->RemoveWeakReflowObserver(this);
    docShell->RemoveWeakScrollObserver(this);
  }

  if (mLongTapDetectorTimer) {
    mLongTapDetectorTimer->Cancel();
  }
  if (mScrollEndDetectorTimer) {
    mScrollEndDetectorTimer->Cancel();
  }
}

nsEventStatus
CopyPasteEventHub::HandleEvent(WidgetEvent* aEvent)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  switch (aEvent->mClass) {
  case eMouseEventClass:
    status = HandleMouseEvent(aEvent->AsMouseEvent());
    break;

  case eTouchEventClass:
    status = HandleTouchEvent(aEvent->AsTouchEvent());
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

  nsPoint point = GetMouseEventPosition(aEvent);

  switch (aEvent->message) {
  case NS_MOUSE_BUTTON_DOWN:
    mLastPressEventPoint = point;
    rv = mState->OnPress(point);
    break;

  case NS_MOUSE_MOVE:
    rv = mState->OnMove(point);
    break;

  case NS_MOUSE_BUTTON_UP:
    rv = mState->OnRelease();
    break;

  case NS_MOUSE_MOZLONGTAP:
    rv = mState->OnLongTap(point);
    break;

  default:
    break;
  }

  return rv;
}

nsEventStatus
CopyPasteEventHub::HandleTouchEvent(WidgetTouchEvent* aEvent)
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  nsPoint point = GetTouchEventPosition(aEvent, mActiveTouchId);

  switch (aEvent->message) {
  case NS_TOUCH_START:
    rv = mState->OnPress(point);
    break;

  case NS_TOUCH_MOVE:
    rv = mState->OnMove(point);
    break;

  case NS_TOUCH_END:
  case NS_TOUCH_CANCEL:
    rv = mState->OnRelease();
    break;

  default:
    break;
  }

  return rv;
}

nsEventStatus
CopyPasteEventHub::HandleMouseMoveEvent(WidgetMouseEvent* aEvent)
{
  LOG_DEBUG("Got a mouse move in state %s", ToStr(mInputState));

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mInputState) {
    case InputState::PRESS:
    case InputState::DRAG:
      {
        nsPoint movePoint = GetMouseEventPosition(aEvent);
        if (mInputState == InputState::PRESS &&
            IsDistanceExceededDragThreshold(mLastPressEventPoint, movePoint)) {
          SetState(InputState::DRAG);
        }
        status = mHandler->OnDrag(movePoint);
      }
      break;
    default:
      break;
  }

  return status;
}

nsEventStatus
CopyPasteEventHub::HandleTouchMoveEvent(WidgetTouchEvent* aEvent)
{
  LOG_DEBUG("Got a touch move in state %s", ToStr(mInputState));

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mInputState) {
    case InputState::PRESS:
    case InputState::DRAG:
      {
        if (mActiveTouchId == kInvalidTouchId) {
          break;
        }

        nsPoint movePoint = GetTouchEventPosition(aEvent, mActiveTouchId);
        if (mInputState == InputState::PRESS &&
            IsDistanceExceededDragThreshold(mLastPressEventPoint, movePoint)) {
          SetState(InputState::DRAG);
        }
        status = mHandler->OnDrag(movePoint);
      }
      break;
    default:
      break;
  }

  return status;
}

nsEventStatus
CopyPasteEventHub::HandleMouseUpEvent(WidgetMouseEvent* aEvent)
{
  LOG_DEBUG("Got a mouse up in state %s", ToStr(mInputState));

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mInputState) {
    case InputState::PRESS:
    case InputState::DRAG:
      if (aEvent->button == WidgetMouseEvent::eLeftButton) {
        status = mHandler->OnRelease();
        mType = InputType::NONE;
        if (mInputState == InputState::PRESS) {
          mHandler->OnTap(mLastPressEventPoint);
        }
        SetState(InputState::RELEASE);
      }
      break;
    default:
      break;
  }

  return status;
}

nsEventStatus
CopyPasteEventHub::HandleTouchEndEvent(WidgetTouchEvent* aEvent)
{
  LOG_DEBUG("Got a touch up in state %s", ToStr(mInputState));

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mInputState) {
    case InputState::PRESS:
    case InputState::DRAG:
      if (aEvent->touches[0]->Identifier() == mActiveTouchId) {
        status = mHandler->OnRelease();
        mActiveTouchId = kInvalidTouchId;
        mType = InputType::NONE;
        if (mInputState == InputState::PRESS) {
          mHandler->OnTap(mLastPressEventPoint);
        }
        SetState(InputState::RELEASE);
      }
      break;
    default:
      break;
  }

  return status;
}

nsEventStatus
CopyPasteEventHub::HandleMouseDownEvent(WidgetMouseEvent* aEvent)
{
  LOG_DEBUG("Got a mouse down in state %s", ToStr(mInputState));

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mInputState) {
    case InputState::RELEASE:
      if (aEvent->button == WidgetMouseEvent::eLeftButton) {
        nsPoint mLastPressEventPoint = GetMouseEventPosition(aEvent);
        SetState(InputState::PRESS);
        mType = InputType::MOUSE;
        status = mHandler->OnPress(mLastPressEventPoint);
      }
      break;
    default:
      break;
  }

  return status;
}

nsEventStatus
CopyPasteEventHub::HandleTouchStartEvent(WidgetTouchEvent* aEvent)
{
  LOG_DEBUG("Got a touch down in state %s", ToStr(mInputState));

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mInputState) {
    case InputState::RELEASE:
      if (mActiveTouchId == kInvalidTouchId) {
        mActiveTouchId = aEvent->touches[0]->Identifier();
        nsPoint mLastPressEventPoint = GetTouchEventPosition(aEvent, mActiveTouchId);
        SetState(InputState::PRESS);
        mType = InputType::TOUCH;
        status = mHandler->OnPress(mLastPressEventPoint);
      }
      break;
    default:
      break;
  }

  return status;
}

nsEventStatus
CopyPasteEventHub::HandleLongTapEvent(WidgetMouseEvent* aEvent)
{
  LOG_DEBUG("Got a long tap in state %s", ToStr(mInputState));

  nsPoint point = aEvent ? GetMouseEventPosition(aEvent) : mLastPressEventPoint;
  return mHandler->OnLongTap(point);
}

void
CopyPasteEventHub::HandleScrollStart()
{
  mHandler->OnScrollStart();
}

void
CopyPasteEventHub::HandleScrollEnd()
{
  mHandler->OnScrollEnd();
}

void
CopyPasteEventHub::SetState(InputState aState)
{
  switch (aState) {
    case InputState::RELEASE:
      CancelLongTapDetector();
      break;
    case InputState::PRESS:
      LaunchLongTapDetector();
      break;
    case InputState::DRAG:
      CancelLongTapDetector();
      break;
  }
  mInputState = aState;
}


bool
CopyPasteEventHub::IsDistanceExceededDragThreshold(const nsPoint& aPoint1,
                                                   const nsPoint& aPoint2)
{
  nsPoint delta = aPoint1 - aPoint2;
  return NS_hypot(delta.x, delta.y) >
    nsPresContext::AppUnitsPerCSSPixel() * kMinDragDistanceInPixel;
}

void
CopyPasteEventHub::LaunchLongTapDetector()
{
  if (mAsyncPanZoomEnabled) {
    return;
  }

  if (!mLongTapDetectorTimer) {
    return;
  }

  int32_t longTapDelay = gfxPrefs::UiClickHoldContextMenusDelay();
  mLongTapDetectorTimer->InitWithFuncCallback(FireLongTap,
                                              this,
                                              longTapDelay,
                                              nsITimer::TYPE_ONE_SHOT);
}

void
CopyPasteEventHub::CancelLongTapDetector()
{
  if (mAsyncPanZoomEnabled) {
    return;
  }

  if (!mLongTapDetectorTimer) {
    return;
  }

  mLongTapDetectorTimer->Cancel();
}

/* static */ void
CopyPasteEventHub::FireLongTap(nsITimer* aTimer, void* aCopyPasteEventHub)
{
  CopyPasteEventHub* self = static_cast<CopyPasteEventHub*>(aCopyPasteEventHub);
  self->HandleLongTapEvent(nullptr);
}

NS_IMETHODIMP
CopyPasteEventHub::Reflow(DOMHighResTimeStamp aStart,
                          DOMHighResTimeStamp aEnd)
{
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
  HandleScrollStart();
}

void
CopyPasteEventHub::AsyncPanZoomStopped(const CSSIntPoint aScrollPos)
{
  HandleScrollEnd();
}

void
CopyPasteEventHub::ScrollPositionChanged()
{
  HandleScrollStart();
  LaunchScrollEndDetector();
}

void
CopyPasteEventHub::LaunchScrollEndDetector()
{
  if (mAsyncPanZoomEnabled) {
    return;
  }

  if (!mScrollEndDetectorTimer) {
    return;
  }

  mScrollEndDetectorTimer->InitWithFuncCallback(FireScrollEnd,
                                                this,
                                                kScrollEndTimerDelay,
                                                nsITimer::TYPE_ONE_SHOT);
}

/* static */ void
CopyPasteEventHub::FireScrollEnd(nsITimer* aTimer, void* aCopyPasteEventHub)
{
  CopyPasteEventHub* self = static_cast<CopyPasteEventHub*>(aCopyPasteEventHub);
  self->HandleScrollEnd();
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
