/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteManager.h"

#include "AccessibleCaret.h"
#include "CopyPasteEventHub.h"
#include "CopyPasteLogger.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/TreeWalker.h"
#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsFrame.h"
#include "nsFrameSelection.h"

namespace mozilla {

#ifdef PR_LOGGING

#undef CP_LOG
#define CP_LOG(message, ...)                                                   \
  CP_LOG_BASE("CopyPasteManager (%p): " message, this, ##__VA_ARGS__);

#undef CP_LOGV
#define CP_LOGV(message, ...)                                                  \
  CP_LOGV_BASE("CopyPasteManager (%p): " message, this, ##__VA_ARGS__);

#endif // #ifdef PR_LOGGING

using namespace dom;
using Appearance = AccessibleCaret::Appearance;
using PositionChangedResult = AccessibleCaret::PositionChangedResult;

CopyPasteManager::CopyPasteManager(nsIPresShell* aPresShell)
  : mOffsetYToCaretLogicalPosition(0)
  , mPresShell(aPresShell)
  , mActiveCaret(nullptr)
  , mCaretMode(CaretMode::None)
{
  if (mPresShell) {
    mFirstCaret = MakeUnique<AccessibleCaret>(mPresShell);
    mSecondCaret = MakeUnique<AccessibleCaret>(mPresShell);

    mCaretTimeoutTimer = do_CreateInstance("@mozilla.org/timer;1");
  }
}

CopyPasteManager::~CopyPasteManager()
{
  CancelTimeoutTimer();
}

nsresult
CopyPasteManager::OnSelectionChanged(nsIDOMDocument* aDoc,
                                     nsISelection* aSel,
                                     int16_t aReason)
{
  CP_LOG("aSel: %p, GetSelection(): %p, aReason: %d", aSel, GetSelection(), aReason);

  if (aSel != GetSelection()) {
    return NS_OK;
  }

  // Move the cursor by Javascript.
  if (aReason == nsISelectionListener::NO_REASON) {
    HideCarets();
    return NS_OK;
  }

  // Move cursor by keyboard.
  if (aReason & nsISelectionListener::KEYPRESS_REASON) {
    HideCarets();
    return NS_OK;
  }

  // Range will collapse after cutting or copying text.
  if (aReason & (nsISelectionListener::COLLAPSETOSTART_REASON |
                 nsISelectionListener::COLLAPSETOEND_REASON)) {
    HideCarets();
    return NS_OK;
  }

  UpdateCarets();
  return NS_OK;
}

void
CopyPasteManager::HideCarets()
{
  CP_LOGV("%s", __FUNCTION__);
  mFirstCaret->SetAppearance(Appearance::None);
  mSecondCaret->SetAppearance(Appearance::None);
  CancelTimeoutTimer();
}

void
CopyPasteManager::UpdateCarets()
{
  mCaretMode = GetCaretMode();
  if (mCaretMode == CaretMode::None) {
    HideCarets();
    return;
  }

  // XXX: Calling this to force generate nsTextFrame for contents which contains
  // only newline in test_selectioncarets_multiplerange.html. It should be
  // removed once we implement event dispatching to Gaia.
  nsContentUtils::GetSelectionBoundingRect(GetSelection());

  if (mCaretMode == CaretMode::Cursor) {
    UpdateCaretsForCursorMode();
  } else {
    UpdateCaretsForSelectionMode();
  }
}

void
CopyPasteManager::UpdateCaretsForCursorMode()
{
  CP_LOGV("%s, selection: %p", __FUNCTION__, GetSelection());

  nsRefPtr<nsCaret> caret = mPresShell->GetCaret();
  if (!caret || !caret->IsVisible()) {
    HideCarets();
    return;
  }

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  Selection* selection = GetSelection();
  if (!fs || !selection) {
    HideCarets();
    return;
  }

  nsINode* focusNode = selection->GetFocusNode();
  nsIContent* focusContent = focusNode->AsContent();
  uint32_t focusOffset = selection->FocusOffset();

  nsIFrame* frame = nullptr;
  int32_t offset = 0;
  nsresult rv = nsCaret::GetCaretFrameForNodeOffset(
    fs, focusContent, focusOffset, fs->GetHint(), fs->GetCaretBidiLevel(),
    &frame, &offset);

  if (NS_FAILED(rv) || !frame) {
    HideCarets();
    return;
  }

  if (frame->GetStateBits() & NS_FRAME_IS_DIRTY) {
    mPresShell->FlushPendingNotifications(Flush_Layout);
  }

  Element* editingHost = frame->GetContent()->GetEditingHost();
  if (!editingHost) {
    HideCarets();
    return;
  }

  // No need to consider whether the caret's position is out of scrollport.
  // According to the spec, we need to explicitly hide it after the scrolling is
  // ended.
  mFirstCaret->SetPosition(frame, offset);
  if (nsContentUtils::HasNonEmptyTextContent(
        editingHost, nsContentUtils::eRecurseIntoChildren)) {
    mFirstCaret->SetAppearance(Appearance::Normal);
    LaunchTimeoutTimer();
  } else {
    mFirstCaret->SetAppearance(Appearance::NormalNotShown);
  }
  mSecondCaret->SetAppearance(Appearance::None);

}

void
CopyPasteManager::UpdateCaretsForSelectionMode()
{
  CP_LOGV("%s, selection: %p", __FUNCTION__, GetSelection());

  int32_t startOffset = 0;
  nsIFrame* startFrame = FindFirstNodeWithFrame(false, &startOffset);

  int32_t endOffset = 0;
  nsIFrame* endFrame = FindFirstNodeWithFrame(true, &endOffset);

  if(!startFrame || !endFrame ||
     nsLayoutUtils::CompareTreePosition(startFrame, endFrame) > 0) {
    HideCarets();
    return;
  }

  if ((startFrame->GetStateBits() & NS_FRAME_IS_DIRTY) ||
      (endFrame->GetStateBits() & NS_FRAME_IS_DIRTY)) {
    mPresShell->FlushPendingNotifications(Flush_Layout);
  }

  PositionChangedResult firstCaretResult =
    mFirstCaret->SetPosition(startFrame, startOffset);
  PositionChangedResult secondCaretResult =
    mSecondCaret->SetPosition(endFrame, endOffset);

  // XXX: Let's revise this duplicate code later.
  if (firstCaretResult == PositionChangedResult::Invisible) {
    mFirstCaret->SetAppearance(Appearance::NormalNotShown);
  } else if (firstCaretResult == PositionChangedResult::Changed) {
    mFirstCaret->SetAppearance(Appearance::Normal);
  }

  if (secondCaretResult == PositionChangedResult::Invisible) {
    mSecondCaret->SetAppearance(Appearance::NormalNotShown);
  } else if (secondCaretResult == PositionChangedResult::Changed) {
    mSecondCaret->SetAppearance(Appearance::Normal);
  }

  if (firstCaretResult == PositionChangedResult::Changed ||
      secondCaretResult == PositionChangedResult::Changed) {
    // Flush layout to make the carets intersection correct.
    mPresShell->FlushPendingNotifications(Flush_Layout);
  }

  if (mFirstCaret->IsVisible() && mSecondCaret->IsVisible()) {
    if (mFirstCaret->Intersects(*mSecondCaret)) {
      if (mFirstCaret->LogicalPosition().x <=
          mSecondCaret->LogicalPosition().x) {
        mFirstCaret->SetAppearance(Appearance::Left);
        mSecondCaret->SetAppearance(Appearance::Right);
      } else {
        mFirstCaret->SetAppearance(Appearance::Right);
        mSecondCaret->SetAppearance(Appearance::Left);
      }
    } else {
      mFirstCaret->SetAppearance(Appearance::Normal);
      mSecondCaret->SetAppearance(Appearance::Normal);
    }
  }
}

nsresult
CopyPasteManager::PressCaret(const nsPoint& aPoint)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mFirstCaret->Contains(aPoint)) {
    mActiveCaret = mFirstCaret.get();
    SetSelectionDirection(eDirPrevious);
  } else if (mSecondCaret->Contains(aPoint)) {
    mActiveCaret = mSecondCaret.get();
    SetSelectionDirection(eDirNext);
  }

  if (mActiveCaret) {
    mOffsetYToCaretLogicalPosition =
      mActiveCaret->LogicalPosition().y - aPoint.y;
    SetSelectionDragState(true);
    CancelTimeoutTimer();
    rv = NS_OK;
  }

  return rv;
}

nsresult
CopyPasteManager::DragCaret(const nsPoint& aPoint)
{
  MOZ_ASSERT(mActiveCaret);
  MOZ_ASSERT(GetCaretMode() != CaretMode::None);

  nsPoint point(aPoint.x, aPoint.y + mOffsetYToCaretLogicalPosition);
  DragCaretInternal(point);
  UpdateCarets();
  return NS_OK;
}

nsresult
CopyPasteManager::ReleaseCaret()
{
  MOZ_ASSERT(mActiveCaret);

  mActiveCaret = nullptr;
  SetSelectionDragState(false);
  LaunchTimeoutTimer();
  return NS_OK;
}

nsresult
CopyPasteManager::TapCaret(const nsPoint& aPoint)
{
  MOZ_ASSERT(GetCaretMode() != CaretMode::None);

  nsresult rv = NS_ERROR_FAILURE;

  if (GetCaretMode() == CaretMode::Cursor) {
    rv = NS_OK;
  }

  return rv;
}

nsresult
CopyPasteManager::SelectWordOrShortcut(const nsPoint& aPoint)
{
  if (!mPresShell) {
    return NS_ERROR_UNEXPECTED;
  }

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  if (!rootFrame) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Find content offsets for mouse down point
  nsIFrame* ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, aPoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return NS_ERROR_FAILURE;
  }

  bool selectable;
  ptFrame->IsSelectable(&selectable, nullptr);
  if (!selectable) {
    return NS_ERROR_FAILURE;
  }

  nsPoint ptInFrame = aPoint;
  nsLayoutUtils::TransformPoint(rootFrame, ptFrame, ptInFrame);

  nsIContent* editingHost = ptFrame->GetContent()->GetEditingHost();
  if (ChangeFocus(ptFrame) &&
      (editingHost && !nsContentUtils::HasNonEmptyTextContent(
        editingHost, nsContentUtils::eRecurseIntoChildren))) {
    // Content is empty. No need to select word.
    CP_LOG("%s, Cannot select word bacause content is empty", __FUNCTION__);
    return NS_OK;
  }

  nsresult rv = SelectWord(ptFrame, ptInFrame);
  UpdateCarets();
  return rv;
}

void
CopyPasteManager::OnScrollStart()
{
  CP_LOG("%s", __FUNCTION__);

  HideCarets();
}

void
CopyPasteManager::OnScrollEnd()
{
  CP_LOG("%s", __FUNCTION__);

  if (mCaretMode != GetCaretMode()) {
    return;
  }

  if (GetCaretMode() == CaretMode::Cursor) {
    HideCarets();
  } else {
    UpdateCarets();
  }
}

void
CopyPasteManager::OnScrolling()
{
  CP_LOG("%s", __FUNCTION__);

  if (mCaretMode != GetCaretMode()) {
    return;
  }

  if (GetCaretMode() == CaretMode::Cursor) {
    HideCarets();
  } else {
    UpdateCarets();
  }
}

void
CopyPasteManager::OnScrollPositionChanged()
{
  CP_LOG("%s", __FUNCTION__);

  if (mCaretMode != GetCaretMode()) {
    return;
  }

  UpdateCarets();
}

void
CopyPasteManager::OnReflow()
{
  CP_LOG("%s", __FUNCTION__);

  if (mCaretMode != GetCaretMode()) {
    return;
  }

  if (mFirstCaret->IsVisible() || mSecondCaret->IsVisible()) {
    UpdateCarets();
  }
}

void
CopyPasteManager::OnBlur()
{
  CP_LOG("%s", __FUNCTION__);

  HideCarets();
}

void
CopyPasteManager::OnKeyboardEvent()
{
  CP_LOG("%s", __FUNCTION__);
  if (GetCaretMode() == CaretMode::Cursor) {
    HideCarets();
  }
}

nsIContent*
CopyPasteManager::GetFocusedContent()
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  MOZ_ASSERT(fm);
  return fm->GetFocusedContent();
}

Selection*
CopyPasteManager::GetSelection()
{
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return nullptr;
  }
  return fs->GetSelection(nsISelectionController::SELECTION_NORMAL);
}

already_AddRefed<nsFrameSelection>
CopyPasteManager::GetFrameSelection()
{
  nsIContent* focusedContent = GetFocusedContent();
  if (focusedContent) {
    nsIFrame* focusFrame = focusedContent->GetPrimaryFrame();
    if (!focusFrame) {
      return nullptr;
    }

    // Prevent us from touching the nsFrameSelection associated with other
    // PresShell.
    nsRefPtr<nsFrameSelection> fs = focusFrame->GetFrameSelection();
    if (!fs || fs->GetShell() != mPresShell) {
      return nullptr;
    }

    return fs.forget();
  } else {
    // For non-editable content
    return mPresShell->FrameSelection();
  }
}

CopyPasteManager::CaretMode
CopyPasteManager::GetCaretMode()
{
  Selection* selection = GetSelection();
  if (!selection) {
    return CaretMode::None;
  }

  uint32_t rangeCount = selection->RangeCount();
  if (rangeCount <= 0) {
    return CaretMode::None;
  }

  if (selection->IsCollapsed()) {
    return CaretMode::Cursor;
  }

  return CaretMode::Selection;
}

bool
CopyPasteManager::ChangeFocus(nsIFrame* aFrame)
{
  nsIFrame* currFrame = aFrame;
  nsIContent* newFocusContent = nullptr;
  while (currFrame) {
    int32_t tabIndexUnused = 0;
    if (currFrame->IsFocusable(&tabIndexUnused, true)) {
      newFocusContent = currFrame->GetContent();
      nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(newFocusContent));
      if (domElement)
        break;
    }
    currFrame = currFrame->GetParent();
  }

  // If target frame is focusable, we should move focus to it. If target frame
  // isn't focusable, and our previous focused content is editable, we should
  // clear focus.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (newFocusContent && currFrame) {
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(newFocusContent));
    fm->SetFocus(domElement, 0);
  } else {
    nsIContent* focusedContent = GetFocusedContent();
    if (focusedContent && focusedContent->GetTextEditorRootContent()) {
      nsIDOMWindow* win = mPresShell->GetDocument()->GetWindow();
      if (win) {
        fm->ClearFocus(win);
      }
    }
  }

  return (newFocusContent && currFrame);
}

nsresult
CopyPasteManager::SelectWord(nsIFrame* aFrame, const nsPoint& aPoint)
{
  SetSelectionDragState(true);
  nsFrame* frame = static_cast<nsFrame*>(aFrame);
  nsresult rs = frame->SelectByTypeAtPoint(mPresShell->GetPresContext(), aPoint,
                                           eSelectWord, eSelectWord, 0);

#ifdef DEBUG_FRAME_DUMP
  nsCString frameTag;
  frame->ListTag(frameTag);
  CP_LOG("Frame=%s, ptInFrame=(%d, %d)", frameTag.get(), aPoint.x,
         aPoint.y);
#endif

  SetSelectionDragState(false);

  // Clear maintain selection otherwise we cannot select less than a word
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->MaintainSelection(eSelectNoAmount);
  }
  return rs;
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
CopyPasteManager::SetSelectionDirection(nsDirection aDir)
{
  Selection* selection = GetSelection();
  if (selection) {
    selection->SetDirection(aDir);
  }
}

nsIFrame*
CopyPasteManager::FindFirstNodeWithFrame(bool aBackward, int32_t* aOutOffset)
{
  if (!mPresShell) {
    return nullptr;
  }

  nsRefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return nullptr;
  }

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return nullptr;
  }

  uint32_t rangeCount = selection->RangeCount();
  if (rangeCount <= 0) {
    return nullptr;
  }

  nsRange* range = selection->GetRangeAt(aBackward ? rangeCount - 1 : 0);
  nsRefPtr<nsINode> startNode =
    aBackward ? range->GetEndParent() : range->GetStartParent();
  nsRefPtr<nsINode> endNode =
    aBackward ? range->GetStartParent() : range->GetEndParent();
  int32_t offset = aBackward ? range->EndOffset() : range->StartOffset();
  nsIContent* startContent = startNode->AsContent();
  CaretAssociationHint hintStart =
    aBackward ? CARET_ASSOCIATE_BEFORE : CARET_ASSOCIATE_AFTER;
  nsIFrame* startFrame =
    fs->GetFrameForNodeOffset(startContent, offset, hintStart, aOutOffset);

  if (startFrame) {
    return startFrame;
  }

  ErrorResult err;
  nsRefPtr<TreeWalker> walker =
    mPresShell->GetDocument()->CreateTreeWalker(*startNode,
                                                nsIDOMNodeFilter::SHOW_ALL,
                                                nullptr,
                                                err);

  if (!walker) {
    return nullptr;
  }

  startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
  while (!startFrame && startNode != endNode) {
    startNode = aBackward ? walker->PreviousNode(err) : walker->NextNode(err);

    if (!startNode) {
      break;
    }

    startContent = startNode->AsContent();
    startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
  }
  return startFrame;
}

bool
CopyPasteManager::CompareRangeWithContentOffset(nsIFrame::ContentOffsets& aOffsets)
{
  Selection* selection = GetSelection();
  if (!selection) {
    return false;
  }

  uint32_t rangeCount = selection->RangeCount();
  MOZ_ASSERT(rangeCount > 0);

  int32_t rangeIndex = (mActiveCaret == mFirstCaret.get() ? rangeCount - 1 : 0);
  nsRefPtr<nsRange> range = selection->GetRangeAt(rangeIndex);

  nsINode* node = nullptr;
  int32_t nodeOffset = 0;
  CaretAssociationHint hint;
  nsDirection dir;

  if (mActiveCaret == mFirstCaret.get()) {
    // Check previous character of end node offset
    node = range->GetEndParent();
    nodeOffset = range->EndOffset();
    hint = CARET_ASSOCIATE_BEFORE;
    dir = eDirPrevious;
  } else {
    // Check next character of start node offset
    node = range->GetStartParent();
    nodeOffset = range->StartOffset();
    hint =  CARET_ASSOCIATE_AFTER;
    dir = eDirNext;
  }
  nsCOMPtr<nsIContent> content = do_QueryInterface(node);

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return false;
  }

  int32_t offset = 0;
  nsIFrame* theFrame =
    fs->GetFrameForNodeOffset(content, nodeOffset, hint, &offset);

  if (!theFrame) {
    return false;
  }

  // Move one character forward/backward from point and get offset
  nsPeekOffsetStruct pos(eSelectCluster,
                         dir,
                         offset,
                         nsPoint(0, 0),
                         true,
                         true,  //limit on scrolled views
                         false,
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
  if ((mActiveCaret == mFirstCaret.get() && result == 1) ||
      (mActiveCaret == mSecondCaret.get() && result == -1)) {
    aOffsets.content = pos.mResultContent;
    aOffsets.offset = pos.mContentOffset;
    aOffsets.secondaryOffset = pos.mContentOffset;
  }

  return true;
}

nsresult
CopyPasteManager::DragCaretInternal(const nsPoint& aPoint)
{
  if (!mPresShell) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  if (!rootFrame) {
    return NS_ERROR_NULL_POINTER;
  }

  nsPoint point = AdjustDragBoundary(aPoint);

  // Find out which content we point to
  nsIFrame* ptFrame = nsLayoutUtils::GetFrameForPoint(
    rootFrame, point,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult result;
  nsIFrame* newFrame = nullptr;
  nsPoint newPoint;
  nsPoint ptInFrame = point;
  nsLayoutUtils::TransformPoint(rootFrame, ptFrame, ptInFrame);
  result = fs->ConstrainFrameAndPointToAnchorSubtree(ptFrame, ptInFrame,
                                                     &newFrame, newPoint);
  if (NS_FAILED(result) || !newFrame) {
    return NS_ERROR_FAILURE;
  }

  bool selectable;
  newFrame->IsSelectable(&selectable, nullptr);
  if (!selectable) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame::ContentOffsets offsets =
    newFrame->GetContentOffsetsFromPoint(newPoint);
  if (!offsets.content) {
    return NS_ERROR_FAILURE;
  }

  Selection* selection = GetSelection();
  if (!selection) {
    return NS_ERROR_NULL_POINTER;
  }

  if (GetCaretMode() == CaretMode::Selection &&
      !CompareRangeWithContentOffset(offsets)) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame* anchorFrame;
  selection->GetPrimaryFrameForAnchorNode(&anchorFrame);
  if (!anchorFrame) {
    return NS_ERROR_FAILURE;
  }

  // Clear maintain selection so that we can drag caret freely.
  fs->MaintainSelection(eSelectNoAmount);

  // Change the range on the opposite side where the other caret is attached as
  // generated so that AutoPrepareFocusRange could figure out the correct anchor
  // focus range.
  uint32_t rangeCount = selection->RangeCount();
  if (rangeCount >= 2) {
    uint32_t rangeIndex = (mActiveCaret == mFirstCaret.get() ? rangeCount - 1 : 0);
    nsRange* range = selection->GetRangeAt(rangeIndex);
    range->SetIsGenerated(true);
  }

  // Move caret position.
  nsIFrame* scrollable =
    nsLayoutUtils::GetClosestFrameOfType(anchorFrame, nsGkAtoms::scrollFrame);
  nsWeakFrame weakScrollable = scrollable;
  fs->HandleClick(offsets.content, offsets.StartOffset(),
                  offsets.EndOffset(),
                  GetCaretMode() == CaretMode::Selection,
                  false,
                  offsets.associate);
  if (!weakScrollable.IsAlive()) {
    return NS_ERROR_FAILURE;
  }

  // Scroll scrolled frame.
  nsIScrollableFrame* saf = do_QueryFrame(scrollable);
  nsIFrame* capturingFrame = saf->GetScrolledFrame();
  nsPoint ptInScrolled = point;
  nsLayoutUtils::TransformPoint(rootFrame, capturingFrame, ptInScrolled);
  fs->StartAutoScrollTimer(capturingFrame, ptInScrolled, kAutoScrollTimerDelay);
  return NS_OK;
}

nsPoint
CopyPasteManager::AdjustDragBoundary(const nsPoint& aPoint)
{
  // Bug 1068474: Adjust the Y-coordinate so that the carets won't be in tilt
  // mode when a caret is being dragged surpass the other caret.
  //
  // For example, when dragging the second caret, the horizontal boundary (lower
  // bound) of its Y-coordinate is the logical position of the first caret.
  // Likewise, when dragging the first caret, the horizontal boundary (upper
  // bound) of its Y-coordinate is the logical position of the second caret.
  nsPoint adjustedPoint = aPoint;

  if (GetCaretMode() == CaretMode::Selection) {
    if (mActiveCaret == mFirstCaret.get()) {
      nscoord dragDownBoundaryY = mSecondCaret->LogicalPosition().y;
      if (adjustedPoint.y > dragDownBoundaryY) {
        adjustedPoint.y = dragDownBoundaryY;
      }
    } else {
      nscoord dragUpBoundaryY = mFirstCaret->LogicalPosition().y;
      if (adjustedPoint.y < dragUpBoundaryY) {
        adjustedPoint.y = dragUpBoundaryY;
      }
    }
  }

  return adjustedPoint;
}

uint32_t
CopyPasteManager::CaretTimeoutMs()
{
  static bool added = false;
  static uint32_t caretTimeoutMs = 0;

  if (!added) {
    Preferences::AddUintVarCache(&caretTimeoutMs,
                                 "layout.accessiblecaret.timeout_ms");
    added = true;
  }

  return caretTimeoutMs;
}

void
CopyPasteManager::LaunchTimeoutTimer()
{
  if (!mCaretTimeoutTimer || CaretTimeoutMs() == 0 ||
      GetCaretMode() != CaretMode::Cursor || mActiveCaret) {
    return;
  }

  nsTimerCallbackFunc callback = [](nsITimer* aTimer, void* aClosure) {
    CopyPasteManager* self = static_cast<CopyPasteManager*>(aClosure);
    if (self->GetCaretMode() == CaretMode::Cursor) {
      self->HideCarets();
    }
  };

  mCaretTimeoutTimer->InitWithFuncCallback(callback, this, CaretTimeoutMs(),
                                           nsITimer::TYPE_ONE_SHOT);
}

void
CopyPasteManager::CancelTimeoutTimer()
{
  if (mCaretTimeoutTimer) {
    mCaretTimeoutTimer->Cancel();
  }
}

} // namespace mozilla
