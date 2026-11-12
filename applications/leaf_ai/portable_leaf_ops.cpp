#include <optional>
#include <stdio.h>
#include <tuple>

#include <executorch/runtime/core/array_ref.h>
#include <executorch/runtime/core/evalue.h>
#include <executorch/runtime/core/exec_aten/exec_aten.h>
#include <executorch/runtime/kernel/kernel_runtime_context.h>
#include <executorch/runtime/kernel/operator_registry.h>

namespace torch {
namespace executor {
namespace native {

void convolution_out(
    executorch::runtime::KernelRuntimeContext& context,
    const executorch::aten::Tensor& input,
    const executorch::aten::Tensor& weight,
    const std::optional<executorch::aten::Tensor>& bias,
    executorch::aten::ArrayRef<int64_t> stride,
    executorch::aten::ArrayRef<int64_t> padding,
    executorch::aten::ArrayRef<int64_t> dilation,
    bool transposed,
    executorch::aten::ArrayRef<int64_t> output_padding,
    int64_t groups,
    executorch::aten::Tensor& output);

std::tuple<
    executorch::aten::Tensor&,
    executorch::aten::Tensor&,
    executorch::aten::Tensor&>
_native_batch_norm_legit_no_training_out(
    executorch::runtime::KernelRuntimeContext& context,
    const executorch::aten::Tensor& input,
    const std::optional<executorch::aten::Tensor>& weight,
    const std::optional<executorch::aten::Tensor>& bias,
    const executorch::aten::Tensor& running_mean,
    const executorch::aten::Tensor& running_var,
    double momentum,
    double eps,
    executorch::aten::Tensor& output,
    executorch::aten::Tensor& save_mean,
    executorch::aten::Tensor& save_invstd);

void hardtanh_out(
    executorch::runtime::KernelRuntimeContext& context,
    const executorch::aten::Tensor& input,
    const executorch::aten::Scalar& min,
    const executorch::aten::Scalar& max,
    executorch::aten::Tensor& output);

void add_out(
    executorch::runtime::KernelRuntimeContext& context,
    const executorch::aten::Tensor& input,
    const executorch::aten::Tensor& other,
    const executorch::aten::Scalar& alpha,
    executorch::aten::Tensor& output);

void mean_dim_out(
    executorch::runtime::KernelRuntimeContext& context,
    const executorch::aten::Tensor& input,
    std::optional<executorch::aten::ArrayRef<int64_t>> dim,
    bool keepdim,
    std::optional<executorch::aten::ScalarType> dtype,
    executorch::aten::Tensor& output);

void _clone_dim_order_out(
    executorch::runtime::KernelRuntimeContext& context,
    const executorch::aten::Tensor& input,
    bool non_blocking,
    std::optional<executorch::aten::ArrayRef<int64_t>> memory_format,
    executorch::aten::Tensor& output);

void permute_copy_out(
    executorch::runtime::KernelRuntimeContext& context,
    const executorch::aten::Tensor& input,
    executorch::aten::ArrayRef<int64_t> dims,
    executorch::aten::Tensor& output);

void addmm_out(
    executorch::runtime::KernelRuntimeContext& context,
    const executorch::aten::Tensor& self,
    const executorch::aten::Tensor& mat1,
    const executorch::aten::Tensor& mat2,
    const executorch::aten::Scalar& beta,
    const executorch::aten::Scalar& alpha,
    executorch::aten::Tensor& output);

} // namespace native
} // namespace executor
} // namespace torch

namespace {

using executorch::runtime::EValue;
using executorch::runtime::Kernel;
using executorch::runtime::KernelRuntimeContext;
using executorch::runtime::Span;

void convolution_out_wrapper(
    KernelRuntimeContext& context, Span<EValue*> args) {
  const auto& input = args[0]->toTensor();
  const auto& weight = args[1]->toTensor();
  auto bias = args[2]->toOptional<executorch::aten::Tensor>();
  auto stride = args[3]->toIntList();
  auto padding = args[4]->toIntList();
  auto dilation = args[5]->toIntList();
  bool transposed = args[6]->toBool();
  auto output_padding = args[7]->toIntList();
  int64_t groups = args[8]->toInt();
  auto& output = args[9]->toTensor();

  torch::executor::native::convolution_out(
      context, input, weight, bias, stride, padding, dilation, transposed,
      output_padding, groups, output);
}

void batch_norm_out_wrapper(
    KernelRuntimeContext& context, Span<EValue*> args) {
  const auto& input = args[0]->toTensor();
  auto weight = args[1]->toOptional<executorch::aten::Tensor>();
  auto bias = args[2]->toOptional<executorch::aten::Tensor>();
  const auto& running_mean = args[3]->toTensor();
  const auto& running_var = args[4]->toTensor();
  double momentum = args[5]->toDouble();
  double eps = args[6]->toDouble();
  auto& output = args[7]->toTensor();
  auto& save_mean = args[8]->toTensor();
  auto& save_invstd = args[9]->toTensor();

  torch::executor::native::_native_batch_norm_legit_no_training_out(
      context, input, weight, bias, running_mean, running_var, momentum, eps,
      output, save_mean, save_invstd);
}

void hardtanh_out_wrapper(
    KernelRuntimeContext& context, Span<EValue*> args) {
  const auto& input = args[0]->toTensor();
  const auto& min = args[1]->toScalar();
  const auto& max = args[2]->toScalar();
  auto& output = args[3]->toTensor();

  torch::executor::native::hardtanh_out(context, input, min, max, output);
}

void add_out_wrapper(
    KernelRuntimeContext& context, Span<EValue*> args) {
  const auto& input = args[0]->toTensor();
  const auto& other = args[1]->toTensor();
  const auto& alpha = args[2]->toScalar();
  auto& output = args[3]->toTensor();

  torch::executor::native::add_out(
      context, input, other, alpha, output);
}

void mean_out_wrapper(
    KernelRuntimeContext& context, Span<EValue*> args) {
  const auto& input = args[0]->toTensor();
  auto dim = args[1]->toOptional<executorch::aten::ArrayRef<int64_t>>();
  bool keepdim = args[2]->toBool();
  auto dtype = args[3]->toOptional<executorch::aten::ScalarType>();
  auto& output = args[4]->toTensor();

  torch::executor::native::mean_dim_out(
      context, input, dim, keepdim, dtype, output);
}

void clone_dim_order_out_wrapper(
    KernelRuntimeContext& context, Span<EValue*> args) {
  const auto& input = args[0]->toTensor();
  bool non_blocking = args[1]->toBool();
  auto memory_format =
      args[2]->toOptional<executorch::aten::ArrayRef<int64_t>>();
  auto& output = args[3]->toTensor();

  torch::executor::native::_clone_dim_order_out(
      context, input, non_blocking, memory_format, output);
}

void permute_copy_out_wrapper(
    KernelRuntimeContext& context, Span<EValue*> args) {
  const auto& input = args[0]->toTensor();
  auto dims = args[1]->toIntList();
  auto& output = args[2]->toTensor();

  torch::executor::native::permute_copy_out(context, input, dims, output);
}

void addmm_out_wrapper(
    KernelRuntimeContext& context, Span<EValue*> args) {
  const auto& self = args[0]->toTensor();
  const auto& mat1 = args[1]->toTensor();
  const auto& mat2 = args[2]->toTensor();
  const auto& beta = args[3]->toScalar();
  const auto& alpha = args[4]->toScalar();
  auto& output = args[5]->toTensor();

  torch::executor::native::addmm_out(
      context, self, mat1, mat2, beta, alpha, output);
}

const auto convolution_registration =
    executorch::runtime::register_kernel(
        Kernel("aten::convolution.out", &convolution_out_wrapper));
const auto batch_norm_registration =
    executorch::runtime::register_kernel(
        Kernel(
            "aten::_native_batch_norm_legit_no_training.out",
            &batch_norm_out_wrapper));
const auto hardtanh_registration =
    executorch::runtime::register_kernel(
        Kernel("aten::hardtanh.out", &hardtanh_out_wrapper));
const auto add_registration =
    executorch::runtime::register_kernel(
        Kernel("aten::add.out", &add_out_wrapper));
const auto mean_registration =
    executorch::runtime::register_kernel(
        Kernel("aten::mean.out", &mean_out_wrapper));
const auto clone_dim_order_registration =
    executorch::runtime::register_kernel(
        Kernel("dim_order_ops::_clone_dim_order.out",
               &clone_dim_order_out_wrapper));
const auto permute_copy_registration =
    executorch::runtime::register_kernel(
        Kernel("aten::permute_copy.out", &permute_copy_out_wrapper));
const auto addmm_registration =
    executorch::runtime::register_kernel(
        Kernel("aten::addmm.out", &addmm_out_wrapper));

} // namespace

