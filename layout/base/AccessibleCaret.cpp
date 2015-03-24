/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleCaret.h"

#include "CopyPasteLogger.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/AnonymousContent.h"
#include "nsCanvasFrame.h"
#include "nsCaret.h"
#include "nsDOMTokenList.h"
#include "nsIFrame.h"

namespace mozilla {
using namespace dom;

#undef CP_LOG
#define CP_LOG(message, ...)                                                   \
  CP_LOG_BASE("AccessibleCaret (%p): " message, this, ##__VA_ARGS__);

#undef CP_LOGV
#define CP_LOGV(message, ...)                                                  \
  CP_LOGV_BASE("AccessibleCaret (%p): " message, this, ##__VA_ARGS__);

NS_IMPL_ISUPPORTS(AccessibleCaret::DummyTouchListener, nsIDOMEventListener)

AccessibleCaret::AccessibleCaret(nsIPresShell* aPresShell)
  : mAppearance(Appearance::None)
  , mBarEnabled(false)
  , mPresShell(aPresShell)
  , mDummyTouchListener(new DummyTouchListener())
{
  // Check all resources required.
  MOZ_ASSERT(mPresShell);
  MOZ_ASSERT(RootFrame());
  MOZ_ASSERT(mPresShell->GetDocument());
  MOZ_ASSERT(mPresShell->GetCanvasFrame());
  MOZ_ASSERT(mPresShell->GetCanvasFrame()->GetCustomContentContainer());

  InjectCaretElement(mPresShell->GetDocument());
}

AccessibleCaret::~AccessibleCaret()
{
  RemoveCaretElement(mPresShell->GetDocument());
}

bool
AccessibleCaret::IsLogicallyVisible() const
{
  return mAppearance != Appearance::None;
}

bool
AccessibleCaret::IsVisuallyVisible() const
{
  return (mAppearance != Appearance::None) &&
         (mAppearance != Appearance::NormalNotShown);
}

AccessibleCaret::Appearance
AccessibleCaret::GetAppearance() const
{
  return mAppearance;
}

void
AccessibleCaret::SetAppearance(Appearance aAppearance)
{
  if (mAppearance == aAppearance) {
    return;
  }

  ErrorResult rv;
  CaretElement()->ClassList()->Remove(AppearanceString(mAppearance), rv);
  MOZ_ASSERT(!rv.Failed(), "Remove old appearance failed!");

  CaretElement()->ClassList()->Add(AppearanceString(aAppearance), rv);
  MOZ_ASSERT(!rv.Failed(), "Add new appearance failed!");

  mAppearance = aAppearance;

  // Need to reset rect since the cached rect will be compared in SetPosition.
  if (mAppearance == Appearance::None) {
    mImaginaryCaretRect = nsRect();
  }
}

void
AccessibleCaret::SetBarEnabled(bool aEnabled)
{
  CP_LOG("%s, enabled %d", __FUNCTION__, aEnabled);

  if (mBarEnabled == aEnabled) {
    return;
  }

  ErrorResult rv;
  CaretElement()->ClassList()->Toggle(NS_LITERAL_STRING("nobar"),
                                      Optional<bool>(!aEnabled), rv);
  MOZ_ASSERT(!rv.Failed());

  mBarEnabled = aEnabled;
}

/* static */ nsString
AccessibleCaret::AppearanceString(Appearance aAppearance)
{
  nsAutoString string;
  switch (aAppearance) {
  case Appearance::None:
  case Appearance::NormalNotShown:
    string = NS_LITERAL_STRING("none");
    break;
  case Appearance::Normal:
    string = NS_LITERAL_STRING("normal");
    break;
  case Appearance::Right:
    string = NS_LITERAL_STRING("right");
    break;
  case Appearance::Left:
    string = NS_LITERAL_STRING("left");
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
AccessibleCaret::CaretImageElement() const
{
  return CaretElement()->GetFirstElementChild();
}

Element*
AccessibleCaret::CaretBarElement() const
{
  return CaretElement()->GetLastElementChild();
}

bool
AccessibleCaret::Intersects(const AccessibleCaret& aCaret) const
{
  MOZ_ASSERT(mPresShell == aCaret.mPresShell);

  if (!IsVisuallyVisible() || !aCaret.IsVisuallyVisible()) {
    return false;
  }

  nsRect rect = nsLayoutUtils::GetRectRelativeToFrame(CaretElement(), RootFrame());
  nsRect rhsRect = nsLayoutUtils::GetRectRelativeToFrame(aCaret.CaretElement(), RootFrame());
  return rect.Intersects(rhsRect);
}

bool
AccessibleCaret::Contains(const nsPoint& aPosition) const
{
  if (!IsVisuallyVisible()) {
    return false;
  }

  nsRect rect =
    nsLayoutUtils::GetRectRelativeToFrame(CaretImageElement(), RootFrame());

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

  // InsertAnonymousContent will clone the element to make an AnonymousContent.
  // Since event listeners are not being cloned when cloning a node, we need to
  // add the listener here.
  CaretElement()->AddEventListener(NS_LITERAL_STRING("touchstart"),
                                   mDummyTouchListener, false);
}

already_AddRefed<Element>
AccessibleCaret::CreateCaretElement(nsIDocument* aDocument) const
{
  // Content structure of AccessibleCaret
  // <div class="accessiblecaret">      <- CaretElement()
  //   <div class="image">              <- CaretImageElement()
  //   <div class="bar">                <- CaretBarElement()

  ErrorResult rv;
  nsCOMPtr<Element> parent = aDocument->CreateHTMLElement(nsGkAtoms::div);
  parent->ClassList()->Add(NS_LITERAL_STRING("moz-accessiblecaret"), rv);
  parent->ClassList()->Add(NS_LITERAL_STRING("none"), rv);
  parent->ClassList()->Add(NS_LITERAL_STRING("nobar"), rv);

  nsCOMPtr<Element> image = aDocument->CreateHTMLElement(nsGkAtoms::div);
  image->ClassList()->Add(NS_LITERAL_STRING("image"), rv);
  parent->AppendChildTo(image, false);

  nsCOMPtr<Element> bar = aDocument->CreateHTMLElement(nsGkAtoms::div);
  bar->ClassList()->Add(NS_LITERAL_STRING("bar"), rv);
  parent->AppendChildTo(bar, false);

  return parent.forget();
}

void
AccessibleCaret::RemoveCaretElement(nsIDocument* aDocument)
{
  CaretElement()->RemoveEventListener(NS_LITERAL_STRING("touchstart"),
                                      mDummyTouchListener, false);

  ErrorResult rv;
  aDocument->RemoveAnonymousContent(*mCaretElementHolder, rv);
  MOZ_ASSERT(!rv.Failed(), "Remove anonymous content should not fail!");
}

AccessibleCaret::PositionChangedResult
AccessibleCaret::SetPosition(nsIFrame* aFrame, int32_t aOffset)
{
  nsRect imaginaryCaretRectInFrame =
    nsCaret::GetGeometryForFrame(aFrame, aOffset, nullptr);

  imaginaryCaretRectInFrame =
    nsLayoutUtils::ClampRectToScrollFrames(aFrame, imaginaryCaretRectInFrame);

  if (imaginaryCaretRectInFrame.IsEmpty()) {
    // Don't bother to set the caret position since it's invisible.
    return PositionChangedResult::Invisible;
  }

  nsRect imaginaryCaretRect = imaginaryCaretRectInFrame;
  nsLayoutUtils::TransformRect(aFrame, RootFrame(), imaginaryCaretRect);

  if (imaginaryCaretRect.IsEqualEdges(mImaginaryCaretRect)) {
    return PositionChangedResult::NotChanged;
  }

  mImaginaryCaretRect = imaginaryCaretRect;

  SetCaretElementPosition(aFrame, imaginaryCaretRectInFrame);
  SetCaretBarElementPosition(aFrame, imaginaryCaretRectInFrame);

  return PositionChangedResult::Changed;
}

nsIFrame*
AccessibleCaret::RootFrame() const
{
  return mPresShell->GetRootFrame();
}

nsIFrame*
AccessibleCaret::CustomContentContainerFrame() const
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
AccessibleCaret::SetCaretElementPosition(nsIFrame* aFrame, const nsRect& aRect)
{
  // Transform position so that it relatives to containerFrame.
  nsPoint position = CaretElementPosition(aRect);
  nsLayoutUtils::TransformPoint(aFrame, CustomContentContainerFrame(),
                                position);

  nsAutoString styleStr;
  styleStr.AppendPrintf("left: %dpx; top: %dpx;",
                        nsPresContext::AppUnitsToIntCSSPixels(position.x),
                        nsPresContext::AppUnitsToIntCSSPixels(position.y));

  ErrorResult rv;
  CaretElement()->SetAttribute(NS_LITERAL_STRING("style"), styleStr, rv);
  MOZ_ASSERT(!rv.Failed());

  CP_LOG("Set caret style: %s", NS_ConvertUTF16toUTF8(styleStr).get());
}

void
AccessibleCaret::SetCaretBarElementPosition(nsIFrame* aFrame,
                                            const nsRect& aRect)
{
  int32_t height = nsPresContext::AppUnitsToIntCSSPixels(aRect.height);
  nsAutoString barStyleStr;
  barStyleStr.AppendPrintf("margin-top: -%dpx; height: %dpx;",
                           height, height);

  ErrorResult rv;
  CaretBarElement()->SetAttribute(NS_LITERAL_STRING("style"), barStyleStr, rv);
  MOZ_ASSERT(!rv.Failed());

  CP_LOG("Set bar style: %s", NS_ConvertUTF16toUTF8(barStyleStr).get());
}

} // namespace mozilla
