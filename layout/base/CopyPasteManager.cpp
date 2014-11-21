/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyPasteManager.h"

#include "AccessibleCaret.h"
#include "mozilla/dom/TreeWalker.h"
#include "nsFocusManager.h"
#include "nsFrameSelection.h"
#include "nsIDOMNodeFilter.h"

using namespace mozilla;
using namespace mozilla::dom;
using Appearance = AccessibleCaret::Appearance;

NS_IMPL_ISUPPORTS(CopyPasteManager, nsISelectionListener)

CopyPasteManager::CopyPasteManager(nsIPresShell* aPresShell)
  : mPresShell(aPresShell)
  , mFirstCaret(new AccessibleCaret(aPresShell))
  , mSecondCaret(new AccessibleCaret(aPresShell))
{
}

CopyPasteManager::~CopyPasteManager()
{
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
CopyPasteManager::UpdateCarets()
{
  if (!mPresShell) {
    return;
  }

  nsRefPtr<Selection> selection = GetSelection();
  if (!selection) {
    mFirstCaret->SetAppearance(Appearance::NONE);
    mSecondCaret->SetAppearance(Appearance::NONE);
    return;
  }

  if (selection->IsCollapsed()) {
    nsRefPtr<nsRange> firstRange = selection->GetRangeAt(0);

    nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
    if (!fs) {
      mFirstCaret->SetAppearance(Appearance::NONE);
      mSecondCaret->SetAppearance(Appearance::NONE);
      return;
    }

    int32_t startOffset;
    nsIFrame* startFrame = CopyPasteManager::FindFirstNodeWithFrame(mPresShell->GetDocument(),
                                                                    firstRange, fs, false, startOffset);

    if (!startFrame) {
      mFirstCaret->SetAppearance(Appearance::NONE);
      mSecondCaret->SetAppearance(Appearance::NONE);
      return;
    }

    if (!startFrame->GetContent()->IsEditable()) {
      mFirstCaret->SetAppearance(Appearance::NONE);
      mSecondCaret->SetAppearance(Appearance::NONE);
      return;
    }

    mFirstCaret->SetPositionBasedOnFrameOffset(startFrame, startOffset);
    mFirstCaret->SetAppearance(Appearance::NORMAL);
    mSecondCaret->SetAppearance(Appearance::NONE);
  } else {
    int32_t rangeCount = selection->GetRangeCount();
    nsRefPtr<nsRange> firstRange = selection->GetRangeAt(0);
    nsRefPtr<nsRange> lastRange = selection->GetRangeAt(rangeCount - 1);

    nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
    if (!fs) {
      mFirstCaret->SetAppearance(Appearance::NONE);
      mSecondCaret->SetAppearance(Appearance::NONE);
      return;
    }

    int32_t startOffset;
    nsIFrame* startFrame = CopyPasteManager::FindFirstNodeWithFrame(mPresShell->GetDocument(),
                                                                    firstRange, fs, false, startOffset);

    int32_t endOffset;
    nsIFrame* endFrame = CopyPasteManager::FindFirstNodeWithFrame(mPresShell->GetDocument(),
                                                                  lastRange, fs, true, endOffset);

    if (!startFrame || !endFrame) {
      mFirstCaret->SetAppearance(Appearance::NONE);
      mSecondCaret->SetAppearance(Appearance::NONE);
      return;
    }

    // Check if startFrame is after endFrame.
    if (nsLayoutUtils::CompareTreePosition(startFrame, endFrame) > 0) {
      mFirstCaret->SetAppearance(Appearance::NONE);
      mSecondCaret->SetAppearance(Appearance::NONE);
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
  }
}
