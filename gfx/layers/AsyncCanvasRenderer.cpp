/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncCanvasRenderer.h"

#include "gfxUtils.h"
#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/ImageEncoder.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/layers/CanvasClient.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace layers {

AsyncCanvasRenderer::AsyncCanvasRenderer()
  : mIsAlphaPremultiplied(true)
  , mIsSurfaceMutexLocked(false)
  , mWidth(0)
  , mHeight(0)
  , mCanvasClientAsyncID(0)
  , mCanvasClient(nullptr)
  , mSurfaceMutex("AsyncCanvasRenderer::mSurface mutex")
{
  MOZ_COUNT_CTOR(AsyncCanvasRenderer);
}

AsyncCanvasRenderer::~AsyncCanvasRenderer()
{
  MOZ_COUNT_DTOR(AsyncCanvasRenderer);
}

void
AsyncCanvasRenderer::NotifyElementAboutAttributesChanged()
{
  class Runnable final : public nsRunnable
  {
  public:
    Runnable(AsyncCanvasRenderer* aRenderer)
      : mRenderer(aRenderer)
    {}

    NS_IMETHOD Run()
    {
      if (mRenderer) {
        dom::HTMLCanvasElement::SetAttrFromAsyncCanvasRenderer(mRenderer);
      }

      return NS_OK;
    }

    void Revoke()
    {
      mRenderer = nullptr;
    }

  private:
    nsRefPtr<AsyncCanvasRenderer> mRenderer;
  };

  nsRefPtr<nsRunnable> runnable = new Runnable(this);
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch a runnable to the main-thread.");
  }
}

void
AsyncCanvasRenderer::NotifyElementAboutInvalidation()
{
  class Runnable final : public nsRunnable
  {
  public:
    Runnable(AsyncCanvasRenderer* aRenderer)
      : mRenderer(aRenderer)
    {}

    NS_IMETHOD Run()
    {
      if (mRenderer) {
        dom::HTMLCanvasElement::InvalidateFromAsyncCanvasRenderer(mRenderer);
      }

      return NS_OK;
    }

    void Revoke()
    {
      mRenderer = nullptr;
    }

  private:
    nsRefPtr<AsyncCanvasRenderer> mRenderer;
  };

  nsRefPtr<nsRunnable> runnable = new Runnable(this);
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch a runnable to the main-thread.");
  }
}

void
AsyncCanvasRenderer::SetCanvasClient(CanvasClient* aClient)
{
  mCanvasClient = aClient;
  if (aClient) {
    mCanvasClientAsyncID = aClient->GetAsyncID();
  } else {
    mCanvasClientAsyncID = 0;
  }
}

void
AsyncCanvasRenderer::SetActiveThread()
{
  mActiveThread = NS_GetCurrentThread();
  mActiveWorkerPrivate = dom::workers::GetCurrentThreadWorkerPrivate();
}

void
AsyncCanvasRenderer::ResetActiveThread()
{
  mActiveThread = nullptr;
  mActiveWorkerPrivate = nullptr;
}

void
AsyncCanvasRenderer::UpdateTarget()
{
  class UpdateTargetRunnable final : public dom::workers::MainThreadWorkerControlRunnable
  {
  public:
    UpdateTargetRunnable(dom::workers::WorkerPrivate* aWorkerPrivate,
                         AsyncCanvasRenderer* aRenderer,
                         ReentrantMonitor* aBarrier,
                         bool* aDone)
      : MainThreadWorkerControlRunnable(aWorkerPrivate)
      , mRenderer(aRenderer)
      , mBarrier(aBarrier)
      , mDone(aDone)
    {
    }

    bool WorkerRun(JSContext* aCx, dom::workers::WorkerPrivate* aWorkerPrivate) override
    {
      ReentrantMonitorAutoEnter autoMon(*mBarrier);
      if (mRenderer) {
        mRenderer->UpdateTarget();
      }
      *mDone = true;
      mBarrier->NotifyAll();
      return true;
    }

  protected:
    ~UpdateTargetRunnable() {}

  private:
    RefPtr<AsyncCanvasRenderer> mRenderer;
    ReentrantMonitor* mBarrier;
    bool* mDone;
  };

  if (mActiveThread && mActiveThread != NS_GetCurrentThread()) {
    ReentrantMonitor barrier("UpdateTargetRunnable Lock");
    ReentrantMonitorAutoEnter autoMon(barrier);
    bool done = false;
    nsRefPtr<UpdateTargetRunnable> runnable =
      new UpdateTargetRunnable(mActiveWorkerPrivate, this, &barrier, &done);

    AutoSafeJSContext cx;
    if (!runnable->Dispatch(cx)) {
      NS_WARNING("Could not dispatch UpdateTargetRunnable");
      return;
    }

    while (!done) {
      barrier.Wait();
    }

    return;
  }

  MutexAutoLock lock(mSurfaceMutex);
  if (!mGLContext) {
    return;
  }

  gl::SharedSurface* frontbuffer = nullptr;
  gl::GLScreenBuffer* screen = mGLContext->Screen();
  const auto& front = screen->Front();
  if (front) {
    frontbuffer = front->Surf();
  }

  if (!frontbuffer) {
    NS_WARNING("Null frame received.");
    return;
  }

  gfx::IntSize readSize(frontbuffer->mSize);
  gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
  bool needsPremult = frontbuffer->mHasAlpha && !mIsAlphaPremultiplied;

  if (!mSurface ||
      readSize != mSurface->GetSize() ||
      format != mSurface->GetFormat())
  {
    // Create a surface aligned to 8 bytes since that's the highest
    // alignment WebGL can handle.
    uint32_t stride = gfx::GetAlignedStride<8>(readSize.width * BytesPerPixel(format));
    mSurface = gfx::Factory::CreateDataSourceSurfaceWithStride(readSize, format, stride);
  }

  if (NS_WARN_IF(!mSurface)) {
    return;
  }

  // Readback handles Flush/MarkDirty.
  mGLContext->Readback(frontbuffer, mSurface);
  if (needsPremult) {
    gfxUtils::PremultiplyDataSurface(mSurface, mSurface);
  }
}

void
AsyncCanvasRenderer::LockSurfaceMutex()
{
  mSurfaceMutex.Lock();
  mIsSurfaceMutexLocked = true;
}

void
AsyncCanvasRenderer::UnlockSurfaceMutex()
{
  mIsSurfaceMutexLocked = false;
  mSurfaceMutex.Unlock();
}

already_AddRefed<gfx::DataSourceSurface>
AsyncCanvasRenderer::GetSurface()
{
  MOZ_RELEASE_ASSERT(mIsSurfaceMutexLocked);
  RefPtr<gfx::DataSourceSurface> result = mSurface;
  return result.forget();
}


nsresult
AsyncCanvasRenderer::GetInputStream(const char *aMimeType,
                                    const char16_t *aEncoderOptions,
                                    nsIInputStream **aStream)
{
  nsCString enccid("@mozilla.org/image/encoder;2?type=");
  enccid += aMimeType;
  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(enccid.get());
  if (!encoder) {
    return NS_ERROR_FAILURE;
  }

  nsAutoArrayPtr<uint8_t> imageBuffer;
  int32_t format = 0;
  GetSurfaceHelper helper(this);
  if (!mSurface) {
    return NS_ERROR_FAILURE;
  }

  // Handle y flip.
  RefPtr<gfx::DrawTarget> dt =
    gfx::Factory::CreateDrawTarget(gfx::BackendType::CAIRO,
                                   gfx::IntSize(mWidth, mHeight),
                                   gfx::SurfaceFormat::B8G8R8A8);

  if (!dt) {
    return NS_ERROR_FAILURE;
  }

  dt->SetTransform(gfx::Matrix::Translation(0.0, mHeight).PreScale(1.0, -1.0));

  dt->DrawSurface(mSurface,
                  gfx::Rect(0, 0, mWidth, mHeight),
                  gfx::Rect(0, 0, mWidth, mHeight),
                  gfx::DrawSurfaceOptions(),
                  gfx::DrawOptions(1.0f, gfx::CompositionOp::OP_SOURCE));

  RefPtr<gfx::SourceSurface> surf = dt->Snapshot();
  RefPtr<gfx::DataSourceSurface> dataSurf = surf->GetDataSurface();

  gfx::DataSourceSurface::MappedSurface map;
  if (!dataSurf->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
    return NS_ERROR_FAILURE;
  }

  imageBuffer = new (fallible) uint8_t[mWidth * mHeight * 4];
  if (!imageBuffer) {
    dataSurf->Unmap();
    return NS_ERROR_FAILURE;
  }
  memcpy(imageBuffer, map.mData, mWidth * mHeight * 4);

  dataSurf->Unmap();

  format = imgIEncoder::INPUT_FORMAT_HOSTARGB;

  return dom::ImageEncoder::GetInputStream(mWidth, mHeight, imageBuffer, format,
                                           encoder, aEncoderOptions, aStream);
}

} // namespace layers
} // namespace mozilla
