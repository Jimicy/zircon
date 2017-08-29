// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <magenta/compiler.h>

#define VIRTIO_STATUS_ACKNOWLEDGE           (1u << 0)
#define VIRTIO_STATUS_DRIVER                (1u << 1)
#define VIRTIO_STATUS_DRIVER_OK             (1u << 2)
#define VIRTIO_STATUS_FEATURES_OK           (1u << 3)
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET    (1u << 6)
#define VIRTIO_STATUS_FAILED                (1u << 7)

// PCI IO space for transitional virtio devices
#define VIRTIO_PCI_DEVICE_FEATURES          0x0     // uint32_t
#define VIRTIO_PCI_DRIVER_FEATURES          0x4     // uint32_t
#define VIRTIO_PCI_QUEUE_PFN                0x8     // uint32_t
#define VIRTIO_PCI_QUEUE_SIZE               0xc     // uint16_t
#define VIRTIO_PCI_QUEUE_SELECT             0xe     // uint16_t
#define VIRTIO_PCI_QUEUE_NOTIFY             0x10    // uint16_t
#define VIRTIO_PCI_DEVICE_STATUS            0x12    // uint8_t
#define VIRTIO_PCI_ISR_STATUS               0x13    // uint8_t
#define VIRTIO_PCI_MSI_CONFIG_VECTOR        0x14    // uint16_t
#define VIRTIO_PCI_MSI_QUEUE_VECTOR         0x16    // uint16_t

#define VIRTIO_PCI_CONFIG_OFFSET_NOMSI      0x14    // uint16_t
#define VIRTIO_PCI_CONFIG_OFFSET_MSI        0x18    // uint16_t

#define VIRTIO_PCI_CAP_COMMON_CFG           1
#define VIRTIO_PCI_CAP_NOTIFY_CFG           2
#define VIRTIO_PCI_CAP_ISR_CFG              3
#define VIRTIO_PCI_CAP_DEVICE_CFG           4
#define VIRTIO_PCI_CAP_PCI_CFG              5

__BEGIN_CDECLS

typedef struct virtio_pci_legacy_config {
    uint32_t device_features;
    uint32_t guest_features;
    uint32_t queue_address;
    uint16_t queue_size;
    uint16_t queue_select;
    uint16_t queue_notify;
    uint8_t device_status;
    uint8_t isr_status;
} __PACKED virtio_pci_legacy_config_t;

typedef struct virtio_pci_cap {
    uint8_t cap_vndr;
    uint8_t cap_next;
    uint8_t cap_len;
    uint8_t cfg_type;
    uint8_t bar;
    uint8_t padding[3];
    uint32_t offset;
    uint32_t length;
} __PACKED virtio_pci_cap_t;

typedef struct virtio_pci_notify_cap {
  virtio_pci_cap_t cap;
  uint32_t notify_off_multiplier;
} __PACKED virtio_pci_notify_cap_t;

typedef struct virtio_pci_common_cfg {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;

    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_avail;
    uint64_t queue_used;
} __PACKED virtio_pci_common_cfg_t;

__END_CDECLS
