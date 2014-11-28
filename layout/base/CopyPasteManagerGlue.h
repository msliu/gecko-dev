/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  virtual bool GetSelectionIsCollapsed();
  virtual int32_t GetSelectionRangeCount();

  virtual nsresult SelectWord(const nsPoint& aPoint);
  virtual void SetSelectionDragState(bool aState);
  virtual void SetSelectionDirection(bool aForward);

  virtual nsIFrame* FindFirstNodeWithFrame(bool aBackward, int& aOutOffset);
  virtual nsEventStatus DragCaret(const nsPoint &aMovePoint, bool aIsExtend, bool aIsBeginRange);
protected:
  virtual ~CopyPasteManagerGlue() {}
  // Utility function
  dom::Selection* GetSelection();
  already_AddRefed<nsFrameSelection> GetFrameSelection();
  nsIContent* GetFocusedContent();

  nsIPresShell* mPresShell;
};

} // namespace mozilla

#endif //CopyPasteManagerGlue_h__
