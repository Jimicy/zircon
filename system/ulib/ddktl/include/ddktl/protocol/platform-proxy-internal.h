// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <fbl/type_support.h>

namespace ddk {
namespace internal {

DECLARE_HAS_MEMBER_FN_WITH_SIGNATURE(has_platform_proxy_set_protocol, SetProtocol,
        zx_status_t (C::*)(uint32_t proto_id, platform_proxy_cb proxy_cb));

template <typename D>
constexpr void CheckPlatformProxyProtocolSubclass() {
    static_assert(internal::has_platform_proxy_set_protocol<D>::value,
                  "PlatformProxyProtocol subclasses must implement "
                  "SetProtocol(uint32_t proto_id, platform_proxy_cb proxy_cb)");
 }

}  // namespace internal
}  // namespace ddk
