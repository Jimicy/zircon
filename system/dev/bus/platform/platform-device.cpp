// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform-device.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/binding.h>
#include <ddk/metadata.h>
#include <ddk/protocol/platform-defs.h>
#include <fbl/function.h>
#include <zircon/syscalls/resource.h>

#include "platform-bus.h"

namespace platform_bus {

zx_status_t PlatformDevice::Create(const pbus_dev_t* pdev, zx_device_t* parent, PlatformBus* bus,
                                   uint32_t flags,
                                   fbl::unique_ptr<platform_bus::PlatformDevice>* out) {
    fbl::AllocChecker ac;
    fbl::unique_ptr<platform_bus::PlatformDevice> dev(new (&ac)
                                            platform_bus::PlatformDevice(parent, bus, flags, pdev));
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }
    auto status = dev->Init(pdev);
    if (status != ZX_OK) {
        return status;
    }
    out->swap(dev);
    return ZX_OK;
}

PlatformDevice::PlatformDevice(zx_device_t* parent, PlatformBus* bus, uint32_t flags,
                               const pbus_dev_t* pdev)
    : PlatformDeviceType(parent), bus_(bus), flags_(flags), vid_(pdev->vid), pid_(pdev->pid),
      did_(pdev->did), serial_port_info_(pdev->serial_port_info), resource_tree_(ROOT_DEVICE_ID) {
    strlcpy(name_, pdev->name, sizeof(name_));
}

zx_status_t PlatformDevice::Init(const pbus_dev_t* pdev) {
    uint32_t next_device_id = ROOT_DEVICE_ID + 1;
    auto status = resource_tree_.Init(pdev, &next_device_id);
    if (status != ZX_OK) {
        return status;
    }

    fbl::AllocChecker ac;
    device_index_.reserve(resource_tree_.DeviceCount(), &ac);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }
    resource_tree_.BuildDeviceIndex(&device_index_);

    return ZX_OK;
}


zx_status_t PlatformDevice::MapMmio(uint32_t index, uint32_t cache_policy, void** out_vaddr,
                                    size_t* out_size, zx_paddr_t* out_paddr,
                                    zx_handle_t* out_handle) {
    const DeviceResources* dr = device_index_[ROOT_DEVICE_ID];
    if (index >= dr->mmio_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    const pbus_mmio_t& mmio = dr->mmio(index);
    const zx_paddr_t vmo_base = ROUNDDOWN(mmio.base, PAGE_SIZE);
    const size_t vmo_size = ROUNDUP(mmio.base + mmio.length - vmo_base, PAGE_SIZE);
    zx_handle_t vmo_handle;
    zx_status_t status = zx_vmo_create_physical(bus_->GetResource(), vmo_base, vmo_size,
                                                &vmo_handle);
    if (status != ZX_OK) {
        zxlogf(ERROR, "platform_dev_map_mmio: zx_vmo_create_physical failed %d\n", status);
        return status;
    }

    status = zx_vmo_set_cache_policy(vmo_handle, cache_policy);
    if (status != ZX_OK) {
        zxlogf(ERROR, "platform_dev_map_mmio: zx_vmo_set_cache_policy failed %d\n", status);
        goto fail;
    }

    uintptr_t virt;
    status = zx_vmar_map(zx_vmar_root_self(), 0, vmo_handle, 0, vmo_size,
                         ZX_VM_FLAG_PERM_READ | ZX_VM_FLAG_PERM_WRITE | ZX_VM_FLAG_MAP_RANGE,
                         &virt);
    if (status != ZX_OK) {
        zxlogf(ERROR, "platform_dev_map_mmio: zx_vmar_map failed %d\n", status);
        goto fail;
    }

    *out_size = mmio.length;
    *out_handle = vmo_handle;
    if (out_paddr) {
        *out_paddr = vmo_base;
    }
    *out_vaddr = reinterpret_cast<void*>(virt + (mmio.base - vmo_base));
    return ZX_OK;

fail:
    zx_handle_close(vmo_handle);
    return status;
}

zx_status_t PlatformDevice::MapInterrupt(uint32_t index, uint32_t flags, zx_handle_t* out_handle) {
    const DeviceResources* dr = device_index_[ROOT_DEVICE_ID];
    if (index >= dr->irq_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    if (out_handle == nullptr) {
        return ZX_ERR_INVALID_ARGS;
    }

    const pbus_irq_t& irq = dr->irq(index);
    if (flags == 0) {
        flags = irq.mode;
    }
    zx_status_t status = zx_interrupt_create(bus_->GetResource(), irq.irq, flags, out_handle);
    if (status != ZX_OK) {
        zxlogf(ERROR, "platform_dev_map_interrupt: zx_interrupt_create failed %d\n", status);
        return status;
    }
    return status;
}

zx_status_t PlatformDevice::GetBti(uint32_t index, zx_handle_t* out_handle) {
    const DeviceResources* dr = device_index_[ROOT_DEVICE_ID];
    if (index >= dr->bti_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    if (out_handle == nullptr) {
        return ZX_ERR_INVALID_ARGS;
    }

    const pbus_bti_t& bti = dr->bti(index);

    return bus_->GetBti(bti.iommu_index, bti.bti_id, out_handle);
}

zx_status_t PlatformDevice::GetDeviceInfo(const DeviceResources* dr, pdev_device_info_t* out_info) {
    memset(out_info, 0, sizeof(*out_info));
    out_info->vid = vid_;
    out_info->pid = pid_;
    out_info->did = did_;
    memcpy(&out_info->serial_port_info, &serial_port_info_, sizeof(out_info->serial_port_info));
    out_info->mmio_count = static_cast<uint32_t>(dr->mmio_count());
    out_info->irq_count = static_cast<uint32_t>(dr->irq_count());
    out_info->gpio_count = static_cast<uint32_t>(dr->gpio_count());
    out_info->i2c_channel_count = static_cast<uint32_t>(dr->i2c_channel_count());
    out_info->clk_count = static_cast<uint32_t>(dr->clk_count());
    out_info->bti_count = static_cast<uint32_t>(dr->bti_count());
    out_info->metadata_count = static_cast<uint32_t>(dr->metadata_count());
    memcpy(out_info->name, name_, sizeof(out_info->name));

    return ZX_OK;
}

zx_status_t PlatformDevice::GetDeviceInfo(pdev_device_info_t* out_info) {
    return GetDeviceInfo(device_index_[ROOT_DEVICE_ID], out_info);
}

zx_status_t PlatformDevice::GetBoardInfo(pdev_board_info_t* out_info) {
    return bus_->GetBoardInfo(out_info);
}

zx_status_t PlatformDevice::DeviceAdd(uint32_t index, device_add_args_t* args, zx_device_t** out) {
    return ZX_ERR_NOT_SUPPORTED;
}

// Create a resource and pass it back to the proxy along with necessary metadata
// to create/map the VMO in the driver process.
zx_status_t PlatformDevice::RpcGetMmio(const DeviceResources* dr, uint32_t index, zx_paddr_t* out_paddr,
                                       size_t *out_length, zx_handle_t* out_handle,
                                       uint32_t* out_handle_count) {
    if (index >= dr->mmio_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    const pbus_mmio_t& mmio = dr->mmio(index);
    zx_handle_t handle;
    char rsrc_name[ZX_MAX_NAME_LEN];
    snprintf(rsrc_name, ZX_MAX_NAME_LEN-1, "%s.pbus[%u]", name_, index);
    zx_status_t status = zx_resource_create(bus_->GetResource(), ZX_RSRC_KIND_MMIO, mmio.base,
                                            mmio.length, rsrc_name, sizeof(rsrc_name), &handle);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: pdev_rpc_get_mmio: zx_resource_create failed: %d\n", name_, status);
        return status;
    }

    *out_paddr = mmio.base;
    *out_length = mmio.length;
    *out_handle_count = 1;
    *out_handle = handle;
    return ZX_OK;
}

// Create a resource and pass it back to the proxy along with necessary metadata
// to create the IRQ in the driver process.
zx_status_t PlatformDevice::RpcGetInterrupt(const DeviceResources* dr, uint32_t index,
                                            uint32_t* out_irq, uint32_t* out_mode,
                                            zx_handle_t* out_handle, uint32_t* out_handle_count) {
    if (index >= dr->irq_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    zx_handle_t handle;
    const pbus_irq_t& irq = dr->irq(index);
    uint32_t options = ZX_RSRC_KIND_IRQ | ZX_RSRC_FLAG_EXCLUSIVE;
    char rsrc_name[ZX_MAX_NAME_LEN];
    snprintf(rsrc_name, ZX_MAX_NAME_LEN-1, "%s.pbus[%u]", name_, index);
    zx_status_t status = zx_resource_create(bus_->GetResource(), options, irq.irq, 1, rsrc_name,
                                            sizeof(rsrc_name), &handle);
    if (status != ZX_OK) {
        return status;
    }

    *out_irq = irq.irq;
    *out_mode = irq.mode;
    *out_handle_count = 1;
    *out_handle = handle;
    return status;
}

zx_status_t PlatformDevice::RpcGetBti(const DeviceResources* dr, uint32_t index,
                                      zx_handle_t* out_handle, uint32_t* out_handle_count) {
    zx_status_t status = GetBti(index, out_handle);
    if (status == ZX_OK) {
        *out_handle_count = 1;
    }
    return status;
}

zx_status_t PlatformDevice::RpcDeviceAdd(const DeviceResources* dr, uint32_t index,
                                         uint32_t* out_device_id) {
    if (index >= dr->child_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    // TODO(voydanoff) verify that this device has not already been added?
    *out_device_id = dr->child_index(index);
    return ZX_OK;
}

zx_status_t PlatformDevice::RpcGetProtocols(const DeviceResources* dr, uint32_t* out_protocols,
                                            uint32_t* out_protocol_count) {
    auto count = dr->protocol_count();
    memcpy(out_protocols, dr->protocols(), count * sizeof(*out_protocols));
    *out_protocol_count = static_cast<uint32_t>(count);
    return ZX_OK;
}

zx_status_t PlatformDevice::RpcUmsSetMode(const DeviceResources* dr, usb_mode_t mode) {
    if (bus_->ums() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    return bus_->ums()->SetMode(mode);
}

zx_status_t PlatformDevice::RpcGpioConfig(const DeviceResources* dr, uint32_t index, uint32_t flags) {
    if (bus_->gpio() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (index >= dr->gpio_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    return bus_->gpio()->Config(dr->gpio(index).gpio, flags);
}

zx_status_t PlatformDevice::RpcGpioSetAltFunction(const DeviceResources* dr, uint32_t index,
                                                  uint64_t function) {
    if (bus_->gpio() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (index >= dr->gpio_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    return bus_->gpio()->SetAltFunction(dr->gpio(index).gpio, function);
}

zx_status_t PlatformDevice::RpcGpioRead(const DeviceResources* dr, uint32_t index,
                                        uint8_t* out_value) {
    if (bus_->gpio() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (index >= dr->gpio_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    return bus_->gpio()->Read(dr->gpio(index).gpio, out_value);
}

zx_status_t PlatformDevice::RpcGpioWrite(const DeviceResources* dr, uint32_t index, uint8_t value) {
    if (bus_->gpio() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (index >= dr->gpio_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    return bus_->gpio()->Write(dr->gpio(index).gpio, value);
}

zx_status_t PlatformDevice::RpcGpioGetInterrupt(const DeviceResources* dr, uint32_t index,
                                                uint32_t flags, zx_handle_t* out_handle,
                                                uint32_t* out_handle_count) {
    if (bus_->gpio() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (index >= dr->gpio_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    zx_status_t status = bus_->gpio()->GetInterrupt(dr->gpio(index).gpio, flags, out_handle);
    if (status == ZX_OK) {
        *out_handle_count = 1;
    }
    return status;
}

zx_status_t PlatformDevice::RpcGpioReleaseInterrupt(const DeviceResources* dr, uint32_t index) {
    if (bus_->gpio() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (index >= dr->gpio_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    return bus_->gpio()->ReleaseInterrupt(dr->gpio(index).gpio);
}

zx_status_t PlatformDevice::RpcGpioSetPolarity(const DeviceResources* dr, uint32_t index,
                                               uint32_t flags) {
    if (bus_->gpio() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (index >= dr->gpio_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    return bus_->gpio()->SetPolarity(dr->gpio(index).gpio, flags);
}

zx_status_t PlatformDevice::RpcCanvasConfig(const DeviceResources* dr, zx_handle_t vmo,
                                            size_t offset, canvas_info_t* info,
                                            uint8_t* canvas_idx) {
    if (bus_->canvas() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    return bus_->canvas()->Config(vmo, offset, info, canvas_idx);
}

zx_status_t PlatformDevice::RpcCanvasFree(const DeviceResources* dr, uint8_t canvas_idx) {
    if (bus_->canvas() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    return bus_->canvas()->Free(canvas_idx);
}

zx_status_t PlatformDevice::RpcScpiGetSensor(const DeviceResources* dr, char* name,
                                             uint32_t *sensor_id) {
    if (bus_->scpi() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    return bus_->scpi()->GetSensor(name, sensor_id);
}

zx_status_t PlatformDevice::RpcScpiGetSensorValue(const DeviceResources* dr, uint32_t sensor_id,
                                                  uint32_t* sensor_value) {
    if (bus_->scpi() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    return bus_->scpi()->GetSensorValue(sensor_id, sensor_value);
}

zx_status_t PlatformDevice::RpcScpiGetDvfsInfo(const DeviceResources* dr, uint8_t power_domain,
                                               scpi_opp_t* opps) {
    if (bus_->scpi() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    return bus_->scpi()->GetDvfsInfo(power_domain, opps);
}

zx_status_t PlatformDevice::RpcScpiGetDvfsIdx(const DeviceResources* dr, uint8_t power_domain,
                                              uint16_t* idx) {
    if (bus_->scpi() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    return bus_->scpi()->GetDvfsIdx(power_domain, idx);
}

zx_status_t PlatformDevice::RpcScpiSetDvfsIdx(const DeviceResources* dr, uint8_t power_domain,
                                              uint16_t idx) {
    if (bus_->scpi() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    return bus_->scpi()->SetDvfsIdx(power_domain, idx);
}

zx_status_t PlatformDevice::RpcI2cTransact(const DeviceResources* dr, uint32_t txid,
                                           rpc_i2c_req_t* req, uint8_t* data, zx_handle_t channel) {
    if (bus_->i2c_impl() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    uint32_t index = req->index;
    if (index >= dr->i2c_channel_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    const pbus_i2c_channel_t& pdev_channel = dr->i2c_channel(index);

    return bus_->I2cTransact(txid, req, &pdev_channel, data, channel);
}

zx_status_t PlatformDevice::RpcI2cGetMaxTransferSize(const DeviceResources* dr, uint32_t index,
                                                     size_t* out_size) {
    if (bus_->i2c_impl() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (index >= dr->i2c_channel_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    const pbus_i2c_channel_t& pdev_channel = dr->i2c_channel(index);

    return bus_->i2c_impl()->GetMaxTransferSize(pdev_channel.bus_id, out_size);
}

zx_status_t PlatformDevice::RpcClkEnable(const DeviceResources* dr, uint32_t index) {
    if (bus_->clk() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (index >= dr->clk_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    return bus_->clk()->Enable(dr->clk(index).clk);
}

zx_status_t PlatformDevice::RpcClkDisable(const DeviceResources* dr, uint32_t index) {
    if (bus_->clk() == nullptr) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (index >= dr->clk_count()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    return bus_->clk()->Disable(dr->clk(index).clk);
}

zx_status_t PlatformDevice::DdkRxrpc(zx_handle_t channel) {
    if (channel == ZX_HANDLE_INVALID) {
        // proxy device has connected
        return ZX_OK;
    }

    uint8_t req_buf[PROXY_MAX_TRANSFER_SIZE];
    uint8_t resp_buf[PROXY_MAX_TRANSFER_SIZE];
    auto* req_header = reinterpret_cast<rpc_req_header_t*>(&req_buf);
    auto* resp_header = reinterpret_cast<rpc_rsp_header_t*>(&resp_buf);
    uint32_t actual;
    zx_handle_t in_handle;
    uint32_t in_handle_count = 1;

    auto status = zx_channel_read(channel, 0, &req_buf, &in_handle, sizeof(req_buf),
                                  in_handle_count, &actual, &in_handle_count);
    if (status != ZX_OK) {
        zxlogf(ERROR, "platform_dev_rxrpc: zx_channel_read failed %d\n", status);
        return status;
    }

    const uint32_t index = req_header->device_id;
    if (index >= device_index_.size()) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    const DeviceResources* dr = device_index_[index];

    resp_header->txid = req_header->txid;
    zx_handle_t handle = ZX_HANDLE_INVALID;
    uint32_t handle_count = 0;
    uint32_t resp_len;

    switch (req_header->protocol) {
    case ZX_PROTOCOL_PLATFORM_DEV: {
        auto req = reinterpret_cast<rpc_pdev_req_t*>(&req_buf);
        if (actual < sizeof(*req)) {
            zxlogf(ERROR, "%s received %u, expecting %zu\n", __FUNCTION__, actual, sizeof(*req));
            return ZX_ERR_INTERNAL;
        }
        auto resp = reinterpret_cast<rpc_pdev_rsp_t*>(&resp_buf);
        resp_len = sizeof(*resp);

        switch (req_header->op) {
        case PDEV_GET_MMIO:
            status = RpcGetMmio(dr, req->index, &resp->paddr, &resp->length, &handle,
                                &handle_count);
            break;
        case PDEV_GET_INTERRUPT:
            status = RpcGetInterrupt(dr, req->index, &resp->irq, &resp->mode, &handle,
                                     &handle_count);
            break;
        case PDEV_GET_BTI:
            status = RpcGetBti(dr, req->index, &handle, &handle_count);
            break;
        case PDEV_GET_DEVICE_INFO:
            status = GetDeviceInfo(dr, &resp->device_info);
            break;
        case PDEV_GET_BOARD_INFO:
            status = bus_->GetBoardInfo(&resp->board_info);
            break;
        case PDEV_DEVICE_ADD:
            status = RpcDeviceAdd(dr, req->index, &resp->device_id);
            break;
        case PDEV_GET_PROTOCOLS: {
            auto protos = reinterpret_cast<uint32_t*>(&resp[1]);
            status = RpcGetProtocols(dr, protos, &resp->protocol_count);
            resp_len += static_cast<uint32_t>(resp->protocol_count * sizeof(*protos));
            break;
        }
        default:
            zxlogf(ERROR, "platform_dev_rxrpc: unknown op %u\n", req_header->op);
            return ZX_ERR_INTERNAL;
        }
        break;
    }
    case ZX_PROTOCOL_USB_MODE_SWITCH: {
        auto req = reinterpret_cast<rpc_ums_req_t*>(&req_buf);
        if (actual < sizeof(*req)) {
            zxlogf(ERROR, "%s received %u, expecting %zu\n", __FUNCTION__, actual, sizeof(*req));
            return ZX_ERR_INTERNAL;
        }
        resp_len = sizeof(*resp_header);

        switch (req_header->op) {
            case UMS_SET_MODE:
            status = RpcUmsSetMode(dr, req->usb_mode);
            break;
        default:
            zxlogf(ERROR, "platform_dev_rxrpc: unknown op %u\n", req_header->op);
            return ZX_ERR_INTERNAL;
        }
        break;
    }
    case ZX_PROTOCOL_GPIO: {
        auto req = reinterpret_cast<rpc_gpio_req_t*>(&req_buf);
        if (actual < sizeof(*req)) {
            zxlogf(ERROR, "%s received %u, expecting %zu\n", __FUNCTION__, actual, sizeof(*req));
            return ZX_ERR_INTERNAL;
        }
        auto resp = reinterpret_cast<rpc_gpio_rsp_t*>(&resp_buf);
        resp_len = sizeof(*resp);

        switch (req_header->op) {
        case GPIO_CONFIG:
            status = RpcGpioConfig(dr, req->index, req->flags);
            break;
        case GPIO_SET_ALT_FUNCTION:
            status = RpcGpioSetAltFunction(dr, req->index, req->alt_function);
            break;
        case GPIO_READ:
            status = RpcGpioRead(dr, req->index, &resp->value);
            break;
        case GPIO_WRITE:
            status = RpcGpioWrite(dr, req->index, req->value);
            break;
        case GPIO_GET_INTERRUPT:
            status = RpcGpioGetInterrupt(dr, req->index, req->flags, &handle, &handle_count);
            break;
        case GPIO_RELEASE_INTERRUPT:
            status = RpcGpioReleaseInterrupt(dr, req->index);
            break;
        case GPIO_SET_POLARITY:
            status = RpcGpioSetPolarity(dr, req->index, req->polarity);
            break;
        default:
            zxlogf(ERROR, "platform_dev_rxrpc: unknown op %u\n", req_header->op);
            return ZX_ERR_INTERNAL;
        }
        break;
    }
    case ZX_PROTOCOL_SCPI: {
        auto req = reinterpret_cast<rpc_scpi_req_t*>(&req_buf);
        if (actual < sizeof(*req)) {
            zxlogf(ERROR, "%s received %u, expecting %zu\n", __FUNCTION__, actual, sizeof(*req));
            return ZX_ERR_INTERNAL;
        }
        auto resp = reinterpret_cast<rpc_scpi_rsp_t*>(&resp_buf);
        resp_len = sizeof(*resp);

        switch (req_header->op) {
        case SCPI_GET_SENSOR:
            status = RpcScpiGetSensor(dr, req->name, &resp->sensor_id);
            break;
        case SCPI_GET_SENSOR_VALUE:
            status = RpcScpiGetSensorValue(dr, req->sensor_id, &resp->sensor_value);
            break;
        case SCPI_GET_DVFS_INFO:
            status = RpcScpiGetDvfsInfo(dr, req->power_domain, &resp->opps);
            break;
        case SCPI_GET_DVFS_IDX:
            status = RpcScpiGetDvfsIdx(dr, req->power_domain, &resp->dvfs_idx);
            break;
        case SCPI_SET_DVFS_IDX:
            status = RpcScpiSetDvfsIdx(dr, req->power_domain, req->idx);
            break;
        default:
            zxlogf(ERROR, "platform_dev_rxrpc: unknown op %u\n", req_header->op);
            return ZX_ERR_INTERNAL;
        }
        break;
    }
    case ZX_PROTOCOL_I2C: {
        auto req = reinterpret_cast<rpc_i2c_req_t*>(&req_buf);
        if (actual < sizeof(*req)) {
            zxlogf(ERROR, "%s received %u, expecting %zu\n", __FUNCTION__, actual, sizeof(*req));
            return ZX_ERR_INTERNAL;
        }
        auto resp = reinterpret_cast<rpc_i2c_rsp_t*>(&resp_buf);
        resp_len = sizeof(*resp);

        switch (req_header->op) {
        case I2C_GET_MAX_TRANSFER:
            status = RpcI2cGetMaxTransferSize(dr, req->index, &resp->max_transfer);
            break;
        case I2C_TRANSACT: {
            auto buf = reinterpret_cast<uint8_t*>(&req[1]);
            status = RpcI2cTransact(dr, req_header->txid, req, buf, channel);
            if (status == ZX_OK) {
                // If platform_i2c_transact succeeds, we return immmediately instead of calling
                // zx_channel_write below. Instead we will respond in platform_i2c_complete().
                return ZX_OK;
            }
            break;
        }
        default:
            zxlogf(ERROR, "platform_dev_rxrpc: unknown op %u\n", req_header->op);
            return ZX_ERR_INTERNAL;
        }
        break;
    }
    case ZX_PROTOCOL_CLK: {
        auto req = reinterpret_cast<rpc_clk_req_t*>(&req_buf);
        if (actual < sizeof(*req)) {
            zxlogf(ERROR, "%s received %u, expecting %zu\n", __FUNCTION__, actual, sizeof(*req));
            return ZX_ERR_INTERNAL;
        }
        resp_len = sizeof(*resp_header);

        switch (req_header->op) {
        case CLK_ENABLE:
            status = RpcClkEnable(dr, req->index);
            break;
        case CLK_DISABLE:
            status = RpcClkDisable(dr, req->index);
            break;
        default:
            zxlogf(ERROR, "platform_dev_rxrpc: unknown op %u\n", req_header->op);
            return ZX_ERR_INTERNAL;
        }
        break;
    }
    case ZX_PROTOCOL_CANVAS: {
        auto req = reinterpret_cast<rpc_canvas_req_t*>(&req_buf);
        if (actual < sizeof(*req)) {
            zxlogf(ERROR, "%s received %u, expecting %zu\n", __FUNCTION__, actual, sizeof(*req));
            return ZX_ERR_INTERNAL;
        }
        auto resp = reinterpret_cast<rpc_canvas_rsp_t*>(&resp_buf);
        resp_len = sizeof(*resp);

        switch (req_header->op) {
        case CANVAS_CONFIG:
            status = RpcCanvasConfig(dr, in_handle, req->offset, &req->info,
                                          &resp->idx);
            break;
        case CANVAS_FREE:
            status = RpcCanvasFree(dr, req->idx);
            break;
        default:
            zxlogf(ERROR, "platform_dev_rxrpc: unknown op %u\n", req_header->op);
            return ZX_ERR_INTERNAL;
        }
        break;
    }
    default:
        zxlogf(ERROR, "platform_dev_rxrpc: unknown op %u\n", req_header->op);
        return ZX_ERR_INTERNAL;
    }

    // set op to match request so zx_channel_write will return our response
    resp_header->status = status;
    status = zx_channel_write(channel, 0, resp_header, resp_len,
                              (handle_count == 1 ? &handle : nullptr), handle_count);
    if (status != ZX_OK) {
        zxlogf(ERROR, "platform_dev_rxrpc: zx_channel_write failed %d\n", status);
    }
    return status;
}

zx_status_t PlatformDevice::DdkGetProtocol(uint32_t proto_id, void* out) {
    if (proto_id == ZX_PROTOCOL_PLATFORM_DEV) {
        auto proto = static_cast<platform_device_protocol_t*>(out);
        proto->ops = &pdev_proto_ops_;
        proto->ctx = this;
        return ZX_OK;
    } else {
        return bus_->DdkGetProtocol(proto_id, out);
    }
}

void PlatformDevice::DdkRelease() {
    delete this;
}

zx_status_t PlatformDevice::AddMetaData(const pbus_metadata_t& pbm) {
    const uint32_t type = pbm.type;
    const uint32_t extra = pbm.extra;
    const uint8_t* metadata = bus_->metadata();
    const size_t metadata_size = bus_->metadata_size();
    size_t offset = 0;

    while (offset < metadata_size) {
        auto header = reinterpret_cast<const zbi_header_t*>(metadata);
        size_t length = ZBI_ALIGN(sizeof(zbi_header_t) + header->length);

        if (header->type == type && header->extra == extra) {
            return DdkAddMetadata(type, header + 1, length - sizeof(zbi_header_t));
        }
        metadata += length;
        offset += length;
    }
    zxlogf(ERROR, "%s metadata not found for type %08x, extra %u\n", __FUNCTION__, type, extra);
    return ZX_ERR_NOT_FOUND;
}

zx_status_t PlatformDevice::Start() {
    zx_device_prop_t props[] = {
        {BIND_PLATFORM_DEV_VID, 0, vid_},
        {BIND_PLATFORM_DEV_PID, 0, pid_},
        {BIND_PLATFORM_DEV_DID, 0, did_},
    };

    char namestr[ZX_DEVICE_NAME_MAX];
    if (vid_ == PDEV_VID_GENERIC && pid_ == PDEV_PID_GENERIC && did_ == PDEV_DID_KPCI) {
        strlcpy(namestr, "pci", sizeof(namestr));
    } else {
        snprintf(namestr, sizeof(namestr), "%02x:%02x:%01x", vid_, pid_, did_);
    }
    char argstr[64];
    snprintf(argstr, sizeof(argstr), "pdev:%s,", namestr);
    uint32_t flags = 0;
    if (!(flags_ & PDEV_ADD_PBUS_DEVHOST)) {
        flags |= DEVICE_ADD_MUST_ISOLATE;
    }

// FIXME do this for child devices somehow
    const DeviceResources* dr = device_index_[ROOT_DEVICE_ID];
    size_t metadata_count = dr->metadata_count();
    if (metadata_count > 0) {
        // Keep device invisible until after we add its metadata.
        flags |= DEVICE_ADD_INVISIBLE;
    }

    auto status = DdkAdd(namestr, flags, props, countof(props), argstr);
    if (status != ZX_OK) {
        return status;
    }

    if (metadata_count > 0) {
        for (size_t i = 0; i < metadata_count; i++) {
            const pbus_metadata_t& pbm = dr->metadata(i);
            if (pbm.data && pbm.len) {
                status = DdkAddMetadata(pbm.type, pbm.data, pbm.len);
            } else {
                status = AddMetaData(pbm);
            }
            if (status != ZX_OK) {
                DdkRemove();
                return status;
            }
        }
        DdkMakeVisible();
    }

    return ZX_OK;
}

} // namespace platform_bus
