/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaret_h__
#define AccessibleCaret_h__

#include "mozilla/Attributes.h"
#include "mozilla/TypedEnum.h"
#include "nsISupportsBase.h"
#include "nsISupportsImpl.h"
#include "nsCOMPtr.h"
#include "nsRect.h"
#include "nsRefPtr.h"
#include "nsString.h"

class nsIDocument;
class nsIFrame;
class nsIPresShell;
struct nsPoint;

namespace mozilla {

namespace dom {
class AnonymousContent;
class Element;
}

// All the rect or point used are relative to root frame except being specified
// explicitly.
class AccessibleCaret MOZ_FINAL
{
public:
  explicit AccessibleCaret(nsIPresShell* aPresShell);
  ~AccessibleCaret();

  MOZ_BEGIN_NESTED_ENUM_CLASS(Appearance, uint8_t)
    NONE,
    NORMAL,
    LEFT,
    RIGHT
  MOZ_END_ENUM_CLASS(Appearance)

  bool IsVisible() const;
  void SetAppearance(Appearance aAppearance);

  void SetPosition(nsIFrame* aFrame, int32_t aOffset);
  bool Intersects(const AccessibleCaret& rhs);
  bool Contains(const nsPoint& aPosition);

  // The geometry center of a imaginary caret to which this AccessibleCaret is
  // attached.
  nsPoint LogicalPosition() const;

private:
  void SetCaretElementPosition(nsIFrame* aFrame, const nsPoint& aPosition);

  // Element for 'Intersects' test.
  dom::Element* CaretElement() const;

  // Element which contains the caret image for 'Contains' test.
  dom::Element* CaretElementInner() const;

  nsIFrame* RootFrame() const;
  nsIFrame* ElementContainerFrame() const;

  // Utilities
  static nsString AppearanceString(Appearance aAppearance);

  static already_AddRefed<dom::AnonymousContent> InjectCaretElement(nsIDocument* aDocument);
  static already_AddRefed<dom::Element> CreateCaretElement(nsIDocument* aDocument);

  // The bottom-center of a imaginary caret to which this AccessibleCaret is
  // attached.
  static nsPoint CaretElementPosition(const nsRect& aRect);

  static nsPoint ClampPositionToScrollFrames(nsIFrame* aFrame,
                                             const nsPoint& aPosition);

  // Member variables
  Appearance mAppearance;
  nsIPresShell* mPresShell;
  nsRefPtr<dom::AnonymousContent> mCaretElementHolder;
  nsRect mImaginaryCaretRect;
}; // class AccessibleCaret

MOZ_FINISH_NESTED_ENUM_CLASS(AccessibleCaret::Appearance)

} // namespace mozilla

#endif //AccessibleCaret_h__
