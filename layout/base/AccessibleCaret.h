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

// All the rect or point used are relative to root frame except being specified
// explicitly.
class AccessibleCaret MOZ_FINAL
{
public:
  explicit AccessibleCaret(nsIPresShell* aPresShell);
  ~AccessibleCaret();

  enum class Appearance : uint8_t {
    None,
    Normal,
    Left,
    Right
  };

  bool IsVisible() const;
  void SetAppearance(Appearance aAppearance);

  // Return NS_OK if the we did change to a new position.
  nsresult SetPosition(nsIFrame* aFrame, int32_t aOffset);

  bool Intersects(const AccessibleCaret& rhs);
  bool Contains(const nsPoint& aPosition);

  // The geometry center of a imaginary caret to which this AccessibleCaret is
  // attached. It is needed when dragging the caret.
  nsPoint LogicalPosition() const;

private:
  void SetCaretElementPosition(nsIFrame* aFrame, const nsPoint& aPosition);

  // Element for 'Intersects' test.
  dom::Element* CaretElement() const;

  // Element which contains the caret image for 'Contains' test.
  dom::Element* CaretElementInner() const;

  nsIFrame* RootFrame() const;
  nsIFrame* ElementContainerFrame() const;

  // Transform Appearance to CSS class name in ua.css.
  static nsString AppearanceString(Appearance aAppearance);

  void InjectCaretElement(nsIDocument* aDocument);
  already_AddRefed<dom::Element> CreateCaretElement(nsIDocument* aDocument) const;
  void RemoveCaretElement(nsIDocument* aDocument);

  // The bottom-center of a imaginary caret to which this AccessibleCaret is
  // attached.
  static nsPoint CaretElementPosition(const nsRect& aRect);

  static nsPoint ClampPositionToScrollFrames(nsIFrame* aFrame,
                                             const nsPoint& aPosition);

  class DummyTouchListener MOZ_FINAL : public nsIDOMEventListener
  {
  public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) MOZ_OVERRIDE
    {
      return NS_OK;
    }

  private:
    virtual ~DummyTouchListener() {};
  };

  // Member variables
  Appearance mAppearance;
  nsIPresShell* mPresShell;
  nsRefPtr<dom::AnonymousContent> mCaretElementHolder;
  nsRect mImaginaryCaretRect;

  // A no-op touch-start listener which prevents APZ from panning when dragging
  // the caret.
  nsRefPtr<DummyTouchListener> mDummyTouchListener;


}; // class AccessibleCaret

} // namespace mozilla

#endif // AccessibleCaret_h__
