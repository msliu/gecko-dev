/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 *
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

[Global=(Worker,ServiceWorker),
 Exposed=ServiceWorker]
interface ServiceWorkerGlobalScope : WorkerGlobalScope {
  readonly attribute Clients clients;
  readonly attribute ServiceWorkerRegistration registration;

  attribute EventHandler oninstall;
  attribute EventHandler onactivate;
  attribute EventHandler onfetch;
  attribute EventHandler onbeforeevicted;
  attribute EventHandler onevicted;

  // The event.source of these MessageEvents are instances of Client
  attribute EventHandler onmessage;
};


