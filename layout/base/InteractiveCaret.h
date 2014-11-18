/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InteractiveCaret_h__
#define InteractiveCaret_h__

class nsIPresShell;

namespace mozilla {

namespace dom {
class AnonymousContent;
}

class InteractiveCaret MOZ_FINAL : public nsISupports
{
public:
  explicit InteractiveCaret(nsIPresShell* aPresShell);
  NS_DECL_ISUPPORTS

  void SetVisibility(bool aVisible);
  void SetPositionBasedOnFrameOffset(nsIFrame* aFrame, int32_t aOffset);
private:
  ~InteractiveCaret();

  void MaybeInjectAnonymousContent();
  void SetPosition(const nsPoint& aPosition);

  bool mHasInjected;
  bool mVisible;
  nsIPresShell* mPresShell;
  nsRefPtr<mozilla::dom::AnonymousContent> mAnonymousContent;
};
} // namespace mozilla

#endif //InteractiveCaret_h__
