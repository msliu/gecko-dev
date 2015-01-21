/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "CopyPasteEventHub.h"
#include "CopyPasteManager.h"
#include "gfxPrefs.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"

using ::testing::AtLeast;
using ::testing::DefaultValue;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace mozilla
{

class MockCopyPasteManager : public CopyPasteManager
{
public:
  explicit MockCopyPasteManager(nsIPresShell* aPresShell)
    : CopyPasteManager(aPresShell)
  {
  }

  MOCK_METHOD1(PressCaret, nsresult(const nsPoint& aPoint));
  MOCK_METHOD1(DragCaret, nsresult(const nsPoint& aPoint));
  MOCK_METHOD0(ReleaseCaret, nsresult());
  MOCK_METHOD1(TapCaret, nsresult(const nsPoint& aPoint));
  MOCK_METHOD1(SelectWordOrShortcut, nsresult(const nsPoint& aPoint));
};

class MockCopyPasteEventHub : public CopyPasteEventHub
{
public:
  using CopyPasteEventHub::State;
  using CopyPasteEventHub::NoActionState;
  using CopyPasteEventHub::PressCaretState;
  using CopyPasteEventHub::DragCaretState;
  using CopyPasteEventHub::WaitLongTapState;
  using CopyPasteEventHub::ScrollState;
  using CopyPasteEventHub::GetState;

  explicit MockCopyPasteEventHub() : CopyPasteEventHub()
  {
    mHandler = MakeUnique<MockCopyPasteManager>(nullptr);
    mInitialized = true;
  }

  MockCopyPasteManager* GetMockCopyPasteManager()
  {
    return reinterpret_cast<MockCopyPasteManager*>(mHandler.get());
  }
};

class CopyPasteEventHubTester : public ::testing::Test
{
public:
  explicit CopyPasteEventHubTester() : mHub(new MockCopyPasteEventHub())
  {
    DefaultValue<nsresult>::Set(NS_OK);
    EXPECT_EQ(mHub->GetState(), MockCopyPasteEventHub::NoActionState());
  }

  static UniquePtr<WidgetEvent> CreateMouseEvent(uint32_t aMessage, nscoord aX,
                                                 nscoord aY)
  {
    auto event = MakeUnique<WidgetMouseEvent>(true, aMessage, nullptr,
                                              WidgetMouseEvent::eReal);

    event->button = WidgetMouseEvent::eLeftButton;
    event->refPoint = LayoutDeviceIntPoint(aX, aY);

    return Move(event);
  }

  static UniquePtr<WidgetEvent> CreateMousePressEvent(nscoord aX, nscoord aY)
  {
    return CreateMouseEvent(NS_MOUSE_BUTTON_DOWN, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateMouseMoveEvent(nscoord aX, nscoord aY)
  {
    return CreateMouseEvent(NS_MOUSE_MOVE, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateMouseReleaseEvent(nscoord aX, nscoord aY)
  {
    return CreateMouseEvent(NS_MOUSE_BUTTON_UP, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateLongTapEvent(nscoord aX, nscoord aY)
  {
    return CreateMouseEvent(NS_MOUSE_MOZLONGTAP, aX, aY);
  }

  void HandleEventAndCheckState(UniquePtr<WidgetEvent> aEvent,
                                MockCopyPasteEventHub::State* aExpectedState,
                                nsEventStatus aExpectedEventStatus)
  {
    nsEventStatus rv = mHub->HandleEvent(aEvent.get());
    EXPECT_EQ(mHub->GetState(), aExpectedState);
    EXPECT_EQ(rv, aExpectedEventStatus);
  }

  template <typename PressEventCreator, typename ReleaseEventCreator>
  void TestPressReleaseNotOnCaret(PressEventCreator aPressEventCreator,
                                  ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename ReleaseEventCreator>
  void TestPressReleaseOnCaret(PressEventCreator aPressEventCreator,
                               ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename MoveEventCreator,
            typename ReleaseEventCreator>
  void TestPressMoveReleaseOnCaret(PressEventCreator aPressEventCreator,
                                   MoveEventCreator aMoveEventCreator,
                                   ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename ReleaseEventCreator>
  void TestLongTapWithSelectWordSuccessful(
    PressEventCreator aPressEventCreator,
    ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename ReleaseEventCreator>
  void TestLongTapWithSelectWordFailed(
    PressEventCreator aPressEventCreator,
    ReleaseEventCreator aReleaseEventCreator);

  nsRefPtr<MockCopyPasteEventHub> mHub;
}; // class CopyPasteEventHubTester

TEST_F(CopyPasteEventHubTester, TestMousePressReleaseNotOnCaret)
{
  TestPressReleaseNotOnCaret(CreateMousePressEvent, CreateMouseReleaseEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void
CopyPasteEventHubTester::TestPressReleaseNotOnCaret(
  PressEventCreator aPressEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  EXPECT_CALL(*mHub->GetMockCopyPasteManager(), PressCaret(_))
    .WillOnce(Return(NS_ERROR_FAILURE));

  EXPECT_CALL(*mHub->GetMockCopyPasteManager(), ReleaseCaret())
    .Times(0);

  EXPECT_CALL(*mHub->GetMockCopyPasteManager(), TapCaret(_))
    .Times(0);

  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockCopyPasteEventHub::WaitLongTapState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockCopyPasteEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(CopyPasteEventHubTester, TestMousePressReleaseOnCaret)
{
  TestPressReleaseOnCaret(CreateMousePressEvent, CreateMouseReleaseEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void
CopyPasteEventHubTester::TestPressReleaseOnCaret(
  PressEventCreator aPressEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), PressCaret(_))
      .WillOnce(Return(NS_OK));

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), ReleaseCaret());
    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), TapCaret(_));
  }

  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockCopyPasteEventHub::PressCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockCopyPasteEventHub::NoActionState(),
                           nsEventStatus_eConsumeNoDefault);
}

TEST_F(CopyPasteEventHubTester, TestMousePressMoveReleaseOnCaret)
{
  TestPressMoveReleaseOnCaret(CreateMousePressEvent, CreateMouseMoveEvent,
                              CreateMouseReleaseEvent);
}

template <typename PressEventCreator, typename MoveEventCreator,
          typename ReleaseEventCreator>
void
CopyPasteEventHubTester::TestPressMoveReleaseOnCaret(
  PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  nscoord x0 = 0, y0 = 0;
  nscoord x1 = 100, y1 = 100;
  nscoord x2 = 300, y2 = 300;
  nscoord x3 = 400, y3 = 400;

  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), PressCaret(_))
      .WillOnce(Return(NS_OK));

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), DragCaret(_))
      .Times(2) // two valid drag operations
      .WillRepeatedly(Return(NS_OK));

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), ReleaseCaret())
      .WillOnce(Return(NS_OK));
  }

  HandleEventAndCheckState(aPressEventCreator(x0, y0),
                           MockCopyPasteEventHub::PressCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  // A small move with the distance between (x0, y0) and (x1, y1) below the
  // tolerance value.
  HandleEventAndCheckState(aMoveEventCreator(x1, y1),
                           MockCopyPasteEventHub::PressCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  // A large move forms a valid drag since the distance between (x0, y0) and
  // (x2, y2) is above the tolerance value.
  HandleEventAndCheckState(aMoveEventCreator(x2, y2),
                           MockCopyPasteEventHub::DragCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  // Also a valid drag since the distance between (x0, y0) and (x3, y3) above
  // the tolerance value even if the distance between (x2, y2) and (x3, y3) is
  // below the tolerance value.
  HandleEventAndCheckState(aMoveEventCreator(x3, y3),
                           MockCopyPasteEventHub::DragCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(aReleaseEventCreator(x3, y3),
                           MockCopyPasteEventHub::NoActionState(),
                           nsEventStatus_eConsumeNoDefault);
}

TEST_F(CopyPasteEventHubTester, TestMouseLongTapWithSelectWordSuccessful)
{
  TestLongTapWithSelectWordSuccessful(CreateMousePressEvent,
                                      CreateMouseReleaseEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void
CopyPasteEventHubTester::TestLongTapWithSelectWordSuccessful(
  PressEventCreator aPressEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), PressCaret(_))
      .WillOnce(Return(NS_ERROR_FAILURE));

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), SelectWordOrShortcut(_))
      .WillOnce(Return(NS_OK));
  }

  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockCopyPasteEventHub::WaitLongTapState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(CreateLongTapEvent(0, 0),
                           MockCopyPasteEventHub::NoActionState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockCopyPasteEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(CopyPasteEventHubTester, TestMouseLongTapWithSelectWordFailed)
{
  TestLongTapWithSelectWordFailed(CreateMousePressEvent,
                                  CreateMouseReleaseEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void
CopyPasteEventHubTester::TestLongTapWithSelectWordFailed(
  PressEventCreator aPressEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), PressCaret(_))
      .WillOnce(Return(NS_ERROR_FAILURE));

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), SelectWordOrShortcut(_))
      .WillOnce(Return(NS_ERROR_FAILURE));
  }

  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockCopyPasteEventHub::WaitLongTapState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(CreateLongTapEvent(0, 0),
                           MockCopyPasteEventHub::NoActionState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockCopyPasteEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

}; // namespace mozilla

/*
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
*/
