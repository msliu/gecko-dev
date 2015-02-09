/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteManager.h"

#include "AccessibleCaret.h"
#include "CopyPasteLogger.h"
#include "CopyPasteEventHub.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/TreeWalker.h"
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
typedef AccessibleCaret::Appearance Appearance;

/* static */ const char*
CopyPasteManager::ToStr(CaretMode aCaretMode)
{
  switch(aCaretMode) {
  case CaretMode::NONE: return "CaretMode::NONE";
  case CaretMode::CURSOR: return "CaretMode::CURSOR";
  case CaretMode::SELECTION: return "CaretMode::SELECTION";
  default: return "";
  }
}

CopyPasteManager::CopyPasteManager(nsIPresShell* aPresShell)
  : mCaretMode(CaretMode::NONE)
  , mOffsetYToCaretLogicalPosition(0)
  , mPresShell(aPresShell)
  , mActiveCaret(nullptr)
{
  if (mPresShell) {
    mFirstCaret = MakeUnique<AccessibleCaret>(mPresShell);
    mSecondCaret = MakeUnique<AccessibleCaret>(mPresShell);
  }
}

CopyPasteManager::~CopyPasteManager()
{
}

nsresult
CopyPasteManager::OnSelectionChanged(nsIDOMDocument* aDoc,
                                     nsISelection* aSel,
                                     int16_t aReason)
{
  CP_LOG("aSel: %p, GetSelection(): %p", aSel, GetSelection());

  if (aSel != GetSelection()) {
    return NS_OK;
  }

  if (!aReason) {
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
  mCaretMode = CaretMode::NONE;
}

void
CopyPasteManager::UpdateCarets()
{
  int32_t rangeCount = SelectionRangeCount();
  if (rangeCount <= 0) {
    HideCarets();
    return;
  }

  if (SelectionIsCollapsed()) {
    mCaretMode = CaretMode::CURSOR;
    UpdateCaretsForCursorMode();
  } else {
    mCaretMode = CaretMode::SELECTION;
    UpdateCaretsForSelectionMode();
  }
}

void
CopyPasteManager::UpdateCaretsForCursorMode()
{
  CP_LOGV("%s, selection: %p", __FUNCTION__, GetSelection());

  int32_t startOffset;
  nsIFrame* startFrame = FindFirstNodeWithFrame(false, &startOffset);

  if (!startFrame || !startFrame->GetContent()->IsEditable()) {
    HideCarets();
    return;
  }

  mFirstCaret->SetPosition(startFrame, startOffset);
  mFirstCaret->SetAppearance(Appearance::Normal);
  mSecondCaret->SetAppearance(Appearance::None);
}

void
CopyPasteManager::UpdateCaretsForSelectionMode()
{
  CP_LOGV("%s, selection: %p", __FUNCTION__, GetSelection());

  int32_t startOffset;
  nsIFrame* startFrame = FindFirstNodeWithFrame(false, &startOffset);

  int32_t endOffset;
  nsIFrame* endFrame = FindFirstNodeWithFrame(true, &endOffset);

  if(!startFrame || !endFrame ||
     nsLayoutUtils::CompareTreePosition(startFrame, endFrame) > 0) {
    HideCarets();
    return;
  }

  nsresult firstCaretPosChanged = mFirstCaret->SetPosition(startFrame, startOffset);
  nsresult secondCaretPosChanged = mSecondCaret->SetPosition(endFrame, endOffset);

  if (NS_SUCCEEDED(firstCaretPosChanged) || NS_SUCCEEDED(secondCaretPosChanged)) {
    // Flush layout to make the carets intersection correct.
    mPresShell->FlushPendingNotifications(Flush_Layout);
  }

  if (mFirstCaret->IsVisible() && mSecondCaret->IsVisible()) {
    if (mFirstCaret->Intersects(*mSecondCaret)) {
      mFirstCaret->SetAppearance(Appearance::Left);
      mSecondCaret->SetAppearance(Appearance::Right);
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
    rv = NS_OK;
  }

  return rv;
}

nsresult
CopyPasteManager::DragCaret(const nsPoint& aPoint)
{
  MOZ_ASSERT(mActiveCaret);

  nsPoint point = aPoint;
  point.y += mOffsetYToCaretLogicalPosition;
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
  return NS_OK;
}

nsresult
CopyPasteManager::TapCaret(const nsPoint& aPoint)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mCaretMode == CaretMode::CURSOR && mActiveCaret == mFirstCaret.get()) {
    rv = NS_OK;
  }

  return rv;
}

nsresult
CopyPasteManager::SelectWordOrShortcut(const nsPoint& aPoint)
{
  // TODO: Handle shortcut mode
  nsresult rv = SelectWord(aPoint);
  UpdateCarets();
  return rv;
}

void
CopyPasteManager::OnScrollStart()
{
  HideCarets();
}

void
CopyPasteManager::OnScrollEnd()
{
  UpdateCarets();
}

void
CopyPasteManager::OnScrolling()
{
  if (mFirstCaret->IsVisible() || mSecondCaret->IsVisible()) {
    UpdateCarets();
  }
}

void
CopyPasteManager::OnReflow()
{
  if (mFirstCaret->IsVisible() || mSecondCaret->IsVisible()) {
    UpdateCarets();
  }
}

void
CopyPasteManager::OnBlur()
{
  HideCarets();
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

    // Prevent us from touching the nsFrameSelection associated to other
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

bool
CopyPasteManager::SelectionIsCollapsed()
{
  Selection* sel = GetSelection();
  if (!sel) {
    return true;
  }

  return sel->IsCollapsed();
}

int32_t
CopyPasteManager::SelectionRangeCount()
{
  Selection* sel = GetSelection();
  if (!sel) {
    return 0;
  }

  return sel->GetRangeCount();
}

nsresult
CopyPasteManager::SelectWord(const nsPoint& aPoint)
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

  nsIFrame* currFrame = ptFrame;
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

  SetSelectionDragState(true);
  nsFrame* frame = static_cast<nsFrame*>(ptFrame);
  nsresult rs = frame->SelectByTypeAtPoint(mPresShell->GetPresContext(), ptInFrame,
                                           eSelectWord, eSelectWord, 0);

#ifdef DEBUG_FRAME_DUMP
  nsCString frameTag;
  frame->ListTag(frameTag);
  CP_LOG("Frame=%s, ptInFrame=(%d, %d)", frameTag.get(), ptInFrame.x,
         ptInFrame.y);
#endif

  SetSelectionDragState(false);

  // Clear maintain selection otherwise we cannot select less than a word
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->MaintainSelection();
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
  nsRefPtr<dom::Selection> selection = GetSelection();
  if (selection) {
    selection->SetDirection(aDir);
  }
}

nsIFrame*
CopyPasteManager::FindFirstNodeWithFrame(bool aBackward, int* aOutOffset)
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

  int32_t rangeCount = selection->GetRangeCount();
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
  nsRefPtr<dom::Selection> selection = GetSelection();
  if (!selection) {
    return nsEventStatus_eConsumeNoDefault;
  }

  int32_t rangeCount = selection->GetRangeCount();
  MOZ_ASSERT(rangeCount > 0);

  int32_t rangeIndex = (mActiveCaret == mFirstCaret.get() ? 0 : rangeCount - 1);
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

nsEventStatus
CopyPasteManager::DragCaretInternal(const nsPoint &aMovePoint)
{
  if (!mPresShell) {
    return nsEventStatus_eConsumeNoDefault;
  }

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

  if (mCaretMode == CaretMode::SELECTION &&
      !CompareRangeWithContentOffset(offsets)) {
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
  fs->StartAutoScrollTimer(capturingFrame, ptInScrolled, kAutoScrollTimerDelay);
  return nsEventStatus_eConsumeNoDefault;
}

} // namespace mozilla
