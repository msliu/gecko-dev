/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaret_h__
#define AccessibleCaret_h__

#include "mozilla/Attributes.h"
#include "nsIDOMEventListener.h"
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

// -----------------------------------------------------------------------------
// Upon the creation of AccessibleCaret, it will insert DOM Element as an
// anonymous content containing the caret image. The caret appearance and
// position can be controlled by SetAppearance() and SetPosition().
//
// All the rect or point used are relative to root frame except being specified
// explicitly.
//
class AccessibleCaret final
{
public:
  explicit AccessibleCaret(nsIPresShell* aPresShell);
  ~AccessibleCaret();

  // This enumeration representing the visibility and visual style of an
  // AccessibleCaret.
  //
  // Use SetAppearance() to change the appearance, and use GetAppearance() to
  // get the current appearance.
  enum class Appearance : uint8_t {
    // Do not display the caret at all.
    None,

    // Display the caret in default style.
    Normal,

    // The caret should be displayed logically but it is kept invisible to the
    // user. This enum is the only difference between "logically visible" and
    // "visually visible". It can be used for reasons such as:
    // 1. Out of scroll port.
    // 2. For UX requirement such as hide a caret in an empty text area.
    NormalNotShown,

    // Display the caret which is tilted to the left.
    Left,

    // Display the caret which is tilted to the right.
    Right
  };
  Appearance GetAppearance() const;
  void SetAppearance(Appearance aAppearance);

  // Return true if current appearance is either Normal, NormalNotShown, Left,
  // or Right.
  bool IsLogicallyVisible() const;

  // Return true if current appearance is either Normal, Left, or Right.
  bool IsVisuallyVisible() const;

  // Control the "Text Selection Bar" described in "Text Selection Visual Spec"
  // in bug 921965.
  void SetBarEnabled(bool aEnabled);

  // This enumeration representing the result returned by SetPosition().
  enum class PositionChangedResult : uint8_t {
    // Position is not changed.
    NotChanged,

    // Position is changed.
    Changed,

    // Position is out of scroll port.
    Invisible
  };
  PositionChangedResult SetPosition(nsIFrame* aFrame, int32_t aOffset);

  // Does two AccessibleCarets overlap?
  bool Intersects(const AccessibleCaret& rhs) const;

  // Is the position within the caret's rect?
  bool Contains(const nsPoint& aPosition) const;

  // The geometry center of the imaginary caret (nsCaret) to which this
  // AccessibleCaret is attached. It is needed when dragging the caret.
  nsPoint LogicalPosition() const;

  // Element for 'Intersects' test. Container of image and bar elements.
  dom::Element* CaretElement() const;

private:
  void SetCaretElementPosition(nsIFrame* aFrame, const nsRect& aRect);
  void SetCaretBarElementPosition(nsIFrame* aFrame, const nsRect& aRect);

  // Element which contains the caret image for 'Contains' test.
  dom::Element* CaretImageElement() const;

  // Element which represents the text selection bar.
  dom::Element* CaretBarElement() const;

  nsIFrame* RootFrame() const;
  nsIFrame* CustomContentContainerFrame() const;

  // Transform Appearance to CSS class name in ua.css.
  static nsString AppearanceString(Appearance aAppearance);

  void InjectCaretElement(nsIDocument* aDocument);
  already_AddRefed<dom::Element> CreateCaretElement(nsIDocument* aDocument) const;
  void RemoveCaretElement(nsIDocument* aDocument);

  // The bottom-center of the imaginary caret to which this AccessibleCaret is
  // attached.
  static nsPoint CaretElementPosition(const nsRect& aRect);

  class DummyTouchListener final : public nsIDOMEventListener
  {
  public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) override
    {
      return NS_OK;
    }

  private:
    virtual ~DummyTouchListener() {};
  };

  // Member variables
  Appearance mAppearance = Appearance::None;
  bool mBarEnabled = false;
  nsIPresShell* mPresShell = nullptr;
  nsRefPtr<dom::AnonymousContent> mCaretElementHolder;
  nsRect mImaginaryCaretRect;

  // A no-op touch-start listener which prevents APZ from panning when dragging
  // the caret.
  nsRefPtr<DummyTouchListener> mDummyTouchListener{new DummyTouchListener()};

}; // class AccessibleCaret

} // namespace mozilla

#endif // AccessibleCaret_h__
