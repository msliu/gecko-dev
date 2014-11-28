/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteManager.h"

#include "AccessibleCaret.h"
#include "CopyPasteManagerGlue.h"
#include "mozilla/dom/TreeWalker.h"
#include "mozilla/TouchEvents.h"
#include "nsDocShell.h"
#include "nsFocusManager.h"
#include "nsFrameSelection.h"
#include "nsIDOMNodeFilter.h"

using namespace mozilla;
using namespace mozilla::dom;
using Appearance = AccessibleCaret::Appearance;
static const int32_t kMoveStartTolerancePx2 = 5;

NS_IMPL_ISUPPORTS(CopyPasteManager, nsISelectionListener)

CopyPasteManager::CopyPasteManager()
  : mDragMode(DragMode::NONE)
  , mCaretMode(CaretMode::NONE)
  , mCaretCenterToDownPointOffsetY(0)
  , mHasInited(false)
{
}

void
CopyPasteManager::Init(nsIPresShell* aPresShell)
{
  if (aPresShell->GetCanvasFrame()) {
    // TODO: Pass canvas frame directly to AccessibleCaret's constructor.
    mFirstCaret = new AccessibleCaret(aPresShell);
    mSecondCaret = new AccessibleCaret(aPresShell);
    mGlue = new CopyPasteManagerGlue(aPresShell);
    mGestureManager = new GestureManager(aPresShell, this);
    mHasInited = true;
  }
}

CopyPasteManager::~CopyPasteManager()
{
}

nsEventStatus
CopyPasteManager::HandleEvent(WidgetEvent* aEvent)
{
  if (!mHasInited) {
    return nsEventStatus_eIgnore;
  }

  return mGestureManager->HandleEvent(aEvent);
}

nsresult
CopyPasteManager::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                         nsISelection* aSel,
                                         int16_t aReason)
{
  if (!mHasInited) {
    return NS_OK;
  }

  UpdateCarets();
  return NS_OK;
}

void
CopyPasteManager::HideCarets()
{
  mFirstCaret->SetAppearance(Appearance::NONE);
  mSecondCaret->SetAppearance(Appearance::NONE);
  mCaretMode = CaretMode::NONE;
}

void
CopyPasteManager::UpdateCarets()
{
  int32_t rangeCount = mGlue->GetSelectionRangeCount();
  if (!rangeCount) {
    HideCarets();
    return;
  }

  mCaretMode = mGlue->GetSelectionIsCollapsed() ?
               CaretMode::CURSOR :
               CaretMode::SELECTION;

  // Update first caret
  int32_t startOffset;
  nsIFrame* startFrame = mGlue->FindFirstNodeWithFrame(false, startOffset);

  if (!startFrame) {
    HideCarets();
    return;
  }

  if (mCaretMode == CaretMode::CURSOR &&
      !startFrame->GetContent()->IsEditable()) {
    HideCarets();
    return;
  }

  mFirstCaret->SetPositionBasedOnFrameOffset(startFrame, startOffset);

  //Update second caret
  if (mCaretMode == CaretMode::SELECTION) {
    int32_t endOffset;
    nsIFrame* endFrame = mGlue->FindFirstNodeWithFrame(true, endOffset);

    if (!endFrame) {
      HideCarets();
      return;
    }

    // Check if startFrame is after endFrame.
    if (nsLayoutUtils::CompareTreePosition(startFrame, endFrame) > 0) {
      HideCarets();
      return;
    }

    mSecondCaret->SetPositionBasedOnFrameOffset(endFrame, endOffset);
  }

  if (mFirstCaret->Intersects(*mSecondCaret)) {
    mFirstCaret->SetAppearance(Appearance::LEFT);
    mSecondCaret->SetAppearance(Appearance::RIGHT);
  } else {
    mFirstCaret->SetAppearance(Appearance::NORMAL);

    if (mCaretMode == CaretMode::CURSOR) {
      mSecondCaret->SetAppearance(Appearance::NONE);
    } else if (mCaretMode == CaretMode::SELECTION) {
      mSecondCaret->SetAppearance(Appearance::NORMAL);
    }
  }
}

nsEventStatus
CopyPasteManager::OnPress(const nsPoint& aPoint)
{
  if (mFirstCaret->Contains(aPoint)) {
    mDragMode = DragMode::FIRST_CARET;
    mCaretCenterToDownPointOffsetY = mFirstCaret->GetFrameOffsetRect().y - aPoint.y;
    mGlue->SetSelectionDirection(false);
    mGlue->SetSelectionDragState(true);
    return nsEventStatus_eConsumeNoDefault;
  } else if (mSecondCaret->Contains(aPoint)) {
    mDragMode = DragMode::SECOND_CARET;
    mCaretCenterToDownPointOffsetY = mSecondCaret->GetFrameOffsetRect().y - aPoint.y;
    mGlue->SetSelectionDirection(true);
    mGlue->SetSelectionDragState(true);
    return nsEventStatus_eConsumeNoDefault;
  } else {
    mDragMode = DragMode::NONE;
  }

  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteManager::OnDrag(const nsPoint& aPoint)
{
  if (mDragMode != DragMode::NONE) {
    nsPoint point = aPoint;
    point.y += mCaretCenterToDownPointOffsetY;
    mGlue->DragCaret(point,
                     mCaretMode == CaretMode::SELECTION,
                     mDragMode == DragMode::FIRST_CARET);
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteManager::OnRelease()
{
  if (mDragMode != DragMode::NONE) {
    mGlue->SetSelectionDragState(false);
    mDragMode = DragMode::NONE;
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteManager::OnLongTap(const nsPoint& aPoint)
{
  if (mCaretMode != CaretMode::SELECTION) {
    mGlue->SelectWord(aPoint);
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteManager::OnTap(const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
}

NS_IMPL_ISUPPORTS0(CopyPasteManager::GestureManager)

CopyPasteManager::GestureManager::GestureManager(nsIPresShell* aPresShell,
                                                 CopyPasteManager* aHandler)
  : mState(InputState::RELEASE)
  , mType(InputType::NONE)
  , mActiveTouchId(-1)
  , mPresShell(aPresShell)
  , mHandler(aHandler)
  , mAsyncPanZoomEnabled(false)
{
  nsPresContext* presContext = mPresShell->GetPresContext();
  MOZ_ASSERT(presContext, "PresContext should be given in PresShell::Init()");

  nsIDocShell* docShell = presContext->GetDocShell();
  if (!docShell) {
    return;
  }

  docShell->GetAsyncPanZoomEnabled(&mAsyncPanZoomEnabled);
  mAsyncPanZoomEnabled = mAsyncPanZoomEnabled && gfxPrefs::AsyncPanZoomEnabled();
}

nsEventStatus
CopyPasteManager::GestureManager::HandleEvent(WidgetEvent* aEvent)
{
  switch (aEvent->message) {
    case NS_MOUSE_BUTTON_UP:
    case NS_MOUSE_MOVE:
      if (mType != InputType::MOUSE) {
        return nsEventStatus_eIgnore;
      }
      break;
    case NS_TOUCH_END:
    case NS_TOUCH_MOVE:
    case NS_TOUCH_CANCEL:
      if (mType != InputType::TOUCH) {
        return nsEventStatus_eIgnore;
      }
      break;
  }

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (aEvent->message) {
    case NS_TOUCH_START:
      status = HandleTouchDownEvent(aEvent->AsTouchEvent());
      break;
    case NS_MOUSE_BUTTON_DOWN:
      status = HandleMouseDownEvent(aEvent->AsMouseEvent());
      break;
    case NS_TOUCH_END:
    case NS_TOUCH_CANCEL:
      status = HandleTouchUpEvent(aEvent->AsTouchEvent());
      break;
    case NS_MOUSE_BUTTON_UP:
      status = HandleMouseUpEvent(aEvent->AsMouseEvent());
      break;
    case NS_TOUCH_MOVE:
      status = HandleTouchMoveEvent(aEvent->AsTouchEvent());
      break;
    case NS_MOUSE_MOVE:
      status = HandleMouseMoveEvent(aEvent->AsMouseEvent());
      break;
    case NS_MOUSE_MOZLONGTAP:
      status = HandleLongTapEvent(aEvent->AsMouseEvent());
      break;
  }

  return status;
}

nsEventStatus
CopyPasteManager::GestureManager::HandleMouseMoveEvent(WidgetMouseEvent* aEvent)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case InputState::PRESS:
    case InputState::DRAG:
      {
        nsPoint movePoint = GetEventPosition(aEvent);
        nsPoint delta = mDownPoint - movePoint;
        if (mState == InputState::PRESS &&
            NS_hypot(delta.x, delta.y) >
              nsPresContext::AppUnitsPerCSSPixel() * kMoveStartTolerancePx2) {
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
CopyPasteManager::GestureManager::HandleTouchMoveEvent(WidgetTouchEvent* aEvent)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case InputState::PRESS:
    case InputState::DRAG:
      {
        if (mActiveTouchId == -1) {
          break;
        }

        nsPoint movePoint = GetEventPosition(aEvent, mActiveTouchId);
        nsPoint delta = mDownPoint - movePoint;
        if (mState == InputState::PRESS &&
            NS_hypot(delta.x, delta.y) >
              nsPresContext::AppUnitsPerCSSPixel() * kMoveStartTolerancePx2) {
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
CopyPasteManager::GestureManager::HandleMouseUpEvent(WidgetMouseEvent* aEvent)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case InputState::PRESS:
    case InputState::DRAG:
      if (aEvent->button == WidgetMouseEvent::eLeftButton) {
        SetState(InputState::RELEASE);
        status = mHandler->OnRelease();
      }
      break;
    default:
      break;
  }

  return status;
}

nsEventStatus
CopyPasteManager::GestureManager::HandleTouchUpEvent(WidgetTouchEvent* aEvent)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case InputState::PRESS:
    case InputState::DRAG:
      if (aEvent->touches[0]->mIdentifier == mActiveTouchId) {
        SetState(InputState::RELEASE);
        status = mHandler->OnRelease();
      }
      break;
    default:
      break;
  }

  return status;
}

nsEventStatus
CopyPasteManager::GestureManager::HandleMouseDownEvent(WidgetMouseEvent* aEvent)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case InputState::RELEASE:
      if (aEvent->button == WidgetMouseEvent::eLeftButton) {
        nsPoint point = GetEventPosition(aEvent);
        mDownPoint = point;
        SetState(InputState::PRESS);
        mType = InputType::MOUSE;
        status = mHandler->OnPress(point);
      }
      break;
    default:
      break;
  }

  return status;
}

nsEventStatus
CopyPasteManager::GestureManager::HandleTouchDownEvent(WidgetTouchEvent* aEvent)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case InputState::RELEASE:
      if (mActiveTouchId == -1) {
        mActiveTouchId = aEvent->touches[0]->Identifier();
        nsPoint point = GetEventPosition(aEvent, mActiveTouchId);
        mDownPoint = point;
        SetState(InputState::PRESS);
        mType = InputType::TOUCH;
        status = mHandler->OnPress(point);
      }
      break;
    default:
      break;
  }

  return status;
}

void
CopyPasteManager::GestureManager::SetState(InputState aState)
{
  switch (aState) {
    case InputState::RELEASE:
      mType = InputType::NONE;
      if (mState == InputState::PRESS) {
        mHandler->OnTap(mDownPoint);
      }
      CancelLongTapDetector();
      break;
    case InputState::PRESS:
      LaunchLongTapDetector();
      break;
    case InputState::DRAG:
      CancelLongTapDetector();
      break;
  }
  mState = aState;
}

nsEventStatus
CopyPasteManager::GestureManager::HandleLongTapEvent(WidgetMouseEvent* aEvent)
{
  nsPoint point = aEvent ? GetEventPosition(aEvent) : mDownPoint;
  return mHandler->OnLongTap(point);
}

void
CopyPasteManager::GestureManager::LaunchLongTapDetector()
{
  if (mAsyncPanZoomEnabled) {
    return;
  }

  if (!mLongTapDetectorTimer) {
    mLongTapDetectorTimer = do_CreateInstance("@mozilla.org/timer;1");
  }

  MOZ_ASSERT(mLongTapDetectorTimer);
  CancelLongTapDetector();
  int32_t longTapDelay = gfxPrefs::UiClickHoldContextMenusDelay();

  mLongTapDetectorTimer->InitWithFuncCallback(FireLongTap,
                                              this,
                                              longTapDelay,
                                              nsITimer::TYPE_ONE_SHOT);
}

void
CopyPasteManager::GestureManager::CancelLongTapDetector()
{
  if (mAsyncPanZoomEnabled) {
    return;
  }

  if (!mLongTapDetectorTimer) {
    return;
  }

  mLongTapDetectorTimer->Cancel();
}

/* static */void
CopyPasteManager::GestureManager::FireLongTap(nsITimer* aTimer, void* aGestureManager)
{
  GestureManager* self = static_cast<GestureManager*>(aGestureManager);
  NS_PRECONDITION(aTimer == self->mLongTapDetectorTimer,
                  "Unexpected timer");

  self->HandleLongTapEvent(nullptr);
}

nsPoint
CopyPasteManager::GestureManager::GetEventPosition(WidgetTouchEvent* aEvent, int32_t aIdentifier)
{
  for (size_t i = 0; i < aEvent->touches.Length(); i++) {
    if (aEvent->touches[i]->mIdentifier == aIdentifier) {
      // Get event coordinate relative to root frame.
      nsIFrame* rootFrame = mPresShell->GetRootFrame();
      nsIntPoint touchIntPoint = aEvent->touches[i]->mRefPoint;
      return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                          touchIntPoint,
                                                          rootFrame);
    }
  }
  return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

nsPoint
CopyPasteManager::GestureManager::GetEventPosition(WidgetMouseEvent* aEvent)
{
  // Get event coordinate relative to root frame.
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  nsIntPoint mouseIntPoint =
    LayoutDeviceIntPoint::ToUntyped(aEvent->AsGUIEvent()->refPoint);
  return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                      mouseIntPoint,
                                                      rootFrame);
}
