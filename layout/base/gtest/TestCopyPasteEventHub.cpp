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

TEST_F(CopyPasteEventHubTester, TestOnPress) {
  EXPECT_CALL(*mMockManager, OnPress(Eq(nsPoint(0, 0))));

  WidgetMouseEvent evt(true, NS_MOUSE_BUTTON_DOWN, nullptr, WidgetMouseEvent::eReal);
  evt.button = WidgetMouseEvent::eLeftButton;

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::RELEASE);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::NONE);

  mMockEventHub->HandleEvent(&evt);

  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::MOUSE);
}

TEST_F(CopyPasteEventHubTester, TestOnDrag) {
  {
    InSequence dummy;
    EXPECT_CALL(*mMockManager, OnPress(Eq(nsPoint(0, 0))));
    EXPECT_CALL(*mMockManager, OnDrag(Eq(nsPoint(100, 100))));
    EXPECT_CALL(*mMockManager, OnDrag(Eq(nsPoint(300, 300))));
  }

  // Press first
  WidgetMouseEvent evt(true, NS_MOUSE_BUTTON_DOWN, nullptr, WidgetMouseEvent::eReal);
  evt.button = WidgetMouseEvent::eLeftButton;
  evt.refPoint = LayoutDeviceIntPoint(0, 0);
  mMockEventHub->HandleEvent(&evt);
  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);

  // Then drag but not exceed kMoveStartTolerancePx
  evt.message = NS_MOUSE_MOVE;
  evt.refPoint = LayoutDeviceIntPoint(100, 100);
  mMockEventHub->HandleEvent(&evt);
  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::PRESS);

  // Then drag over kMoveStartTolerancePx
  evt.refPoint = LayoutDeviceIntPoint(300, 300);
  mMockEventHub->HandleEvent(&evt);
  EXPECT_EQ(mMockEventHub->GetState(), MockCopyPasteEventHub::InputState::DRAG);
  EXPECT_EQ(mMockEventHub->GetType(), MockCopyPasteEventHub::InputType::MOUSE);
}

TEST_F(CopyPasteEventHubTester, TestOnRelease) {
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
