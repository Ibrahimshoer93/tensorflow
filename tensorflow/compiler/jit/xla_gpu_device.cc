/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// Registers the XLA_GPU device, which is an XlaDevice instantiation that runs
// operators using XLA via the XLA "CUDA" (GPU) backend.

#include "absl/memory/memory.h"
#include "tensorflow/compiler/jit/kernels/xla_ops.h"
#include "tensorflow/compiler/jit/xla_device.h"
#include "tensorflow/compiler/jit/xla_device_ops.h"
#include "tensorflow/compiler/tf2xla/xla_op_registry.h"
#include "tensorflow/core/common_runtime/device_factory.h"
#include "tensorflow/core/lib/core/status.h"

namespace tensorflow {

class XlaGpuDeviceFactory : public DeviceFactory {
 public:
  Status CreateDevices(const SessionOptions& options, const string& name_prefix,
                       std::vector<Device*>* devices) override;
};

Status XlaGpuDeviceFactory::CreateDevices(const SessionOptions& session_options,
                                          const string& name_prefix,
                                          std::vector<Device*>* devices) {
  XlaOpRegistry::DeviceRegistration registration;
  registration.compilation_device_name = DEVICE_GPU_XLA_JIT;
  registration.requires_compilation = true;
  registration.enable_jit_by_default = false;
  registration.compile_resource_ops = true;
  XlaOpRegistry::RegisterCompilationDevice(DEVICE_XLA_GPU, registration);

  static XlaDeviceOpRegistrations* registrations =
      RegisterXlaDeviceKernels(DEVICE_XLA_GPU, DEVICE_GPU_XLA_JIT);
  (void)registrations;

  auto platform = se::MultiPlatformManager::PlatformWithName("CUDA");
  if (!platform.ok()) {
    // Treat failures as non-fatal; there might not be a GPU in the machine.
    VLOG(1) << "Failed to create XLA_GPU device: " << platform.status();
    return Status::OK();
  }

  XlaDevice::Options options;
  options.platform = platform.ValueOrDie();
  options.device_name_prefix = name_prefix;
  options.device_name = DEVICE_XLA_GPU;
  options.device_ordinal = 0;
  options.compilation_device_name = DEVICE_GPU_XLA_JIT;
  options.use_multiple_streams = false;
  auto device = absl::make_unique<XlaDevice>(session_options, options);

  // TODO(b/78468222): Uncomment after fixing this bug
  // status = device->UseGpuDeviceInfo();
  // if (!status.ok()) {
  //  errors::AppendToMessage(&status, "while setting up ", DEVICE_GPU_XLA_JIT,
  //                          " device");
  //  return status;
  // }

  devices->push_back(device.release());
  return Status::OK();
}

REGISTER_LOCAL_DEVICE_FACTORY(DEVICE_XLA_GPU, XlaGpuDeviceFactory);

// Kernel registrations

constexpr std::array<DataType, 13> kAllXlaGpuTypes = {
    {DT_UINT8, DT_QUINT8, DT_INT8, DT_QINT8, DT_INT32, DT_QINT32, DT_INT64,
     DT_HALF, DT_FLOAT, DT_DOUBLE, DT_COMPLEX64, DT_BOOL, DT_BFLOAT16}};

REGISTER_XLA_LAUNCH_KERNEL(DEVICE_XLA_GPU, XlaLocalLaunchOp, kAllXlaGpuTypes);
REGISTER_XLA_COMPILE_KERNEL(DEVICE_XLA_GPU, XlaCompileOp, kAllXlaGpuTypes);
REGISTER_XLA_RUN_KERNEL(DEVICE_XLA_GPU, XlaRunOp, kAllXlaGpuTypes);

REGISTER_XLA_DEVICE_KERNELS(DEVICE_XLA_GPU, kAllXlaGpuTypes);

}  // namespace tensorflow
