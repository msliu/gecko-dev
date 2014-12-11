/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

NS_IMPL_ISUPPORTS0(AccessibleCaret)

AccessibleCaret::AccessibleCaret(nsIPresShell* aPresShell)
  : mAppearance(Appearance::NONE)
  , mPresShell(aPresShell)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Check all resources required.
  MOZ_ASSERT(mPresShell);
  MOZ_ASSERT(mPresShell->GetRootFrame());
  MOZ_ASSERT(mPresShell->GetDocument());
  MOZ_ASSERT(mPresShell->GetCanvasFrame());
  MOZ_ASSERT(mPresShell->GetCanvasFrame()->GetCustomContentContainer());

  mCaretElementHolder = InjectCaretElement(mPresShell->GetDocument());
}

AccessibleCaret::~AccessibleCaret()
{
  MOZ_ASSERT(NS_IsMainThread());
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

  mAppearance = aAppearance;

  ErrorResult rv;
  CaretElement()->SetAttribute(NS_LITERAL_STRING("class"),
                               AppearanceString(aAppearance), rv);
  MOZ_ASSERT(!rv.Failed());
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

Element*
AccessibleCaret::CaretElement() const
{
  return mCaretElementHolder->GetContentNode();
}

Element*
AccessibleCaret::CaretElementInner() const
{
  return CaretElement()->GetFirstElementChild();
}

bool
AccessibleCaret::Intersects(const AccessibleCaret& aCaret)
{
  MOZ_ASSERT(mPresShell == aCaret.mPresShell);

  if (!IsVisible() || !aCaret.IsVisible()) {
    return false;
  }

  nsRect rect = GetRectRelativeToRootFrame(CaretElement());
  nsRect rhsRect = GetRectRelativeToRootFrame(aCaret.CaretElement());
  return rect.Intersects(rhsRect);
}

bool
AccessibleCaret::Contains(const nsPoint& aPosition)
{
  if (!IsVisible()) {
    return false;
  }

  nsRect rect = GetRectRelativeToRootFrame(CaretElementInner());
  return rect.Contains(aPosition);
}

nsRect
AccessibleCaret::GetRectRelativeToRootFrame(Element* aElement)
{
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  return nsLayoutUtils::GetRectRelativeToFrame(aElement, rootFrame);
}

/* static */ already_AddRefed<AnonymousContent>
AccessibleCaret::InjectCaretElement(nsIDocument* aDocument)
{
  ErrorResult rv;
  nsCOMPtr<Element> element = CreateCaretElement(aDocument);
  nsRefPtr<AnonymousContent> anonymousContent =
    aDocument->InsertAnonymousContent(*element, rv);

  MOZ_ASSERT(!rv.Failed(), "Insert anonymous content should not fail!");
  MOZ_ASSERT(anonymousContent, "We must have anonymous content!");

  return anonymousContent.forget();
}

/* static */ already_AddRefed<Element>
AccessibleCaret::CreateCaretElement(nsIDocument* aDocument)
{
  ErrorResult rv;
  nsCOMPtr<Element> element = aDocument->CreateHTMLElement(nsGkAtoms::div);
  nsCOMPtr<Element> elementInner = aDocument->CreateHTMLElement(nsGkAtoms::div);
  element->AppendChildTo(elementInner, false);
  element->SetAttribute(NS_LITERAL_STRING("class"),
                        AppearanceString(Appearance::NONE), rv);
  MOZ_ASSERT(!rv.Failed());
  return element.forget();
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

  nsPoint caretElementPositionRelativeToContainerFrame = CaretElementPosition();
    nsLayoutUtils::TransformPoint(rootFrame, containerFrame,
                                  caretElementPositionRelativeToContainerFrame);
  SetPosition(caretElementPositionRelativeToContainerFrame);
}

nsPoint
AccessibleCaret::LogicalPosition() const
{
  return mFrameOffsetRect.Center();
}

nsPoint
AccessibleCaret::CaretElementPosition() const
{
  return mFrameOffsetRect.TopLeft()
    + nsPoint(mFrameOffsetRect.width/2, mFrameOffsetRect.height);
}

void
AccessibleCaret::SetPosition(const nsPoint& aPosition)
{
  nsAutoString styleStr;
  styleStr.AppendPrintf("left: %dpx; top: %dpx;",
                        nsPresContext::AppUnitsToIntCSSPixels(aPosition.x),
                        nsPresContext::AppUnitsToIntCSSPixels(aPosition.y));

  ErrorResult rv;
  CaretElement()->SetAttribute(NS_LITERAL_STRING("style"), styleStr, rv);
  MOZ_ASSERT(!rv.Failed());
}
