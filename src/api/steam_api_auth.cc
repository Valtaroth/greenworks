// Copyright (c) 2016 Greenheart Games Pty. Ltd. All rights reserved.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "nan.h"
#include "steam/steam_api.h"
#include "steam/steamencryptedappticket.h"
#include "v8.h"

#include "greenworks_async_workers.h"
#include "steam_api_registry.h"
#include "steam_id.h"

namespace greenworks {
namespace api {
namespace {

NAN_METHOD(GetAuthSessionTicket) {
  Nan::HandleScope scope;
  if (info.Length() < 1 || !info[0]->IsFunction()) {
    THROW_BAD_ARGS("Bad arguments");
  }
  Nan::Callback* success_callback =
      new Nan::Callback(info[0].As<v8::Function>());
  Nan::Callback* error_callback = NULL;
  if (info.Length() > 1 && info[1]->IsFunction())
    error_callback = new Nan::Callback(info[1].As<v8::Function>());
  Nan::AsyncQueueWorker(new greenworks::GetAuthSessionTicketWorker(
    success_callback, error_callback));
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(CancelAuthTicket) {
  Nan::HandleScope scope;
  if (info.Length() < 1 || !info[0]->IsNumber()) {
    THROW_BAD_ARGS("Bad arguments");
  }
  HAuthTicket h = info[1].As<v8::Number>()->Int32Value();
  SteamUser()->CancelAuthTicket(h);
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(GetEncryptedAppTicket) {
  Nan::HandleScope scope;
  if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
    THROW_BAD_ARGS("Bad arguments");
  }
  char* user_data = *(static_cast<v8::String::Utf8Value>(info[0]->ToString()));
  if (!user_data) {
    THROW_BAD_ARGS("Bad arguments");
  }
  Nan::Callback* success_callback =
      new Nan::Callback(info[1].As<v8::Function>());
  Nan::Callback* error_callback = NULL;
  if (info.Length() > 2 && info[2]->IsFunction())
    error_callback = new Nan::Callback(info[2].As<v8::Function>());
  Nan::AsyncQueueWorker(new greenworks::RequestEncryptedAppTicketWorker(
    user_data, success_callback, error_callback));
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(DecryptAppTicket) {
  Nan::HandleScope scope;
  if (info.Length() < 2 || !node::Buffer::HasInstance(info[0]) ||
      !node::Buffer::HasInstance(info[1])) {
    THROW_BAD_ARGS("Bad arguments");
  }
  char* encrypted_ticket_buf = node::Buffer::Data(info[0]);
  size_t encrypted_ticket_buf_size = node::Buffer::Length(info[0]);
  char* key_buf = node::Buffer::Data(info[1]);
  if (node::Buffer::Length(info[1]) !=
      k_nSteamEncryptedAppTicketSymmetricKeyLen) {
    THROW_BAD_ARGS("The key length is not matched");
  }
  uint8 key[k_nSteamEncryptedAppTicketSymmetricKeyLen];
  memcpy(key, key_buf, k_nSteamEncryptedAppTicketSymmetricKeyLen);

  uint8 decrypted_ticket[1024];
  uint32 decrypted_ticket_size = 1024;
  bool is_success = SteamEncryptedAppTicket_BDecryptTicket(
      reinterpret_cast<const uint8*>(encrypted_ticket_buf),
      encrypted_ticket_buf_size, decrypted_ticket, &decrypted_ticket_size, key,
      k_nSteamEncryptedAppTicketSymmetricKeyLen);

  if (!is_success) {
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }
  info.GetReturnValue().Set(
      Nan::CopyBuffer(reinterpret_cast<const char *>(decrypted_ticket),
                      decrypted_ticket_size)
          .ToLocalChecked());
}

NAN_METHOD(IsTicketForApp) {
  Nan::HandleScope scope;
  if (info.Length() < 2 || !node::Buffer::HasInstance(info[0]) ||
      info[1]->IsUint32()) {
    THROW_BAD_ARGS("Bad arguments");
  }

  char* decrypted_ticket_buf = node::Buffer::Data(info[0]);
  size_t decrypted_ticket_buf_size = node::Buffer::Length(info[0]);
  uint32 app_id = info[1]->Uint32Value();
  bool result = SteamEncryptedAppTicket_BIsTicketForApp(
      reinterpret_cast<uint8*>(decrypted_ticket_buf), decrypted_ticket_buf_size,
      app_id);
  info.GetReturnValue().Set(result);
}

NAN_METHOD(getTicketIssueTime) {
  Nan::HandleScope scope;
  if (info.Length() < 1 || !node::Buffer::HasInstance(info[0])) {
    THROW_BAD_ARGS("Bad arguments");
  }
  char* decrypted_ticket_buf = node::Buffer::Data(info[0]);
  size_t decrypted_ticket_buf_size = node::Buffer::Length(info[0]);
  uint32 time = SteamEncryptedAppTicket_GetTicketIssueTime(
      reinterpret_cast<uint8*>(decrypted_ticket_buf),
      decrypted_ticket_buf_size);
  info.GetReturnValue().Set(time);
}

NAN_METHOD(getTicketSteamId) {
  Nan::HandleScope scope;
  if (info.Length() < 1 || !node::Buffer::HasInstance(info[0])) {
    THROW_BAD_ARGS("Bad arguments");
  }
  char* decrypted_ticket_buf = node::Buffer::Data(info[0]);
  size_t decrypted_ticket_buf_size = node::Buffer::Length(info[0]);
  CSteamID steam_id;
  SteamEncryptedAppTicket_GetTicketSteamID(
      reinterpret_cast<uint8*>(decrypted_ticket_buf), decrypted_ticket_buf_size,
      &steam_id);
  info.GetReturnValue().Set(greenworks::SteamID::Create(steam_id));
}

NAN_METHOD(getTicketAppId) {
  Nan::HandleScope scope;
  if (info.Length() < 1 || !node::Buffer::HasInstance(info[0])) {
    THROW_BAD_ARGS("Bad arguments");
  }
  char* decrypted_ticket_buf = node::Buffer::Data(info[0]);
  size_t decrypted_ticket_buf_size = node::Buffer::Length(info[0]);
  uint32 app_id = SteamEncryptedAppTicket_GetTicketAppID(
      reinterpret_cast<uint8*>(decrypted_ticket_buf),
      decrypted_ticket_buf_size);
  info.GetReturnValue().Set(app_id);
}

void RegisterAPIs(v8::Handle<v8::Object> target) {
  Nan::Set(target,
           Nan::New("getAuthSessionTicket").ToLocalChecked(),
           Nan::New<v8::FunctionTemplate>(GetAuthSessionTicket)->GetFunction());
  Nan::Set(target,
           Nan::New("getEncryptedAppTicket").ToLocalChecked(),
           Nan::New<v8::FunctionTemplate>(
               GetEncryptedAppTicket)->GetFunction());
  Nan::Set(target,
           Nan::New("decryptAppTicket").ToLocalChecked(),
           Nan::New<v8::FunctionTemplate>(
               DecryptAppTicket)->GetFunction());
  Nan::Set(target,
           Nan::New("isTicketForApp").ToLocalChecked(),
           Nan::New<v8::FunctionTemplate>(
               IsTicketForApp)->GetFunction());
  Nan::Set(target,
           Nan::New("getTicketIssueTime").ToLocalChecked(),
           Nan::New<v8::FunctionTemplate>(
               getTicketIssueTime)->GetFunction());
  Nan::Set(target,
           Nan::New("getTicketSteamId").ToLocalChecked(),
           Nan::New<v8::FunctionTemplate>(
               getTicketSteamId)->GetFunction());
  Nan::Set(target,
           Nan::New("getTicketAppId").ToLocalChecked(),
           Nan::New<v8::FunctionTemplate>(
               getTicketAppId)->GetFunction());
  Nan::Set(target,
           Nan::New("cancelAuthTicket").ToLocalChecked(),
           Nan::New<v8::FunctionTemplate>(CancelAuthTicket)->GetFunction());
}

SteamAPIRegistry::Add X(RegisterAPIs);

}  // namespace
}  // namespace api
}  // namespace greenworks
