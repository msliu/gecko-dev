/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "nsIFrame.h"
#include "CopyPasteEventHub.h"
#include "CopyPasteManager.h"
#include "gfxPrefs.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/MouseEvents.h"

using namespace mozilla;
using ::testing::_;
using ::testing::Eq;
using ::testing::AtLeast;
using ::testing::DefaultValue;

class MockCopyPasteManager : public CopyPasteManager
{
public:
  MockCopyPasteManager()
    : CopyPasteManager(nullptr)
  { }

  MOCK_METHOD1(OnPress, nsEventStatus(const nsPoint&));
  MOCK_METHOD1(OnDrag, nsEventStatus(const nsPoint&));
  MOCK_METHOD0(OnRelease, nsEventStatus());
  MOCK_METHOD1(OnLongTap, nsEventStatus(const nsPoint&));
  MOCK_METHOD1(OnTap, nsEventStatus(const nsPoint&));
};

class MockCopyPasteEventHub : public CopyPasteEventHub {
public:
  MockCopyPasteEventHub(MockCopyPasteManager* aManager)
    : CopyPasteEventHub(nullptr, aManager)
  { }

  CopyPasteEventHub::InputType GetType() { return mType; }
  CopyPasteEventHub::InputState GetState() { return mState; }

  typedef CopyPasteEventHub::InputState InputState;
  typedef CopyPasteEventHub::InputType InputType;
};

class CopyPasteEventHubTester : public ::testing::Test {
protected:
  CopyPasteEventHubTester()
    : mMockManager(new MockCopyPasteManager())
    , mMockEventHub(new MockCopyPasteEventHub(mMockManager))
  {
    gfxPrefs::GetSingleton();
    DefaultValue<nsEventStatus>::Set(nsEventStatus_eIgnore);
  }

  nsRefPtr<MockCopyPasteManager> mMockManager;
  nsRefPtr<MockCopyPasteEventHub> mMockEventHub;
};

TEST_F(CopyPasteEventHubTester, TestOnPress) {
  EXPECT_CALL(*mMockManager, OnPress(Eq(nsPoint(0, 0))))
    .Times(1);

  WidgetMouseEvent evt(true, NS_MOUSE_BUTTON_DOWN, nullptr, WidgetMouseEvent::eReal);
  evt.button = WidgetMouseEvent::eLeftButton;
  mMockEventHub->HandleEvent(&evt);

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::MOUSE);
}

TEST_F(CopyPasteEventHubTester, TestOnDrag) {
  // Press first
  EXPECT_CALL(*mMockManager, OnPress(Eq(nsPoint(0, 0))))
    .Times(1);
  WidgetMouseEvent evt(true, NS_MOUSE_BUTTON_DOWN, nullptr, WidgetMouseEvent::eReal);
  evt.button = WidgetMouseEvent::eLeftButton;
  evt.refPoint = LayoutDeviceIntPoint(0, 0);
  mMockEventHub->HandleEvent(&evt);
  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);

  // Then drag but not exceed kMoveStartTolerancePx
  EXPECT_CALL(*mMockManager, OnDrag(Eq(nsPoint(100, 100))))
    .Times(1);
  evt.message = NS_MOUSE_MOVE;
  evt.refPoint = LayoutDeviceIntPoint(100, 100);
  mMockEventHub->HandleEvent(&evt);
  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);

  // Then drag over kMoveStartTolerancePx
  EXPECT_CALL(*mMockManager, OnDrag(Eq(nsPoint(300, 300))))
    .Times(1);
  evt.refPoint = LayoutDeviceIntPoint(300, 300);
  mMockEventHub->HandleEvent(&evt);
  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::DRAG);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::MOUSE);
}

TEST_F(CopyPasteEventHubTester, TestOnRelease) {
  EXPECT_CALL(*mMockManager, OnPress(_))
    .Times(1);

  WidgetMouseEvent evt(true, NS_MOUSE_BUTTON_DOWN, nullptr, WidgetMouseEvent::eReal);
  evt.button = WidgetMouseEvent::eLeftButton;
  mMockEventHub->HandleEvent(&evt);

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::MOUSE);

  EXPECT_CALL(*mMockManager, OnRelease())
    .Times(1);
  EXPECT_CALL(*mMockManager, OnTap(_))
    .Times(1);

  evt.message = NS_MOUSE_BUTTON_UP;
  mMockEventHub->HandleEvent(&evt);

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::RELEASE);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::NONE);
}
