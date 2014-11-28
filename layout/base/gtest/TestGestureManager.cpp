/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "CopyPasteManager.h"

using namespace mozilla;

class MockCopyPasteManager : CopyPasteManager
{
  MOCK_METHOD1(OnPress, nsEventStatus(const nsPoint&));
  MOCK_METHOD1(OnDrag, nsEventStatus(const nsPoint&));
  MOCK_METHOD0(OnRelease, nsEventStatus());
  MOCK_METHOD1(OnLongTap, nsEventStatus(const nsPoint&));
  MOCK_METHOD1(OnTap, nsEventStatus(const nsPoint&));
};
