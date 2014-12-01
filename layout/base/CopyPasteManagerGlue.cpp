/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteManagerGlue.h"

NS_IMPL_ISUPPORTS0(CopyPasteManagerGlue)

CopyPasteManagerGlue::CopyPasteManagerGlue(nsIPresShell* aPresShell)
  : mPresShell(aPresShell)
{
}

nsIContent*
CopyPasteManagerGlue::GetFocusedContent()
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    return fm->GetFocusedContent();
  }

  return nullptr;
}

Selection*
CopyPasteManagerGlue::GetSelection()
{
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    return fs->GetSelection(nsISelectionController::SELECTION_NORMAL);
  }
  return nullptr;
}

already_AddRefed<nsFrameSelection>
CopyPasteManagerGlue::GetFrameSelection()
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

bool
CopyPasteManagerGlue::GetSelectionIsCollapsed()
{
  Selection* sel = GetSelection();
  if (!sel) {
    return true;
  }

  return sel->IsCollapsed();
}

int32_t
CopyPasteManagerGlue::GetSelectionRangeCount()
{
  Selection* sel = GetSelection();
  if (!sel) {
    return 0;
  }

  return sel->GetRangeCount();
}

nsresult
CopyPasteManagerGlue::SelectWord(const nsPoint& aPoint)
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

  bool selectable;
  ptFrame->IsSelectable(&selectable, nullptr);
  if (!selectable) {
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

void
CopyPasteManagerGlue::SetSelectionDragState(bool aState)
{
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->SetDragState(aState);
  }
}

void
CopyPasteManagerGlue::SetSelectionDirection(bool aForward)
{
  nsRefPtr<dom::Selection> selection = GetSelection();
  if (selection) {
    selection->SetDirection(aForward ? eDirNext : eDirPrevious);
  }
}

nsIFrame*
CopyPasteManagerGlue::FindFirstNodeWithFrame(bool aBackward,
                                             int& aOutOffset)
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
  if (!rangeCount) {
    return nullptr;
  }

  nsRefPtr<nsRange> range = selection->GetRangeAt(aBackward ? rangeCount - 1 : 0);

  nsCOMPtr<nsINode> startNode =
    do_QueryInterface(aBackward ? range->GetEndParent() : range->GetStartParent());
  nsCOMPtr<nsINode> endNode =
    do_QueryInterface(aBackward ? range->GetStartParent() : range->GetEndParent());
  int32_t offset = aBackward ? range->EndOffset() : range->StartOffset();

  nsCOMPtr<nsIContent> startContent = do_QueryInterface(startNode);
  CaretAssociationHint hintStart =
    aBackward ? CARET_ASSOCIATE_BEFORE : CARET_ASSOCIATE_AFTER;
  nsIFrame* startFrame = fs->GetFrameForNodeOffset(startContent,
                                                   offset,
                                                   hintStart,
                                                   &aOutOffset);

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

bool
CopyPasteManagerGlue::CompareRangeWithContentOffset(nsIFrame::ContentOffsets& aOffsets,
                                                    bool aIsBeginRange)
{
  nsRefPtr<dom::Selection> selection = GetSelection();
  if (!selection) {
    return nsEventStatus_eConsumeNoDefault;
  }

  int32_t rangeCount = selection->GetRangeCount();
  MOZ_ASSERT(rangeCount > 0);

  nsRefPtr<nsRange> range = aIsBeginRange ?
    selection->GetRangeAt(0) : selection->GetRangeAt(rangeCount - 1);

  nsINode* node = nullptr;
  int32_t nodeOffset = 0;
  CaretAssociationHint hint;
  nsDirection dir;

  if (aIsBeginRange) {
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
  if ((aIsBeginRange && result == 1) ||
      (!aIsBeginRange && result == -1)) {
    aOffsets.content = pos.mResultContent;
    aOffsets.offset = pos.mContentOffset;
    aOffsets.secondaryOffset = pos.mContentOffset;
  }

  return true;
}

nsEventStatus
CopyPasteManagerGlue::DragCaret(const nsPoint &aMovePoint, bool aIsExtend, bool aIsBeginRange)
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

  if (aIsExtend && !CompareRangeWithContentOffset(offsets, aIsBeginRange)) {
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
                  aIsExtend,
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
  fs->StartAutoScrollTimer(capturingFrame, ptInScrolled, sAutoScrollTimerDelay);
  return nsEventStatus_eConsumeNoDefault;
}

