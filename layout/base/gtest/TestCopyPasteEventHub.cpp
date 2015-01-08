/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "nsIFrame.h"
#include "CopyPasteEventHub.h"
#include "CopyPasteManager.h"
#include "gfxPrefs.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"

using namespace mozilla;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::DefaultValue;
using ::testing::InSequence;
using ::testing::Eq;

class MockCopyPasteManager : public CopyPasteManager
{
public:
  explicit MockCopyPasteManager()
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
  explicit MockCopyPasteEventHub(MockCopyPasteManager* aManager)
    : CopyPasteEventHub(nullptr, aManager)
  { }

  CopyPasteEventHub::InputType GetType() { return mType; }
  CopyPasteEventHub::InputState GetState() { return mState; }
  int32_t ActiveTouchId() { return mActiveTouchId; }
  static int32_t InvalidTouchId() { return CopyPasteEventHub::kInvalidTouchId; }

  typedef CopyPasteEventHub::InputState InputState;
  typedef CopyPasteEventHub::InputType InputType;
};

class CopyPasteEventHubTester : public ::testing::Test {
protected:
  explicit CopyPasteEventHubTester()
    : mMockManager(new MockCopyPasteManager())
    , mMockEventHub(new MockCopyPasteEventHub(mMockManager))
  {
    gfxPrefs::GetSingleton();
    DefaultValue<nsEventStatus>::Set(nsEventStatus_eIgnore);
  }

  nsRefPtr<MockCopyPasteManager> mMockManager;
  nsRefPtr<MockCopyPasteEventHub> mMockEventHub;
};

TEST_F(CopyPasteEventHubTester, TestMouseEventOnPress) {
  EXPECT_CALL(*mMockManager, OnPress(Eq(nsPoint(0, 0))));

  WidgetMouseEvent evt(true, NS_MOUSE_BUTTON_DOWN, nullptr, WidgetMouseEvent::eReal);
  evt.button = WidgetMouseEvent::eLeftButton;

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::RELEASE);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::NONE);

  mMockEventHub->HandleEvent(&evt);

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::MOUSE);
}

TEST_F(CopyPasteEventHubTester, TestTouchEventOnPress) {
  EXPECT_CALL(*mMockManager, OnPress(Eq(nsPoint(0, 0))));

  WidgetTouchEvent evt(true, NS_TOUCH_START, nullptr);
  int32_t identifier = 0;
  nsIntPoint point(0, 0);
  nsIntPoint radius(19, 19);
  float rotationAngle = 0;
  float force = 1;
  evt.touches.AppendElement(new dom::Touch(identifier, point, radius,
                                           rotationAngle, force));

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::RELEASE);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::NONE);
  EXPECT_EQ(mMockEventHub->ActiveTouchId(), MockCopyPasteEventHub::InvalidTouchId());

  mMockEventHub->HandleEvent(&evt);

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::TOUCH);
  EXPECT_EQ(mMockEventHub->ActiveTouchId(), identifier);
}

TEST_F(CopyPasteEventHubTester, TestOnDrag) {
  nscoord x1 = 0, y1 = 0;
  nscoord x2 = 100, y2 = 100;
  nscoord x3 = 300, y3 = 300;

  {
    InSequence dummy;
    EXPECT_CALL(*mMockManager, OnPress(Eq(nsPoint(x1, y1))));
    EXPECT_CALL(*mMockManager, OnDrag(Eq(nsPoint(x2, y2))));
    EXPECT_CALL(*mMockManager, OnDrag(Eq(nsPoint(x3, y3))));
  }

  // Press first
  WidgetMouseEvent evt(true, NS_MOUSE_BUTTON_DOWN, nullptr, WidgetMouseEvent::eReal);
  evt.button = WidgetMouseEvent::eLeftButton;
  evt.refPoint = LayoutDeviceIntPoint(x1, y1);
  mMockEventHub->HandleEvent(&evt);
  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);

  // Then drag without exceeding the threshold
  evt.message = NS_MOUSE_MOVE;
  evt.refPoint = LayoutDeviceIntPoint(x2, y2);
  mMockEventHub->HandleEvent(&evt);
  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);

  // Then drag over the threshold
  evt.refPoint = LayoutDeviceIntPoint(x3, y3);
  mMockEventHub->HandleEvent(&evt);
  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::DRAG);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::MOUSE);
}

TEST_F(CopyPasteEventHubTester, TestMouseEventOnRelease) {
  {
    InSequence dummy;
    EXPECT_CALL(*mMockManager, OnPress(_));
    EXPECT_CALL(*mMockManager, OnRelease());
    EXPECT_CALL(*mMockManager, OnTap(_));
  }

  WidgetMouseEvent evt(true, NS_MOUSE_BUTTON_DOWN, nullptr, WidgetMouseEvent::eReal);
  evt.button = WidgetMouseEvent::eLeftButton;
  mMockEventHub->HandleEvent(&evt);

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::MOUSE);

  evt.message = NS_MOUSE_BUTTON_UP;
  mMockEventHub->HandleEvent(&evt);

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::RELEASE);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::NONE);
}

TEST_F(CopyPasteEventHubTester, TestTouchEventOnRelease) {
  {
    InSequence dummy;
    EXPECT_CALL(*mMockManager, OnPress(_));
    EXPECT_CALL(*mMockManager, OnRelease());
    EXPECT_CALL(*mMockManager, OnTap(_));
  }

  WidgetTouchEvent evt(true, NS_TOUCH_START, nullptr);
  int32_t identifier = 0;
  nsIntPoint point(0, 0);
  nsIntPoint radius(19, 19);
  float rotationAngle = 0;
  float force = 1;
  evt.touches.AppendElement(new dom::Touch(identifier, point, radius,
                                           rotationAngle, force));

  mMockEventHub->HandleEvent(&evt);

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::TOUCH);
  EXPECT_EQ(mMockEventHub->ActiveTouchId(), identifier);

  evt.message = NS_TOUCH_END;
  mMockEventHub->HandleEvent(&evt);

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::RELEASE);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::NONE);
  EXPECT_EQ(mMockEventHub->ActiveTouchId(), MockCopyPasteEventHub::InvalidTouchId());
}
