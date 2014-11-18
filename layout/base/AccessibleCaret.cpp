/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleCaret.h"

#include "nsCanvasFrame.h"
#include "nsCaret.h"
#include "nsDOMTokenList.h"
#include "nsIFrame.h"
#include "mozilla/dom/AnonymousContent.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;

static int32_t gAccessibleCaretInflateSize = 0;

NS_IMPL_ISUPPORTS0(AccessibleCaret)

AccessibleCaret::AccessibleCaret(nsIPresShell* aPresShell)
  : mHasInjected(false)
  , mVisible(false)
  , mPresShell(aPresShell)
{
  MOZ_ASSERT(NS_IsMainThread());

  // XXX: rename
  /* static bool addedPref = false; */
  /* if (!addedPref) { */
  /*   Preferences::AddIntVarCache(&gAccessibleCaretInflateSize, */
  /*                               "selectioncaret.inflatesize.threshold"); */
  /*   addedPref = true; */
  /* } */
}

AccessibleCaret::~AccessibleCaret()
{
  MOZ_ASSERT(NS_IsMainThread());
  mPresShell = nullptr;
}

void
AccessibleCaret::SetVisibility(bool aVisible)
{
  if (mVisible == aVisible) {
    return;
  }

  if (aVisible) {
    MaybeInjectAnonymousContent();
  }

  mVisible = aVisible;

  if (mAnonymousContent) {
    ErrorResult err;
    nsCOMPtr<Element> element = mAnonymousContent->GetContentNode();
    element->ClassList()->Toggle(NS_LITERAL_STRING("hidden"),
                                 dom::Optional<bool>(!mVisible), err);
  }
}

void
AccessibleCaret::MaybeInjectAnonymousContent()
{
  if (mHasInjected) {
    return;
  }

  nsIDocument* document = mPresShell->GetDocument();
  if (document) {
    ErrorResult rv;
    nsCOMPtr<Element> element = document->CreateHTMLElement(nsGkAtoms::div);
    nsCOMPtr<Element> elementInner = document->CreateHTMLElement(nsGkAtoms::div);
    element->AppendChildTo(elementInner, false);
    element->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                     NS_LITERAL_STRING("moz-selectioncaret-left hidden"),
                     true);
    mAnonymousContent = document->InsertAnonymousContent(*element, rv);
    if (!rv.Failed() && mAnonymousContent) {
      mHasInjected = true;
    }
  }
}

void
AccessibleCaret::SetPositionBasedOnFrameOffset(nsIFrame* aFrame, int32_t aOffset)
{
  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  nsIFrame* rootFrame = mPresShell->GetRootFrame();

  if (!canvasFrame || !rootFrame) {
    return;
  }

  nsRect rectInRootFrame =
    nsCaret::GetGeometryForFrame(aFrame, aOffset, nullptr);

  // GetGeometryForFrame may return a rect that outside frame's rect. So
  // constrain rect inside frame's rect.
  rectInRootFrame = rectInRootFrame.ForceInside(aFrame->GetRectRelativeToSelf());
  nsRect rectInCanvasFrame = rectInRootFrame;
  nsLayoutUtils::TransformRect(aFrame, rootFrame, rectInRootFrame);
  nsLayoutUtils::TransformRect(aFrame, canvasFrame, rectInCanvasFrame);

  rectInRootFrame.Inflate(AppUnitsPerCSSPixel(), 0);

  nsAutoTArray<nsIFrame*, 16> hitFramesInRect;
  nsLayoutUtils::GetFramesForArea(rootFrame,
    rectInRootFrame,
    hitFramesInRect,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION |
      nsLayoutUtils::IGNORE_CROSS_DOC |
      nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME);

  SetVisibility(true);
  /* SetVisibility(hitFramesInRect.Contains(aFrame)); */
  SetPosition(rectInCanvasFrame.BottomLeft());
}

void
AccessibleCaret::SetPosition(const nsPoint& aPosition)
{
  if (!mAnonymousContent) {
    return;
  }

  nsAutoString styleStr;
  styleStr.AppendLiteral("left: ");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(aPosition.x));
  styleStr.AppendLiteral("px; top: ");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(aPosition.y));
  styleStr.AppendLiteral("px;");

  nsCOMPtr<Element> element = mAnonymousContent->GetContentNode();
  element->SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleStr, true);
}
