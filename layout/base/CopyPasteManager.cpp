/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteManager.h"

#include "AccessibleCaret.h"
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

CopyPasteManager::CopyPasteManager(nsIPresShell* aPresShell)
  : mPresShell(aPresShell)
  , mFirstCaret(new AccessibleCaret(aPresShell))
  , mSecondCaret(new AccessibleCaret(aPresShell))
  , mDragMode(DragMode::NONE)
  , mCaretMode(CaretMode::NONE)
  , mCaretCenterToDownPointOffsetY(0)
  , mGestureManager(aPresShell, this)
{
}

CopyPasteManager::~CopyPasteManager()
{
}

nsEventStatus
CopyPasteManager::HandleEvent(WidgetEvent* aEvent)
{
  return mGestureManager.HandleEvent(aEvent);
}

nsresult
CopyPasteManager::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                         nsISelection* aSel,
                                         int16_t aReason)
{
  UpdateCarets();
  return NS_OK;
}

nsIContent*
CopyPasteManager::GetFocusedContent()
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    return fm->GetFocusedContent();
  }

  return nullptr;
}

Selection*
CopyPasteManager::GetSelection()
{
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    return fs->GetSelection(nsISelectionController::SELECTION_NORMAL);
  }
  return nullptr;
}

already_AddRefed<nsFrameSelection>
CopyPasteManager::GetFrameSelection()
{
  nsIContent* focusNode = GetFocusedContent();
  if (focusNode) {
    nsIFrame* focusFrame = focusNode->GetPrimaryFrame();
    if (!focusFrame) {
      return nullptr;
    }
    return focusFrame->GetFrameSelection();
  } else {
    return mPresShell->FrameSelection();
  }
}

/* static */ nsIFrame*
CopyPasteManager::FindFirstNodeWithFrame(nsIDocument* aDocument,
                                         nsRange* aRange,
                                         nsFrameSelection* aFrameSelection,
                                         bool aBackward,
                                         int& aOutOffset)
{
  if (!aDocument || !aRange || !aFrameSelection) {
    return nullptr;
  }

  nsCOMPtr<nsINode> startNode =
    do_QueryInterface(aBackward ? aRange->GetEndParent() : aRange->GetStartParent());
  nsCOMPtr<nsINode> endNode =
    do_QueryInterface(aBackward ? aRange->GetStartParent() : aRange->GetEndParent());
  int32_t offset = aBackward ? aRange->EndOffset() : aRange->StartOffset();

  nsCOMPtr<nsIContent> startContent = do_QueryInterface(startNode);
  CaretAssociationHint hintStart =
    aBackward ? CARET_ASSOCIATE_BEFORE : CARET_ASSOCIATE_AFTER;
  nsIFrame* startFrame = aFrameSelection->GetFrameForNodeOffset(startContent,
                                                                offset,
                                                                hintStart,
                                                                &aOutOffset);

  if (startFrame) {
    return startFrame;
  }

  ErrorResult err;
  nsRefPtr<TreeWalker> walker =
    aDocument->CreateTreeWalker(*startNode,
                                nsIDOMNodeFilter::SHOW_ALL,
                                nullptr,
                                err);

  if (!walker) {
    return nullptr;
  }

  startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
  while (!startFrame && startNode != endNode) {
    if (aBackward) {
      startNode = walker->PreviousNode(err);
    } else {
      startNode = walker->NextNode(err);
    }

    if (!startNode) {
      break;
    }

    startContent = do_QueryInterface(startNode);
    startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
  }
  return startFrame;
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
  if (!mPresShell) {
    return;
  }

  nsRefPtr<Selection> selection = GetSelection();
  if (!selection) {
    HideCarets();
    return;
  }

  if (selection->IsCollapsed()) {
    nsRefPtr<nsRange> firstRange = selection->GetRangeAt(0);

    nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
    if (!fs) {
      HideCarets();
      return;
    }

    int32_t startOffset;
    nsIFrame* startFrame = CopyPasteManager::FindFirstNodeWithFrame(mPresShell->GetDocument(),
                                                                    firstRange, fs, false, startOffset);

    if (!startFrame) {
      HideCarets();
      return;
    }

    if (!startFrame->GetContent()->IsEditable()) {
      HideCarets();
      return;
    }

    mFirstCaret->SetPositionBasedOnFrameOffset(startFrame, startOffset);
    mFirstCaret->SetAppearance(Appearance::NORMAL);
    mSecondCaret->SetAppearance(Appearance::NONE);
    mCaretMode = CaretMode::CURSOR;
  } else {
    int32_t rangeCount = selection->GetRangeCount();
    nsRefPtr<nsRange> firstRange = selection->GetRangeAt(0);
    nsRefPtr<nsRange> lastRange = selection->GetRangeAt(rangeCount - 1);

    nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
    if (!fs) {
      HideCarets();
      return;
    }

    int32_t startOffset;
    nsIFrame* startFrame = CopyPasteManager::FindFirstNodeWithFrame(mPresShell->GetDocument(),
                                                                    firstRange, fs, false, startOffset);

    int32_t endOffset;
    nsIFrame* endFrame = CopyPasteManager::FindFirstNodeWithFrame(mPresShell->GetDocument(),
                                                                  lastRange, fs, true, endOffset);

    if (!startFrame || !endFrame) {
      HideCarets();
      return;
    }

    // Check if startFrame is after endFrame.
    if (nsLayoutUtils::CompareTreePosition(startFrame, endFrame) > 0) {
      HideCarets();
      return;
    }

    mFirstCaret->SetPositionBasedOnFrameOffset(startFrame, startOffset);
    mSecondCaret->SetPositionBasedOnFrameOffset(endFrame, endOffset);
    if (mFirstCaret->Intersects(*mSecondCaret)) {
      mFirstCaret->SetAppearance(Appearance::LEFT);
      mSecondCaret->SetAppearance(Appearance::RIGHT);
    } else {
      mFirstCaret->SetAppearance(Appearance::NORMAL);
      mSecondCaret->SetAppearance(Appearance::NORMAL);
    }
    mCaretMode = CaretMode::SELECTION;
  }
}

nsEventStatus
CopyPasteManager::OnPress(const nsPoint& aPoint)
{
  if (mFirstCaret->Contains(aPoint)) {
    mDragMode = DragMode::FIRST_CARET;
    mCaretCenterToDownPointOffsetY = mFirstCaret->GetFrameOffsetRect().y - aPoint.y;
    SetSelectionDirection(false);
    SetSelectionDragState(true);
    return nsEventStatus_eConsumeNoDefault;
  } else if (mSecondCaret->Contains(aPoint)) {
    mDragMode = DragMode::SECOND_CARET;
    mCaretCenterToDownPointOffsetY = mSecondCaret->GetFrameOffsetRect().y - aPoint.y;
    SetSelectionDirection(true);
    SetSelectionDragState(true);
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
    DragCaret(point);
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

/*
 * If we're dragging start caret, we do not want to drag over previous
 * character of end caret. Same as end caret. So we check if content offset
 * exceed previous/next character of end/start caret base on aDragMode.
 */
// XXX we have same name at SelectionCarets.
static bool
CompareRangeWithContentOffset2(nsRange* aRange,
                              nsFrameSelection* aSelection,
                              nsIFrame::ContentOffsets& aOffsets,
                              CopyPasteManager::DragMode aDragMode)
{
  MOZ_ASSERT(aDragMode != CopyPasteManager::DragMode::NONE);
  nsINode* node = nullptr;
  int32_t nodeOffset = 0;
  CaretAssociationHint hint;
  nsDirection dir;

  if (aDragMode == CopyPasteManager::DragMode::FIRST_CARET) {
    // Check previous character of end node offset
    node = aRange->GetEndParent();
    nodeOffset = aRange->EndOffset();
    hint = CARET_ASSOCIATE_BEFORE;
    dir = eDirPrevious;
  } else {
    // Check next character of start node offset
    node = aRange->GetStartParent();
    nodeOffset = aRange->StartOffset();
    hint =  CARET_ASSOCIATE_AFTER;
    dir = eDirNext;
  }
  nsCOMPtr<nsIContent> content = do_QueryInterface(node);

  int32_t offset = 0;
  nsIFrame* theFrame =
    aSelection->GetFrameForNodeOffset(content, nodeOffset, hint, &offset);

  if (!theFrame) {
    return false;
  }

  // Move one character forward/backward from point and get offset
  nsPeekOffsetStruct pos(eSelectCluster,
                         dir,
                         offset,
                         0,
                         true,
                         true,  //limit on scrolled views
                         false,
                         false);
  nsresult rv = theFrame->PeekOffset(&pos);
  if (NS_FAILED(rv)) {
    pos.mResultContent = content;
    pos.mContentOffset = nodeOffset;
  }

  // Compare with current point
  int32_t result = nsContentUtils::ComparePoints(aOffsets.content,
                                                 aOffsets.StartOffset(),
                                                 pos.mResultContent,
                                                 pos.mContentOffset);
  if ((aDragMode == CopyPasteManager::DragMode::FIRST_CARET && result == 1) ||
      (aDragMode == CopyPasteManager::DragMode::SECOND_CARET && result == -1)) {
    aOffsets.content = pos.mResultContent;
    aOffsets.offset = pos.mContentOffset;
    aOffsets.secondaryOffset = pos.mContentOffset;
  }

  return true;
}

nsEventStatus
CopyPasteManager::DragCaret(const nsPoint &aMovePoint)
{
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  if (!rootFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Find out which content we point to
  nsIFrame *ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, aMovePoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsresult result;
  nsIFrame *newFrame = nullptr;
  nsPoint newPoint;
  nsPoint ptInFrame = aMovePoint;
  nsLayoutUtils::TransformPoint(rootFrame, ptFrame, ptInFrame);
  result = fs->ConstrainFrameAndPointToAnchorSubtree(ptFrame, ptInFrame, &newFrame, newPoint);
  if (NS_FAILED(result) || !newFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  bool selectable;
  newFrame->IsSelectable(&selectable, nullptr);
  if (!selectable) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsFrame::ContentOffsets offsets =
    newFrame->GetContentOffsetsFromPoint(newPoint);
  if (!offsets.content) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsRefPtr<dom::Selection> selection = GetSelection();
  if (!selection) {
    return nsEventStatus_eConsumeNoDefault;
  }

  int32_t rangeCount = selection->GetRangeCount();
  if (rangeCount <= 0) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsRefPtr<nsRange> range = mDragMode == DragMode::FIRST_CARET ?
    selection->GetRangeAt(0) : selection->GetRangeAt(rangeCount - 1);
  if (mCaretMode == CaretMode::SELECTION &&
      !CompareRangeWithContentOffset2(range, fs, offsets, mDragMode)) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsIFrame* anchorFrame;
  selection->GetPrimaryFrameForAnchorNode(&anchorFrame);
  if (!anchorFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Move caret postion.
  nsIFrame *scrollable =
    nsLayoutUtils::GetClosestFrameOfType(anchorFrame, nsGkAtoms::scrollFrame);
  nsWeakFrame weakScrollable = scrollable;
  fs->HandleClick(offsets.content, offsets.StartOffset(),
                  offsets.EndOffset(),
                  mCaretMode == CaretMode::SELECTION,
                  false,
                  offsets.associate);
  if (!weakScrollable.IsAlive()) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Scroll scrolled frame.
  nsIScrollableFrame *saf = do_QueryFrame(scrollable);
  nsIFrame *capturingFrame = saf->GetScrolledFrame();
  nsPoint ptInScrolled = aMovePoint;
  nsLayoutUtils::TransformPoint(rootFrame, capturingFrame, ptInScrolled);
  // XXX: move 300 to a static const variable
  fs->StartAutoScrollTimer(capturingFrame, ptInScrolled, 300);
  UpdateCarets();
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
CopyPasteManager::OnRelease()
{
  if (mDragMode != DragMode::NONE) {
    SetSelectionDragState(false);
    mDragMode = DragMode::NONE;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteManager::OnLongTap(const nsPoint& aPoint)
{
  if (mCaretMode != CaretMode::SELECTION) {
    SelectWord(aPoint);
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteManager::OnTap(const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
}

void
CopyPasteManager::SetSelectionDragState(bool aState)
{
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->SetDragState(aState);
  }
}

void
CopyPasteManager::SetSelectionDirection(bool aForward)
{
  nsRefPtr<dom::Selection> selection = GetSelection();
  if (selection) {
    selection->SetDirection(aForward ? eDirNext : eDirPrevious);
  }
}

nsresult
CopyPasteManager::SelectWord(const nsPoint& aPoint)
{
  if (!mPresShell) {
    return NS_OK;
  }

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  if (!rootFrame) {
    return NS_OK;
  }

  // Find content offsets for mouse down point
  nsIFrame *ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, aPoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return NS_OK;
  }

  nsPoint ptInFrame = aPoint;
  nsLayoutUtils::TransformPoint(rootFrame, ptFrame, ptInFrame);

  // If target frame is editable, we should move focus to targe frame. If
  // target frame isn't editable and our focus content is editable, we should
  // clear focus.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  nsIContent* editingHost = ptFrame->GetContent()->GetEditingHost();
  if (editingHost) {
    nsCOMPtr<nsIDOMElement> elt = do_QueryInterface(editingHost->GetParent());
    if (elt) {
      fm->SetFocus(elt, 0);
    }
  } else {
    nsIContent* focusedContent = GetFocusedContent();
    if (focusedContent && focusedContent->GetTextEditorRootContent()) {
      nsIDOMWindow* win = mPresShell->GetDocument()->GetWindow();
      if (win) {
        fm->ClearFocus(win);
      }
    }
  }

  SetSelectionDragState(true);
  nsFrame* frame = static_cast<nsFrame*>(ptFrame);
  nsresult rs = frame->SelectByTypeAtPoint(mPresShell->GetPresContext(), ptInFrame,
                                           eSelectWord, eSelectWord, 0);

  SetSelectionDragState(false);

  // Clear maintain selection otherwise we cannot select less than a word
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->MaintainSelection();
  }
  return rs;
}

CopyPasteManager::GestureManager::GestureManager(nsIPresShell* aPresShell,
                                                 CopyPasteManager* aManager)
  : mState(InputState::RELEASE)
  , mType(InputType::NONE)
  , mActiveTouchId(-1)
  , mPresShell(aPresShell)
  , mHandler(aManager)
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
