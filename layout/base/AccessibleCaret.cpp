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
  , mAppearance(Appearance::NONE)
  , mPresShell(aPresShell)
{
  MOZ_ASSERT(NS_IsMainThread());

  // XXX: rename
  static bool addedPref = false;
  if (!addedPref) {
    Preferences::AddIntVarCache(&gAccessibleCaretInflateSize,
                                "selectioncaret.inflatesize.threshold");
    addedPref = true;
  }
}

AccessibleCaret::~AccessibleCaret()
{
  MOZ_ASSERT(NS_IsMainThread());
  mPresShell = nullptr;
}

bool
AccessibleCaret::IsVisible() const
{
  return mAppearance != Appearance::NONE;
}

void
AccessibleCaret::SetAppearance(Appearance aAppearance)
{
  if (mAppearance == aAppearance) {
    return;
  }

  if (IsVisible()) {
    MaybeInjectAnonymousContent();
  }

  mAppearance = aAppearance;

  if (mAnonymousContent) {
    ErrorResult rv;
    nsCOMPtr<Element> element = mAnonymousContent->GetContentNode();

    element->SetAttribute(NS_LITERAL_STRING("class"),
                          AppearanceString(aAppearance), rv);
  }
}

/* static */ nsString
AccessibleCaret::AppearanceString(Appearance aAppearance)
{
  nsAutoString string;
  switch (aAppearance) {
  case Appearance::NONE:
    string = NS_LITERAL_STRING("moz-accessiblecaret none");
    break;
  case Appearance::NORMAL:
    string = NS_LITERAL_STRING("moz-accessiblecaret normal");
    break;
  case Appearance::RIGHT:
    string = NS_LITERAL_STRING("moz-accessiblecaret right");
    break;
  case Appearance::LEFT:
    string = NS_LITERAL_STRING("moz-accessiblecaret left");
    break;
  }
  return string;
}

bool
AccessibleCaret::Intersects(const AccessibleCaret& rhs)
{
  if (!IsVisible() || !rhs.IsVisible()) {
    return false;
  }

  if (!mAnonymousContent || !rhs.mAnonymousContent) {
    return false;
  }

  MOZ_ASSERT(mPresShell == rhs.mPresShell);

  nsCOMPtr<Element> thisElement = mAnonymousContent->GetContentNode();
  nsCOMPtr<Element> rhsElement = rhs.mAnonymousContent->GetContentNode();
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  nsRect thisRect = nsLayoutUtils::GetRectRelativeToFrame(thisElement, rootFrame);
  nsRect rhsRect = nsLayoutUtils::GetRectRelativeToFrame(rhsElement, rootFrame);
  return thisRect.Intersects(rhsRect);
}

bool
AccessibleCaret::Contains(const nsPoint& aPosition)
{
  if (!IsVisible()) {
    return false;
  }

  if (!mAnonymousContent) {
    return false;
  }

  nsCOMPtr<Element> element = mAnonymousContent->GetContentNode();
  Element* childElement = element->GetFirstElementChild();
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  nsRect rect = nsLayoutUtils::GetRectRelativeToFrame(childElement, rootFrame);
  return nsLayoutUtils::ContainsPoint(rect, aPosition, gAccessibleCaretInflateSize);
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
    element->SetAttribute(NS_LITERAL_STRING("class"),
                          AppearanceString(Appearance::NONE), rv);
    mAnonymousContent = document->InsertAnonymousContent(*element, rv);
    if (!rv.Failed() && mAnonymousContent) {
      mHasInjected = true;
    }
  }
}

void
AccessibleCaret::SetPositionBasedOnFrameOffset(nsIFrame* aFrame, int32_t aOffset)
{
  nsCanvasFrame* canvasFrame = mPresShell->GetCanvasFrame();
  nsIFrame* rootFrame = mPresShell->GetRootFrame();

  if (!canvasFrame || !rootFrame) {
    return;
  }

  Element* customContainer = canvasFrame->GetCustomContentContainer();
  MOZ_ASSERT(customContainer);

  nsIFrame* containerFrame = customContainer->GetPrimaryFrame();
  MOZ_ASSERT(containerFrame);

  mFrameOffsetRect = nsCaret::GetGeometryForFrame(aFrame, aOffset, nullptr);

  // GetGeometryForFrame may return a rect that outside frame's rect. So
  // constrain rect inside frame's rect.
  mFrameOffsetRect = mFrameOffsetRect.ForceInside(aFrame->GetRectRelativeToSelf());
  nsRect rectInContainerFrame = mFrameOffsetRect;

  nsLayoutUtils::TransformRect(aFrame, rootFrame, mFrameOffsetRect);
  nsLayoutUtils::TransformRect(aFrame, containerFrame, rectInContainerFrame);

  mFrameOffsetRect.Inflate(AppUnitsPerCSSPixel(), 0);

  nsAutoTArray<nsIFrame*, 16> hitFramesInRect;
  nsLayoutUtils::GetFramesForArea(rootFrame,
    mFrameOffsetRect,
    hitFramesInRect,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION |
      nsLayoutUtils::IGNORE_CROSS_DOC |
      nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME);

  SetAppearance(Appearance::NORMAL);
  /* SetVisibility(hitFramesInRect.Contains(aFrame)); */
  SetPosition(rectInContainerFrame.BottomLeft());
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
