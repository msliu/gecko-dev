/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "CopyPasteEventHub.h"
#include "CopyPasteManager.h"
#include "gfxPrefs.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/MouseEvents.h"
#include "nsDocShell.h"

using namespace mozilla;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::DefaultValue;

class MockCopyPasteManager : public CopyPasteManager
{
public:
  MOCK_METHOD1(OnPress, nsEventStatus(const nsPoint&));
  MOCK_METHOD1(OnDrag, nsEventStatus(const nsPoint&));
  MOCK_METHOD0(OnRelease, nsEventStatus());
  MOCK_METHOD1(OnLongTap, nsEventStatus(const nsPoint&));
  MOCK_METHOD1(OnTap, nsEventStatus(const nsPoint&));
};

class MockGestureManager : public CopyPasteEventHub {
public:
  MockGestureManager(MockCopyPasteManager* aManager)
    : CopyPasteEventHub(nullptr, aManager)
  { }

  CopyPasteEventHub::InputType GetType() { return mType; }
  CopyPasteEventHub::InputState GetState() { return mState; }

  typedef CopyPasteEventHub::InputState InputState;
  typedef CopyPasteEventHub::InputType InputType;
};

class GestureManagerTester : public ::testing::Test {
protected:
  virtual void SetUp()
  {
    gfxPrefs::GetSingleton();
    mMockHandler = new MockCopyPasteManager();
    mGestureManager = new MockGestureManager(mMockHandler);

    DefaultValue<nsEventStatus>::Set(nsEventStatus_eIgnore);
  }

  nsRefPtr<MockGestureManager> mGestureManager;
  nsRefPtr<MockCopyPasteManager> mMockHandler;
};

TEST_F(GestureManagerTester, TestOnPress) {
  EXPECT_CALL(*mMockHandler, OnPress(_))
    .Times(AtLeast(1));

  WidgetMouseEvent evt(true, NS_MOUSE_BUTTON_DOWN, nullptr, WidgetMouseEvent::eReal);
  evt.button = WidgetMouseEvent::eLeftButton;
  mGestureManager->HandleEvent(&evt);

  EXPECT_EQ(mGestureManager->GetState(), MockGestureManager::InputState::PRESS);
  EXPECT_EQ(mGestureManager->GetType(), MockGestureManager::InputType::MOUSE);
}
