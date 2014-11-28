/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CopyPasteManagerGlue_h__
#define CopyPasteManagerGlue_h__

namespace mozilla {

class CopyPasteManagerGlue : public nsISupports
{
public:
  CopyPasteManagerGlue(nsIPresShell* aPresShell);

  NS_DECL_ISUPPORTS
  // Utility function
  virtual dom::Selection* GetSelection();
  virtual already_AddRefed<nsFrameSelection> GetFrameSelection();
  virtual nsIContent* GetFocusedContent();
  virtual bool GetSelectionIsCollapsed();

  virtual nsresult SelectWord(const nsPoint& aPoint);
  virtual void SetSelectionDragState(bool aState);
  virtual void SetSelectionDirection(bool aForward);

  virtual nsIFrame* FindFirstNodeWithFrame(bool aBackward, int& aOutOffset);
  virtual nsEventStatus DragCaret(const nsPoint &aMovePoint, bool aIsExtend, bool aIsBeginRange);
private:
  virtual ~CopyPasteManagerGlue() {}
  nsIPresShell* mPresShell;
};

} // namespace mozilla

#endif //CopyPasteManagerGlue_h__
