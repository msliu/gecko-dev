/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteManager.h"

#include "AccessibleCaret.h"
#include "CopyPasteEventHub.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/TreeWalker.h"
#include "nsFocusManager.h"
#include "nsFrame.h"
#include "nsFrameSelection.h"

using namespace mozilla;
using namespace mozilla::dom;

typedef AccessibleCaret::Appearance Appearance;

NS_IMPL_ISUPPORTS(CopyPasteManager, nsISelectionListener)

CopyPasteManager::CopyPasteManager(nsIPresShell* aPresShell)
  : mInitialized(false)
  , mDragMode(DragMode::NONE)
  , mCaretMode(CaretMode::NONE)
  , mCaretCenterToDownPointOffsetY(0)
  , mPresShell(aPresShell)
{
}

void
CopyPasteManager::Init()
{
  if (!mPresShell->GetCanvasFrame()) {
    return;
  }

  mFirstCaret = MakeUnique<AccessibleCaret>(mPresShell);
  mSecondCaret = MakeUnique<AccessibleCaret>(mPresShell);
  mCopyPasteEventHub = new CopyPasteEventHub(mPresShell, this);
  mCopyPasteEventHub->Init();
  mInitialized = true;
}

void
CopyPasteManager::Terminate()
{
  if (!mInitialized) {
    return;
  }

  mCopyPasteEventHub->Terminate();
}

CopyPasteManager::~CopyPasteManager()
{
}

nsEventStatus
CopyPasteManager::HandleEvent(WidgetEvent* aEvent)
{
  if (!mInitialized) {
    return nsEventStatus_eIgnore;
  }

  return mCopyPasteEventHub->HandleEvent(aEvent);
}

nsresult
CopyPasteManager::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                         nsISelection* aSel,
                                         int16_t aReason)
{
  if (!mInitialized) {
    return NS_OK;
  }

  if (aSel != GetSelection()) {
    return NS_OK;
  }

  if (!aReason) {
    return NS_OK;
  }

  if (aReason & (nsISelectionListener::KEYPRESS_REASON |
                 nsISelectionListener::COLLAPSETOSTART_REASON |
                 nsISelectionListener::COLLAPSETOEND_REASON)) {
    HideCarets();
  } else {
    UpdateCarets();
  }

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
  int32_t startOffset;
  nsIFrame* startFrame = FindFirstNodeWithFrame(false, &startOffset);

  if (!startFrame || !startFrame->GetContent()->IsEditable()) {
    HideCarets();
    return;
  }

  mFirstCaret->SetPosition(startFrame, startOffset);
  mSecondCaret->SetAppearance(Appearance::NONE);
}

void
CopyPasteManager::UpdateCaretsForSelectionMode()
{
  int32_t startOffset;
  nsIFrame* startFrame = FindFirstNodeWithFrame(false, &startOffset);

  int32_t endOffset;
  nsIFrame* endFrame = FindFirstNodeWithFrame(true, &endOffset);

  if(!startFrame || !endFrame ||
     nsLayoutUtils::CompareTreePosition(startFrame, endFrame) > 0) {
    HideCarets();
    return;
  }

  mFirstCaret->SetPosition(startFrame, startOffset);
  mSecondCaret->SetPosition(endFrame, endOffset);

  if (mFirstCaret->Intersects(*mSecondCaret)) {
    mFirstCaret->SetAppearance(Appearance::LEFT);
    mSecondCaret->SetAppearance(Appearance::RIGHT);
  }
}

nsEventStatus
CopyPasteManager::OnPress(const nsPoint& aPoint)
{
  if (mFirstCaret->Contains(aPoint)) {
    mDragMode = DragMode::FIRST_CARET;
    mCaretCenterToDownPointOffsetY = mFirstCaret->LogicalPosition().y - aPoint.y;
    SetSelectionDirection(false);
    SetSelectionDragState(true);
    return nsEventStatus_eConsumeNoDefault;
  } else if (mSecondCaret->Contains(aPoint)) {
    mDragMode = DragMode::SECOND_CARET;
    mCaretCenterToDownPointOffsetY = mSecondCaret->LogicalPosition().y - aPoint.y;
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
    UpdateCarets();
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteManager::OnRelease()
{
  if (mDragMode != DragMode::NONE) {
    SetSelectionDragState(false);
    mDragMode = DragMode::NONE;
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteManager::OnLongTap(const nsPoint& aPoint)
{
  if (mCaretMode != CaretMode::SELECTION) {
    SelectWord(aPoint);
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus
CopyPasteManager::OnTap(const nsPoint& aPoint)
{
  return nsEventStatus_eIgnore;
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
CopyPasteManager::OnReflow()
{
  if (mFirstCaret->IsVisible() || mSecondCaret->IsVisible()) {
    UpdateCarets();
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

  nsRefPtr<nsRange> range = mDragMode == DragMode::FIRST_CARET ?
    selection->GetRangeAt(0) : selection->GetRangeAt(rangeCount - 1);

  nsINode* node = nullptr;
  int32_t nodeOffset = 0;
  CaretAssociationHint hint;
  nsDirection dir;

  if (mDragMode == DragMode::FIRST_CARET) {
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
  if ((mDragMode == DragMode::FIRST_CARET && result == 1) ||
      (mDragMode == DragMode::SECOND_CARET && result == -1)) {
    aOffsets.content = pos.mResultContent;
    aOffsets.offset = pos.mContentOffset;
    aOffsets.secondaryOffset = pos.mContentOffset;
  }

  return true;
}

nsEventStatus
CopyPasteManager::DragCaret(const nsPoint &aMovePoint)
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

