/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <iostream>
#include <string>

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
using ::testing::MockFunction;
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
  MOCK_METHOD0(OnScrollStart, void());
  MOCK_METHOD0(OnScrollEnd, void());
};

class MockCopyPasteEventHub : public CopyPasteEventHub
{
public:
  using CopyPasteEventHub::NoActionState;
  using CopyPasteEventHub::PressCaretState;
  using CopyPasteEventHub::DragCaretState;
  using CopyPasteEventHub::PressNoCaretState;
  using CopyPasteEventHub::ScrollState;

  explicit MockCopyPasteEventHub() : CopyPasteEventHub()
  {
    mHandler = MakeUnique<MockCopyPasteManager>(nullptr);
    mInitialized = true;
  }

  MockCopyPasteManager* GetMockCopyPasteManager()
  {
    return reinterpret_cast<MockCopyPasteManager*>(mHandler.get());
  }

  void SetAsyncPanZoomEnabled(bool aEnabled)
  {
    mAsyncPanZoomEnabled = aEnabled;
  }
};

::std::ostream& operator<<(::std::ostream& aOstream,
                           const MockCopyPasteEventHub::State* aState)
{
  return aOstream << aState->Name();
}

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

  static UniquePtr<WidgetEvent> CreateTouchEvent(uint32_t aMessage, nscoord aX,
                                                 nscoord aY)
  {
    auto event = MakeUnique<WidgetTouchEvent>(true, aMessage, nullptr);
    int32_t identifier = 0;
    nsIntPoint point(aX, aY);
    nsIntPoint radius(19, 19);
    float rotationAngle = 0;
    float force = 1;

    nsRefPtr<dom::Touch> touch(
      new dom::Touch(identifier, point, radius, rotationAngle, force));
    event->touches.AppendElement(touch);

    return Move(event);
  }

  static UniquePtr<WidgetEvent> CreateTouchPressEvent(nscoord aX, nscoord aY)
  {
    return CreateTouchEvent(NS_TOUCH_START, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateTouchMoveEvent(nscoord aX, nscoord aY)
  {
    return CreateTouchEvent(NS_TOUCH_MOVE, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateTouchReleaseEvent(nscoord aX, nscoord aY)
  {
    return CreateTouchEvent(NS_TOUCH_END, aX, aY);
  }

  void HandleEventAndCheckState(UniquePtr<WidgetEvent> aEvent,
                                MockCopyPasteEventHub::State* aExpectedState,
                                nsEventStatus aExpectedEventStatus)
  {
    nsEventStatus rv = mHub->HandleEvent(aEvent.get());
    EXPECT_EQ(mHub->GetState(), aExpectedState);
    EXPECT_EQ(rv, aExpectedEventStatus);
  }

  void CheckState(MockCopyPasteEventHub::State* aExpectedState)
  {
    EXPECT_EQ(mHub->GetState(), aExpectedState);
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

  template <typename PressEventCreator, typename MoveEventCreator,
            typename ReleaseEventCreator>
  void TestEventDrivenAsyncPanZoomScroll(
    PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
    ReleaseEventCreator aReleaseEventCreator);

  nsRefPtr<MockCopyPasteEventHub> mHub;
}; // class CopyPasteEventHubTester

TEST_F(CopyPasteEventHubTester, TestMousePressReleaseNotOnCaret)
{
  TestPressReleaseNotOnCaret(CreateMousePressEvent, CreateMouseReleaseEvent);
}

TEST_F(CopyPasteEventHubTester, TestTouchPressReleaseNotOnCaret)
{
  TestPressReleaseNotOnCaret(CreateTouchPressEvent, CreateTouchReleaseEvent);
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
                           MockCopyPasteEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockCopyPasteEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(CopyPasteEventHubTester, TestMousePressReleaseOnCaret)
{
  TestPressReleaseOnCaret(CreateMousePressEvent, CreateMouseReleaseEvent);
}

TEST_F(CopyPasteEventHubTester, TestTouchPressReleaseOnCaret)
{
  TestPressReleaseOnCaret(CreateTouchPressEvent, CreateTouchReleaseEvent);
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

TEST_F(CopyPasteEventHubTester, TestTouchPressMoveReleaseOnCaret)
{
  TestPressMoveReleaseOnCaret(CreateTouchPressEvent, CreateTouchMoveEvent,
                              CreateTouchReleaseEvent);
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

TEST_F(CopyPasteEventHubTester, TestTouchLongTapWithSelectWordSuccessful)
{
  TestLongTapWithSelectWordSuccessful(CreateTouchPressEvent,
                                      CreateTouchReleaseEvent);
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
                           MockCopyPasteEventHub::PressNoCaretState(),
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

TEST_F(CopyPasteEventHubTester, TestTouchLongTapWithSelectWordFailed)
{
  TestLongTapWithSelectWordFailed(CreateTouchPressEvent,
                                  CreateTouchReleaseEvent);
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
                           MockCopyPasteEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(CreateLongTapEvent(0, 0),
                           MockCopyPasteEventHub::NoActionState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockCopyPasteEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(CopyPasteEventHubTester, TestTouchEventDrivenAsyncPanZoomScroll)
{
  TestEventDrivenAsyncPanZoomScroll(CreateTouchPressEvent, CreateTouchMoveEvent,
                                    CreateTouchReleaseEvent);
}

TEST_F(CopyPasteEventHubTester, TestMouseEventDrivenAsyncPanZoomScroll)
{
  TestEventDrivenAsyncPanZoomScroll(CreateMousePressEvent, CreateMouseMoveEvent,
                                    CreateMouseReleaseEvent);
}

template <typename PressEventCreator, typename MoveEventCreator,
          typename ReleaseEventCreator>
void
CopyPasteEventHubTester::TestEventDrivenAsyncPanZoomScroll(
  PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  MockFunction<void(::std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), PressCaret(_))
      .WillOnce(Return(NS_ERROR_FAILURE));

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), DragCaret(_))
      .Times(0);

    EXPECT_CALL(check, Call("Before scroll start"));
    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), OnScrollEnd());
    EXPECT_CALL(check, Call("After scroll start"));
  }

  mHub->SetAsyncPanZoomEnabled(true);

  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockCopyPasteEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aMoveEventCreator(100, 100),
                           MockCopyPasteEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  check.Call("Before scroll start");

  mHub->AsyncPanZoomStarted(CSSIntPoint(150, 150));
  CheckState(MockCopyPasteEventHub::ScrollState());

  HandleEventAndCheckState(aMoveEventCreator(160, 160),
                           MockCopyPasteEventHub::ScrollState(),
                           nsEventStatus_eIgnore);

  mHub->ScrollPositionChanged();
  CheckState(MockCopyPasteEventHub::ScrollState());

  mHub->AsyncPanZoomStopped(CSSIntPoint(200, 200));
  CheckState(MockCopyPasteEventHub::NoActionState());

  check.Call("After scroll start");
}

TEST_F(CopyPasteEventHubTester, TestMomentumDrivenAsyncPanZoomScroll)
{
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockCopyPasteManager(), OnScrollEnd());
  }

  mHub->AsyncPanZoomStarted(CSSIntPoint(150, 150));
  CheckState(MockCopyPasteEventHub::ScrollState());

  mHub->ScrollPositionChanged();
  CheckState(MockCopyPasteEventHub::ScrollState());

  mHub->AsyncPanZoomStopped(CSSIntPoint(200, 200));
  CheckState(MockCopyPasteEventHub::NoActionState());
}

}; // namespace mozilla
