/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteManager.h"

#include "AccessibleCaret.h"
#include "CopyPasteManagerGlue.h"

using namespace mozilla;
using namespace mozilla::dom;
using Appearance = AccessibleCaret::Appearance;

NS_IMPL_ISUPPORTS(CopyPasteManager, nsISelectionListener)

CopyPasteManager::CopyPasteManager()
  : mHasInited(false)
  , mDragMode(DragMode::NONE)
  , mCaretMode(CaretMode::NONE)
  , mCaretCenterToDownPointOffsetY(0)
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
    mCopyPasteEventHub = new CopyPasteEventHub(aPresShell, this);
    mCopyPasteEventHub->Init();
    mHasInited = true;
  }
}

void
CopyPasteManager::Terminate()
{
  mCopyPasteEventHub->Terminate();
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

  return mCopyPasteEventHub->HandleEvent(aEvent);
}

nsresult
CopyPasteManager::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                         nsISelection* aSel,
                                         int16_t aReason)
{
  if (!mHasInited) {
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
    mCaretCenterToDownPointOffsetY = mFirstCaret->GetFrameOffsetRect().Center().y - aPoint.y;
    mGlue->SetSelectionDirection(false);
    mGlue->SetSelectionDragState(true);
    return nsEventStatus_eConsumeNoDefault;
  } else if (mSecondCaret->Contains(aPoint)) {
    mDragMode = DragMode::SECOND_CARET;
    mCaretCenterToDownPointOffsetY = mSecondCaret->GetFrameOffsetRect().Center().y - aPoint.y;
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
    UpdateCarets();
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

