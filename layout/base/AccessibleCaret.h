/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaret_h__
#define AccessibleCaret_h__

#include "mozilla/Attributes.h"
#include "nsISupportsBase.h"
#include "nsISupportsImpl.h"
#include "nsRefPtr.h"

class nsIFrame;
class nsIPresShell;
struct nsPoint;

namespace mozilla {

namespace dom {
class AnonymousContent;
}

class AccessibleCaret MOZ_FINAL : public nsISupports
{
public:
  enum TiltDirection {
    TILT_LEFT,
    TILT_RIGHT
  };

  explicit AccessibleCaret(nsIPresShell* aPresShell);
  NS_DECL_ISUPPORTS

  void SetVisibility(bool aVisible);
  void SetPositionBasedOnFrameOffset(nsIFrame* aFrame, int32_t aOffset);
  void SetTilted(bool aTilted, TiltDirection aDir = TILT_LEFT);
  bool Intersects(const AccessibleCaret& rhs);
  bool Contains(const nsPoint& aPosition);
private:
  ~AccessibleCaret();

  void MaybeInjectAnonymousContent();
  void SetPosition(const nsPoint& aPosition);

  bool mHasInjected;
  bool mVisible;
  nsIPresShell* mPresShell;
  nsRefPtr<mozilla::dom::AnonymousContent> mAnonymousContent;
};
} // namespace mozilla

#endif //AccessibleCaret_h__
