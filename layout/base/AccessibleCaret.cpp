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

AccessibleCaret::AccessibleCaret(nsIPresShell* aPresShell)
  : mAppearance(Appearance::NONE)
  , mPresShell(aPresShell)
{
  // Check all resources required.
  MOZ_ASSERT(mPresShell);
  MOZ_ASSERT(RootFrame());
  MOZ_ASSERT(mPresShell->GetDocument());
  MOZ_ASSERT(mPresShell->GetCanvasFrame());
  MOZ_ASSERT(mPresShell->GetCanvasFrame()->GetCustomContentContainer());
  MOZ_ASSERT(ElementContainerFrame());

  InjectCaretElement(mPresShell->GetDocument());
}

AccessibleCaret::~AccessibleCaret()
{
  RemoveCaretElement(mPresShell->GetDocument());
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

  nsRect rect = nsLayoutUtils::GetRectRelativeToFrame(CaretElement(), RootFrame());
  nsRect rhsRect = nsLayoutUtils::GetRectRelativeToFrame(aCaret.CaretElement(), RootFrame());
  return rect.Intersects(rhsRect);
}

bool
AccessibleCaret::Contains(const nsPoint& aPosition)
{
  if (!IsVisible()) {
    return false;
  }

  nsRect rect = nsLayoutUtils::GetRectRelativeToFrame(CaretElementInner(), RootFrame());
  return rect.Contains(aPosition);
}

void
AccessibleCaret::InjectCaretElement(nsIDocument* aDocument)
{
  ErrorResult rv;
  nsCOMPtr<Element> element = CreateCaretElement(aDocument);
  mCaretElementHolder = aDocument->InsertAnonymousContent(*element, rv);

  MOZ_ASSERT(!rv.Failed(), "Insert anonymous content should not fail!");
  MOZ_ASSERT(mCaretElementHolder, "We must have anonymous content!");
}

already_AddRefed<Element>
AccessibleCaret::CreateCaretElement(nsIDocument* aDocument) const
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
AccessibleCaret::RemoveCaretElement(nsIDocument* aDocument)
{
  ErrorResult rv;
  aDocument->RemoveAnonymousContent(*mCaretElementHolder, rv);
  MOZ_ASSERT(!rv.Failed(), "Remove anonymous content should not fail!");
}

void
AccessibleCaret::SetPosition(nsIFrame* aFrame, int32_t aOffset)
{
  nsRect imaginaryCaretRect =
    nsCaret::GetGeometryForFrame(aFrame, aOffset, nullptr);
  bool imaginaryCaretRectVisible =
    nsLayoutUtils::IsRectVisibleInScrollFrames(aFrame, imaginaryCaretRect);

  if (!imaginaryCaretRectVisible) {
    SetAppearance(Appearance::NONE);
    return;
  }

  SetAppearance(Appearance::NORMAL);

  nsPoint caretElementPosition = CaretElementPosition(imaginaryCaretRect);

  caretElementPosition =
    ClampPositionToScrollFrames(aFrame, caretElementPosition);
  SetCaretElementPosition(aFrame, caretElementPosition);

  mImaginaryCaretRect = imaginaryCaretRect;
  nsLayoutUtils::TransformRect(aFrame, RootFrame(), mImaginaryCaretRect);
}

/* static */ nsPoint
AccessibleCaret::ClampPositionToScrollFrames(nsIFrame* aFrame,
                                             const nsPoint& aPosition)
{
  nsPoint position = aPosition;

  nsIFrame* closestScrollFrame =
    nsLayoutUtils::GetClosestFrameOfType(aFrame, nsGkAtoms::scrollFrame);

  while (closestScrollFrame) {
    nsIScrollableFrame* sf = do_QueryFrame(closestScrollFrame);
    nsRect closestScrollPortRect = sf->GetScrollPortRect();

    // Clamp the position in the scroll port.
    nsLayoutUtils::TransformRect(closestScrollFrame, aFrame,
                                 closestScrollPortRect);
    position = closestScrollPortRect.ClampPoint(position);

    // Get next ancestor scroll frame.
    closestScrollFrame = nsLayoutUtils::GetClosestFrameOfType(
      closestScrollFrame->GetParent(), nsGkAtoms::scrollFrame);
  }

  return position;
}

nsIFrame*
AccessibleCaret::RootFrame() const
{
  return mPresShell->GetRootFrame();
}

nsIFrame*
AccessibleCaret::ElementContainerFrame() const
{
  nsCanvasFrame* canvasFrame = mPresShell->GetCanvasFrame();
  Element* container = canvasFrame->GetCustomContentContainer();
  nsIFrame* containerFrame = container->GetPrimaryFrame();
  return containerFrame;
}

nsPoint
AccessibleCaret::LogicalPosition() const
{
  return mImaginaryCaretRect.Center();
}

/* static */ nsPoint
AccessibleCaret::CaretElementPosition(const nsRect& aRect)
{
  return aRect.TopLeft() + nsPoint(aRect.width / 2, aRect.height);
}

void
AccessibleCaret::SetCaretElementPosition(nsIFrame* aFrame,
                                         const nsPoint& aPosition)
{
  // Transform aPosition so that it relatives to containerFrame.
  nsPoint position = aPosition;
  nsLayoutUtils::TransformPoint(aFrame, ElementContainerFrame(), position);

  nsAutoString styleStr;
  styleStr.AppendPrintf("left: %dpx; top: %dpx;",
                        nsPresContext::AppUnitsToIntCSSPixels(position.x),
                        nsPresContext::AppUnitsToIntCSSPixels(position.y));

  ErrorResult rv;
  CaretElement()->SetAttribute(NS_LITERAL_STRING("style"), styleStr, rv);
  MOZ_ASSERT(!rv.Failed());
}
