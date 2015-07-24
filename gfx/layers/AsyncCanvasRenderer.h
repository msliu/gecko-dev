/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_ASYNCCANVASRENDERER_H_
#define MOZILLA_LAYERS_ASYNCCANVASRENDERER_H_

#include "LayersTypes.h"
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"             // for nsAutoPtr, nsRefPtr, etc
#include "nsCOMPtr.h"                   // for nsCOMPtr

class nsICanvasRenderingContextInternal;
class nsIInputStream;
class nsIThread;

namespace mozilla {

namespace gfx {
class DataSourceSurface;
}

namespace gl {
class GLContext;
}

namespace dom {
class HTMLCanvasElement;
namespace workers {
class WorkerPrivate;
}
}

namespace layers {

class CanvasClient;

/**
 * Since HTMLCanvasElement and OffscreenCanvas are not thread-safe, we create
 * AsyncCanvasRenderer which is thread-safe wrapper object for communicating
 * among main, worker and ImageBridgeChild threads.
 *
 * Each HTMLCanvasElement object is responsible for creating
 * AsyncCanvasRenderer object. Once Canvas is transfered to worker,
 * OffscreenCanvas will keep reference pointer of this object.
 *
 * If layers backend is LAYERS_BASIC, we readback webgl's result to mSurface by
 * calling UpdateTarget(). Otherwise, this object will pass to ImageBridgeChild
 * for submitting frames to Compositor.
 */
class AsyncCanvasRenderer final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncCanvasRenderer)

public:
  AsyncCanvasRenderer();

  void NotifyElementAboutAttributesChanged();
  void NotifyElementAboutInvalidation();

  void SetCanvasClient(CanvasClient* aClient);

  void SetWidth(uint32_t aWidth)
  {
    mWidth = aWidth;
  }

  void SetHeight(uint32_t aHeight)
  {
    mHeight = aHeight;
  }

  void SetIsAlphaPremultiplied(bool aIsAlphaPremultiplied)
  {
    mIsAlphaPremultiplied = aIsAlphaPremultiplied;
  }

  void SetActiveThread();
  void ResetActiveThread();

  void UpdateTarget();

  already_AddRefed<gfx::DataSourceSurface> GetSurface();

  nsresult
  GetInputStream(const char *aMimeType,
                 const char16_t *aEncoderOptions,
                 nsIInputStream **aStream);

  gfx::IntSize GetSize() const
  {
    return gfx::IntSize(mWidth, mHeight);
  }

  uint64_t GetCanvasClientAsyncID() const
  {
    return mCanvasClientAsyncID;
  }

  CanvasClient* GetCanvasClient() const
  {
    return mCanvasClient;
  }

  nsIThread* GetActiveThread()
  {
    return mActiveThread;
  }

  // Indicate the backend type of layer which belong to this renderer
  LayersBackend mBackend;

  // The lifetime is controllered by HTMLCanvasElement.
  dom::HTMLCanvasElement* mHTMLCanvasElement;

  nsICanvasRenderingContextInternal* mContext;

  // We need to keep a reference to the context around here, otherwise the
  // canvas' surface texture destructor will deref and destroy it too early
  RefPtr<gl::GLContext> mGLContext;

  class GetSurfaceHelper final
  {
  public:
    explicit GetSurfaceHelper(AsyncCanvasRenderer* aRenderer)
      : mRenderer(aRenderer)
    {
      if (mRenderer) {
        mRenderer->UpdateTarget();
        mRenderer->LockSurfaceMutex();
      }
    }

    ~GetSurfaceHelper()
    {
      if (mRenderer) {
        mRenderer->UnlockSurfaceMutex();
      }
    }

  private:
    AsyncCanvasRenderer* mRenderer;
  };

private:

  virtual ~AsyncCanvasRenderer();

  void LockSurfaceMutex();
  void UnlockSurfaceMutex();

  bool mIsAlphaPremultiplied;
  bool mIsSurfaceMutexLocked;

  uint32_t mWidth;
  uint32_t mHeight;
  uint64_t mCanvasClientAsyncID;

  // The lifetime of this pointer is controlled by OffscreenCanvas
  CanvasClient* mCanvasClient;

  // When backend is LAYER_BASIC, which means we need fallback to
  // BasicCanvasLayer. BasicCanvasLayer need a surface which contains
  // the result of OffscreenCanvas. So we store result in mSurface by
  // calling UpdateTarget().
  RefPtr<gfx::DataSourceSurface> mSurface;

  // When layers backend is LAYER_BASIC, worker thread will produce frame to
  // mSurface. Main thread will acquire frame from mSurface in order to
  // display frame to screen. To avoid race condition between these two
  // threads, using mutex to protect mSurface.
  Mutex mSurfaceMutex;

  nsCOMPtr<nsIThread> mActiveThread;
  dom::workers::WorkerPrivate* mActiveWorkerPrivate;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_ASYNCCANVASRENDERER_H_
